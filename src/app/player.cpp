#include "app/player.hpp"

#ifdef _WIN32
#include <windows.h>
// <shellapi.h> must follow <windows.h>.
#include <shellapi.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace pfdb::app {

#ifdef _WIN32

bool open_in_default_app(const std::string& path) {
    const int wlen =
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wlen <= 0) {
        return false;
    }
    std::wstring wide(static_cast<std::size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide.data(), wlen);
    // ShellExecute returns a value > 32 on success.
    const HINSTANCE rc =
        ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(rc) > 32;
}

#else

bool open_in_default_app(const std::string& path) {
#ifdef __APPLE__
    const char* opener = "open";
#else
    const char* opener = "xdg-open";
#endif
    const pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        execlp(opener, opener, path.c_str(), static_cast<char*>(nullptr));
        _exit(127);  // exec failed
    }
    return true;  // launch initiated; we don't wait on the player
}

#endif

}  // namespace pfdb::app
