#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <optional>
#include <utility>

namespace pfdb::gui {

/// A single background job producing a `T`. The work runs on its own thread
/// (via std::async); the UI polls `take()` each frame and applies the result on
/// the UI thread. The job function should do only thread-safe work (network,
/// parsing) and return plain data — never touch the Repository/model directly.
template <typename T>
class Job {
public:
    void start(std::function<T()> fn) {
        future_ = std::async(std::launch::async, std::move(fn));
        running_ = true;
    }

    bool running() const { return running_; }

    /// If the job has finished, return its result and clear the running flag;
    /// otherwise nullopt. The job function is expected to have caught its own
    /// errors and encoded them in `T` (so `get()` never throws here).
    std::optional<T> take() {
        if (!running_) {
            return std::nullopt;
        }
        if (future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return std::nullopt;
        }
        running_ = false;
        return future_.get();
    }

private:
    std::future<T> future_;
    bool running_ = false;
};

}  // namespace pfdb::gui
