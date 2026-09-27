#include <ctime>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "app/platform.hpp"
#include "app/update.hpp"
#include "app/version.hpp"
#include "gui/app.hpp"
#include "net/http_client.hpp"

namespace pfdb::gui {
namespace {

std::string install_dir() {
    const std::string exe = app::current_executable_path();
    if (exe.empty()) {
        return ".";
    }
    return std::filesystem::path(exe).parent_path().string();
}

}  // namespace

void App::start_update_check() {
    if (update_check_job_.running() || app_update_.applying) {
        return;
    }
    app_update_.status = "Checking for updates...";
    const std::string repo = config_.update().repo;
    const app::SemVer current = app::current_version();
    update_check_job_.start([repo, current]() -> UpdateCheckOutcome {
        net::CprHttpClient http(10000);
        UpdateCheckOutcome o;
        std::optional<app::UpdateManifest> m = app::fetch_manifest(http, repo);
        if (!m) {
            o.error = "Could not reach the update server.";
            return o;
        }
        if (app::is_newer(*m, current)) {
            o.available = true;
            o.version = m->version_str;
            o.notes = m->notes;
        }
        return o;
    });
}

void App::start_update_apply() {
    if (update_apply_job_.running()) {
        return;
    }
    app_update_.applying = true;
    app_update_.status = "Downloading update...";
    const std::string repo = config_.update().repo;
    const app::SemVer current = app::current_version();
    const std::string staging = app::default_staging_dir(install_dir());
    update_apply_job_.start([repo, current, staging]() -> UpdateApplyOutcome {
        net::CprHttpClient http(120000);  // generous for the download
        UpdateApplyOutcome o;
        app::PrepareResult res =
            app::prepare_update(http, repo, current, staging, /*download=*/true);
        if (!res.error.empty()) {
            o.error = res.error;
            return o;
        }
        if (!res.prepared) {
            o.error = "The update could not be prepared.";
            return o;
        }
        o.ok = true;
        o.prepared = *res.prepared;
        return o;
    });
}

void App::poll_update_jobs() {
    if (auto out = update_check_job_.take()) {
        app_update_.status.clear();
        if (!out->error.empty()) {
            app_update_.status = out->error;
        } else if (out->available) {
            app_update_.available = true;
            app_update_.version = out->version;
            app_update_.notes = out->notes;
            app_update_.show_available = true;
        } else {
            app_update_.status = "PFDB is up to date.";
        }
        config_.mark_update_checked(static_cast<std::int64_t>(std::time(nullptr)));
        try {
            config_.save();
        } catch (const std::exception&) {
            // ignore a read-only config
        }
    }

    if (auto out = update_apply_job_.take()) {
        app_update_.applying = false;
        if (!out->error.empty()) {
            app_update_.status = out->error;
        } else {
            // Hand off to the swapper, then quit so it can replace our files.
            if (app::launch_swapper(out->prepared, install_dir(), config_.path(),
                                    app::Relaunch::Gui)) {
                quit_ = true;
            } else {
                app_update_.status = "Could not launch the update installer.";
            }
        }
    }
}

void App::maybe_auto_check() {
    if (auto_checked_) {
        return;
    }
    auto_checked_ = true;  // once per session
    if (config_.update_check_due(static_cast<std::int64_t>(std::time(nullptr)))) {
        start_update_check();
    }
}

void App::draw_update_modals() {
    // "Update available" modal.
    ensure_open("Update available", app_update_.show_available);
    if (ImGui::BeginPopupModal("Update available", &app_update_.show_available,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Version %s is available (you have %s).",
                    app_update_.version.c_str(), app::current_version().str().c_str());
        if (!app_update_.notes.empty()) {
            ImGui::Separator();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0F);
            ImGui::TextWrapped("%s", app_update_.notes.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::Separator();
        if (app_update_.applying) {
            ImGui::TextDisabled("Downloading and verifying...");
        } else {
            if (ImGui::Button("Update now")) {
                start_update_apply();
            }
            ImGui::SameLine();
            if (ImGui::Button("Later")) {
                app_update_.show_available = false;
            }
        }
        if (!app_update_.status.empty()) {
            ImGui::TextColored(ImVec4(0.9F, 0.6F, 0.3F, 1.0F), "%s",
                               app_update_.status.c_str());
        }
        ImGui::EndPopup();
    }

    // Post-update result modal (shown once on first run after a swap).
    ensure_open("Update result", app_update_.show_result);
    if (ImGui::BeginPopupModal("Update result", &app_update_.show_result,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        if (app_update_.result_ok) {
            ImGui::TextColored(ImVec4(0.4F, 0.8F, 0.4F, 1.0F), "Update successful");
        } else {
            ImGui::TextColored(ImVec4(0.9F, 0.4F, 0.4F, 1.0F), "Update failed");
        }
        ImGui::Separator();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0F);
        ImGui::TextWrapped("%s", app_update_.result_text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Separator();
        if (ImGui::Button("OK")) {
            app_update_.show_result = false;
        }
        ImGui::EndPopup();
    }
}

}  // namespace pfdb::gui
