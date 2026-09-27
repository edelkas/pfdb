#pragma once

#include <string>
#include <vector>

namespace pfdb::app {

/// Absolute path of the currently running executable. Empty on failure.
std::string current_executable_path();

/// The id of the current process (used so a spawned swapper can wait for us).
long current_process_id();

/// Run `exe` with `args`, capturing its combined stdout into `out`. Returns the
/// child's exit code, or -1 if the process could not be started. Used for the
/// pre-swap `--version` sanity check on a freshly downloaded binary.
int run_and_capture(const std::string& exe, const std::vector<std::string>& args,
                    std::string& out);

/// Launch `exe` with `args` as an independent process and return immediately.
/// The caller is expected to exit shortly after (the "re-exec in place" step).
/// Returns false if the process could not be started.
bool launch_detached(const std::string& exe, const std::vector<std::string>& args);

/// Block until the process with id `pid` has exited (polling on POSIX). Returns
/// immediately if it is already gone. `timeout_ms < 0` waits indefinitely.
void wait_for_pid(long pid, int timeout_ms = -1);

}  // namespace pfdb::app
