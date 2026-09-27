#include "app/platform.hpp"

#include <array>
#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <ctime>
#endif

namespace pfdb::app {

#ifdef _WIN32
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                      static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                      static_cast<int>(w.size()), nullptr, 0,
                                      nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(),
                        n, nullptr, nullptr);
    return s;
}

/// Quote a single argument per the Windows command-line (CommandLineToArgvW)
/// rules so paths with spaces survive the round-trip.
std::wstring quote_arg(const std::string& arg) {
    const std::wstring w = widen(arg);
    if (!w.empty() && w.find_first_of(L" \t\"") == std::wstring::npos) {
        return w;
    }
    std::wstring out = L"\"";
    std::size_t backslashes = 0;
    for (wchar_t c : w) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
        } else {
            out.append(backslashes, L'\\');
            out.push_back(c);
        }
        backslashes = 0;
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}

std::wstring build_command_line(const std::string& exe,
                                const std::vector<std::string>& args) {
    std::wstring cmd = quote_arg(exe);
    for (const auto& a : args) {
        cmd.push_back(L' ');
        cmd += quote_arg(a);
    }
    return cmd;
}

}  // namespace

std::string current_executable_path() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n =
            GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) {
            return {};
        }
        if (n < buf.size()) {
            buf.resize(n);
            return narrow(buf);
        }
        buf.resize(buf.size() * 2);
    }
}

long current_process_id() { return static_cast<long>(GetCurrentProcessId()); }

int run_and_capture(const std::string& exe, const std::vector<std::string>& args,
                    std::string& out) {
    out.clear();

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_h = nullptr;
    HANDLE write_h = nullptr;
    if (CreatePipe(&read_h, &write_h, &sa, 0) == 0) {
        return -1;
    }
    SetHandleInformation(read_h, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_h;
    si.hStdError = write_h;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::wstring cmd = build_command_line(exe, args);

    const BOOL ok =
        CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &si, &pi);
    CloseHandle(write_h);  // parent's copy; child keeps its own
    if (ok == 0) {
        CloseHandle(read_h);
        return -1;
    }

    std::array<char, 4096> chunk{};
    DWORD got = 0;
    while (ReadFile(read_h, chunk.data(), static_cast<DWORD>(chunk.size()), &got,
                    nullptr) != 0 &&
           got > 0) {
        out.append(chunk.data(), got);
    }
    CloseHandle(read_h);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(code);
}

bool launch_detached(const std::string& exe, const std::vector<std::string>& args) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cmd = build_command_line(exe, args);
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0,
                                   nullptr, nullptr, &si, &pi);
    if (ok == 0) {
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

void wait_for_pid(long pid, int timeout_ms) {
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (h == nullptr) {
        return;  // already gone (or no access)
    }
    WaitForSingleObject(h, timeout_ms < 0 ? INFINITE : static_cast<DWORD>(timeout_ms));
    CloseHandle(h);
}

#else  // ---------------------------------------------------------------- POSIX

std::string current_executable_path() {
#if defined(__linux__)
    std::array<char, 4096> buf{};
    const ssize_t n = readlink("/proc/self/exe", buf.data(), buf.size() - 1);
    if (n <= 0) {
        return {};
    }
    return std::string(buf.data(), static_cast<std::size_t>(n));
#else
    return {};  // best-effort on other POSIX; extend as needed
#endif
}

long current_process_id() { return static_cast<long>(getpid()); }

int run_and_capture(const std::string& exe, const std::vector<std::string>& args,
                    std::string& out) {
    out.clear();
    int fds[2];
    if (pipe(fds) != 0) {
        return -1;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (const auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);
        execv(exe.c_str(), argv.data());
        _exit(127);
    }
    close(fds[1]);
    std::array<char, 4096> chunk{};
    ssize_t got = 0;
    while ((got = read(fds[0], chunk.data(), chunk.size())) > 0) {
        out.append(chunk.data(), static_cast<std::size_t>(got));
    }
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool launch_detached(const std::string& exe, const std::vector<std::string>& args) {
    const pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        for (const auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);
        execv(exe.c_str(), argv.data());
        _exit(127);
    }
    return true;
}

void wait_for_pid(long pid, int timeout_ms) {
    const int step_ms = 100;
    int waited = 0;
    while (kill(static_cast<pid_t>(pid), 0) == 0) {
        timespec ts{0, step_ms * 1000000L};
        nanosleep(&ts, nullptr);
        waited += step_ms;
        if (timeout_ms >= 0 && waited >= timeout_ms) {
            return;
        }
    }
}

#endif

}  // namespace pfdb::app
