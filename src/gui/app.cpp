#include "gui/app.hpp"

#include <ctime>
#include <filesystem>
#include <string>
#include <utility>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <GLFW/glfw3.h>

#include "app/platform.hpp"
#include "app/version.hpp"
#include "query/parser.hpp"  // query::QueryError

namespace pfdb::gui {
namespace {

// One line describing a changed field, or "" if unchanged.
template <typename T>
std::string diff_line(const char* label, const T& before, const T& after) {
    if (before == after) {
        return {};
    }
    return std::string(label) + " changed";
}

std::string diff_scalar(const char* label, const std::string& before,
                        const std::string& after) {
    if (before == after) {
        return {};
    }
    return std::string(label) + ": \"" + before + "\" \xe2\x86\x92 \"" + after + "\"";
}

std::string opt_year(const std::optional<int>& v) {
    return v ? std::to_string(*v) : std::string("-");
}

std::string opt_money(const std::optional<std::int64_t>& v) {
    return v ? std::to_string(*v) : std::string("-");
}

// Compare the *sourced* fields of an existing film and a freshly fetched one.
std::vector<std::string> compute_diff(const Film& before, const Film& after) {
    std::vector<std::string> out;
    auto push = [&](std::string s) {
        if (!s.empty()) {
            out.push_back(std::move(s));
        }
    };
    push(diff_scalar("Title", before.title, after.title));
    push(diff_scalar("Original title", before.original_title, after.original_title));
    push(diff_scalar("Year", opt_year(before.year), opt_year(after.year)));
    push(diff_scalar("Runtime", opt_year(before.runtime_minutes),
                     opt_year(after.runtime_minutes)));
    push(diff_scalar("Budget", opt_money(before.budget), opt_money(after.budget)));
    push(diff_scalar("Gross", opt_money(before.gross), opt_money(after.gross)));
    if (before.synopsis != after.synopsis) {
        push("Synopsis changed");
    }
    push(diff_line("Genres", before.genres, after.genres));
    push(diff_line("Credits", before.credits, after.credits));
    push(diff_line("Ratings", before.ratings, after.ratings));
    push(diff_line("Topics", before.topics, after.topics));
    push(diff_line("Groups", before.groups, after.groups));
    if (out.empty()) {
        out.emplace_back("No changes.");
    }
    return out;
}

}  // namespace

void ensure_open(const char* name, bool flag) {
    if (flag && !ImGui::IsPopupOpen(name)) {
        ImGui::OpenPopup(name);
    }
}

std::string today_iso() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1,
                  tm.tm_mday);
    return buf;
}

App::App(std::string db_path, std::string config_path)
    : repo_(db_path),
      model_(CollectionModel::load(repo_)),
      config_(config::Config::load(config_path)) {
    apply_theme();
    refresh();

    // Finalize a just-applied update: surface its notes/error once, clean up.
    const std::string exe = app::current_executable_path();
    const std::string dir =
        exe.empty() ? "." : std::filesystem::path(exe).parent_path().string();
    const app::FinalizeResult fin = app::finalize_update(config_, dir);
    if (!fin.error.empty()) {
        app_update_.show_result = true;
        app_update_.result_ok = false;
        app_update_.result_text = fin.error;
    } else if (fin.updated) {
        app_update_.show_result = true;
        app_update_.result_ok = true;
        app_update_.result_text = "Updated to " + app::current_version().str() +
                                  (fin.notes.empty() ? "" : "\n\n" + fin.notes);
    }
}

void App::apply_theme() {
    if (dark_) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
    }
}

void App::reload_model() {
    model_ = CollectionModel::load(repo_);
    refresh();
}

query::QueryRequest App::build_request() const {
    query::QueryRequest r;
    for (const auto& f : filters_) {
        if (f.field.empty() || f.value.empty()) {
            continue;
        }
        r.filter_specs.push_back(f.field + " " + f.op + " " + f.value);
    }
    std::string where = where_;
    if (!quick_title_.empty()) {
        r.filter_specs.push_back("title ~ " + quick_title_);
        if (!where.empty()) {
            where = "(" + where + ") AND F" + std::to_string(r.filter_specs.size());
        }
    }
    r.where = where;
    r.sort = sort_;
    return r;
}

void App::refresh() {
    try {
        results_ = query::run_query(model_, build_request());
        query_error_.clear();
    } catch (const query::QueryError& e) {
        query_error_ = e.what();
    }
}

const Film* App::selected_film() const { return model_.find(selected_); }

void App::apply_filter(const std::string& field, const std::string& op,
                       const std::string& value) {
    filters_.push_back({field, op, value});
    if (!where_.empty()) {
        where_ = "(" + where_ + ") AND F" + std::to_string(filters_.size());
    }
    refresh();
    toast_ = "Filter added: " + field + " " + op + " " + value;
}

void App::store_edges(Id film_id, const std::vector<sources::RelatedRef>& relations,
                      const std::vector<sources::SimilarRef>& similars) {
    std::vector<db::Repository::RelationEdge> rel;
    for (const auto& r : relations) {
        if (auto other = repo_.find_id_by_source_ref("filmaffinity", r.external_id)) {
            rel.push_back({*other, r.kind});
        }
    }
    repo_.replace_relations(film_id, rel);

    std::vector<db::Repository::SimilarityEdge> sim;
    for (const auto& s : similars) {
        if (auto other = repo_.find_id_by_source_ref("filmaffinity", s.external_id)) {
            sim.push_back({*other, s.percent});
        }
    }
    repo_.replace_similarities(film_id, sim);
}

void App::poll_jobs() {
    if (auto out = search_job_.take()) {
        if (!out->error.empty()) {
            add_.status = out->error;
        } else {
            add_.sources = std::move(out->sources);
            add_.selected.assign(add_.sources.size(), -1);
            add_.status.clear();
        }
    }
    if (auto out = add_job_.take()) {
        if (!out->error.empty()) {
            add_.status = out->error;
        } else {
            const Id id = repo_.insert(out->film);
            store_edges(id, out->relations, out->similars);
            if (!out->cover_bytes.empty()) {
                repo_.set_cover(id, out->cover_mime, out->cover_bytes);
            }
            reload_model();
            selected_ = id;
            add_.open = false;
            toast_ = "Added \"" + out->film.title + "\"";
        }
    }
    if (auto out = update_job_.take()) {
        if (!out->error.empty()) {
            toast_ = out->error;
        } else {
            if (const Film* existing = model_.find(out->film_id)) {
                diff_.changes = compute_diff(*existing, out->film);
            }
            diff_.film_id = out->film_id;
            diff_.refreshed = std::move(out->film);
            diff_.relations = std::move(out->relations);
            diff_.similars = std::move(out->similars);
            diff_.open = true;
        }
    }
    poll_update_jobs();
}

void App::draw_toolbar() {
    if (ImGui::Button("Add movie")) {
        add_ = AddState{};
        add_.open = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Settings")) {
        show_settings_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("About")) {
        show_about_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(layout_ == Layout::Vertical ? "Layout: stacked"
                                                  : "Layout: side-by-side")) {
        layout_ = layout_ == Layout::Vertical ? Layout::Horizontal : Layout::Vertical;
    }
    ImGui::SameLine();
    if (ImGui::Button(dark_ ? "Theme: dark" : "Theme: light")) {
        dark_ = !dark_;
        apply_theme();
    }
    ImGui::SameLine();
    const bool checking = update_check_job_.running() || app_update_.applying;
    ImGui::BeginDisabled(checking);
    if (ImGui::Button(app_update_.available ? "Update available!" : "Check for updates")) {
        if (app_update_.available) {
            app_update_.show_available = true;
        } else {
            start_update_check();
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu films", results_.size(), model_.size());
    if (!app_update_.status.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("  %s", app_update_.status.c_str());
    }
    if (!toast_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4F, 0.8F, 0.4F, 1.0F), "  %s", toast_.c_str());
    }
}

void App::draw_panels() {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float splitter = 6.0F;

    if (layout_ == Layout::Vertical) {
        float top_h = (avail.y - splitter) * split_ratio_;
        ImGui::BeginChild("##table", ImVec2(0, top_h), true);
        draw_table();
        ImGui::EndChild();

        ImGui::InvisibleButton("##hsplit", ImVec2(-1, splitter));
        if (ImGui::IsItemActive() && avail.y > 0) {
            split_ratio_ += ImGui::GetIO().MouseDelta.y / avail.y;
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }

        ImGui::BeginChild("##detail", ImVec2(0, 0), true);
        draw_detail();
        ImGui::EndChild();
    } else {
        float left_w = (avail.x - splitter) * split_ratio_;
        ImGui::BeginChild("##table", ImVec2(left_w, 0), true);
        draw_table();
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::InvisibleButton("##vsplit", ImVec2(splitter, -1));
        if (ImGui::IsItemActive() && avail.x > 0) {
            split_ratio_ += ImGui::GetIO().MouseDelta.x / avail.x;
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        ImGui::SameLine();

        ImGui::BeginChild("##detail", ImVec2(0, 0), true);
        draw_detail();
        ImGui::EndChild();
    }
    if (split_ratio_ < 0.15F) {
        split_ratio_ = 0.15F;
    }
    if (split_ratio_ > 0.85F) {
        split_ratio_ = 0.85F;
    }
}

void App::draw_settings_modal() {
    ensure_open("Settings", show_settings_);
    if (ImGui::BeginPopupModal("Settings", &show_settings_,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Schema version: %d", repo_.schema_version());
        ImGui::Text("Films: %zu", model_.size());
        ImGui::Separator();
        if (ImGui::Checkbox("Dark theme", &dark_)) {
            apply_theme();
        }
        bool horizontal = layout_ == Layout::Horizontal;
        if (ImGui::Checkbox("Side-by-side layout", &horizontal)) {
            layout_ = horizontal ? Layout::Horizontal : Layout::Vertical;
        }
        ImGui::Separator();
        ImGui::TextDisabled("Updates");
        int interval = config_.update().check_interval_hours;
        if (ImGui::InputInt("Check every (hours, 0 = off)", &interval)) {
            if (interval < 0) {
                interval = 0;
            }
            config_.update().check_interval_hours = interval;
            try {
                config_.save();
            } catch (const std::exception&) {
                // ignore a read-only config
            }
        }
        if (config_.update().last_check > 0) {
            const std::time_t t = config_.update().last_check;
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
            ImGui::TextDisabled("Last checked: %s", buf);
        } else {
            ImGui::TextDisabled("Last checked: never");
        }
        if (ImGui::Button("Check now")) {
            start_update_check();
            show_settings_ = false;
        }
        ImGui::Separator();
        ImGui::TextDisabled("Field presets (from config)");
        for (const auto& [name, tokens] : config_.presets()) {
            ImGui::BulletText("%s", name.c_str());
        }
        if (config_.presets().empty()) {
            ImGui::BulletText("(none)");
        }
        ImGui::Separator();
        if (ImGui::Button("Close")) {
            show_settings_ = false;
        }
        ImGui::EndPopup();
    }
}

void App::draw_about_modal() {
    ensure_open("About PFDB", show_about_);
    if (ImGui::BeginPopupModal("About PFDB", &show_about_,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("PFDB - Power Film DataBase");
#ifdef PFDB_VERSION
        ImGui::Text("Version %s", PFDB_VERSION);
#endif
#ifdef PFDB_BUILD_DATE
        ImGui::Text("Built %s", PFDB_BUILD_DATE);
#endif
        ImGui::Text("Dear ImGui %s  |  GLFW %s", ImGui::GetVersion(),
                    glfwGetVersionString());
        ImGui::Separator();
        ImGui::TextWrapped(
            "A power-user film collection manager. Data is scraped from IMDb, "
            "FilmAffinity and BoxOfficeMojo for personal, non-commercial use.");
        ImGui::Separator();
        if (ImGui::Button("Close")) {
            show_about_ = false;
        }
        ImGui::EndPopup();
    }
}

void App::frame() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("PFDB", nullptr, flags);
    maybe_auto_check();
    poll_jobs();
    draw_toolbar();
    ImGui::Separator();
    draw_panels();
    ImGui::End();

    // Modals (their open flags are toggled elsewhere).
    draw_filter_modal();
    draw_add_modal();
    draw_settings_modal();
    draw_about_modal();
    draw_diff_modal();
    draw_edit_modal();
    draw_update_modals();
}

}  // namespace pfdb::gui
