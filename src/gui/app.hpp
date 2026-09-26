#pragma once

#include <string>
#include <vector>

#include "app/config.hpp"
#include "db/repository.hpp"
#include "gui/async.hpp"
#include "gui/texture.hpp"
#include "model/collection_model.hpp"
#include "pfdb/film.hpp"
#include "query/engine.hpp"
#include "sources/source.hpp"

namespace pfdb::gui {

/// Today's date as "YYYY-MM-DD" (local time).
std::string today_iso();

/// Open the named popup once when `flag` first becomes true (idempotent).
void ensure_open(const char* name, bool flag);

enum class Layout { Vertical, Horizontal };

/// One editable filter in the Filters modal: `field op value` (M4 grammar).
struct FilterRow {
    std::string field = "title";
    std::string op = "~";
    std::string value;
};

// --- Background-job result payloads (plain data; applied on the UI thread) ---

struct SearchOutcome {
    struct PerSource {
        std::string id;
        std::string display;
        std::vector<sources::SearchResult> results;
    };
    std::vector<PerSource> sources;
    std::string error;
};

struct FetchOutcome {
    Film film;
    std::vector<sources::RelatedRef> relations;
    std::vector<sources::SimilarRef> similars;
    std::string cover_mime;   // downloaded on the worker thread (empty = none)
    std::string cover_bytes;
    std::string error;
};

struct UpdateOutcome {
    Id film_id = kInvalidId;
    Film film;
    std::vector<sources::RelatedRef> relations;
    std::vector<sources::SimilarRef> similars;
    std::string error;
};

// --- Modal state ---

struct AddState {
    bool open = false;
    std::string query;
    bool use_imdb = true;
    bool use_fa = true;
    bool financials = true;
    bool cover = true;
    std::vector<SearchOutcome::PerSource> sources;  // filled after a search
    std::vector<int> selected;                      // selected row per source (-1 = none)
    std::string status;
};

struct DiffState {
    bool open = false;
    Id film_id = kInvalidId;
    std::vector<std::string> changes;
    Film refreshed;
    std::vector<sources::RelatedRef> relations;
    std::vector<sources::SimilarRef> similars;
};

struct EditState {
    bool open = false;
    Film draft;
    std::string new_genre;
    std::string new_topic;
    std::string new_group;
};

/// The whole GUI application: owns the data, the query/view state, and draws one
/// frame at a time. Business actions delegate to the existing pfdb services.
class App {
public:
    App(std::string db_path, std::string config_path);

    /// Draw one frame (called each iteration of the render loop).
    void frame();

    /// Whether dark theme is active (main.cpp applies the ImGui style).
    bool dark_theme() const { return dark_; }

private:
    // --- data ---
    db::Repository repo_;
    CollectionModel model_;
    config::Config config_;
    std::vector<const Film*> results_;
    Id selected_ = kInvalidId;

    // --- query/view state ---
    std::vector<FilterRow> filters_;
    std::string where_;
    std::string sort_ = "year:desc,title:asc";
    std::string quick_title_;
    std::string query_error_;

    // --- ui state ---
    Layout layout_ = Layout::Vertical;
    bool dark_ = true;
    float split_ratio_ = 0.62f;
    TextureCache textures_;
    std::string toast_;

    // --- modals + jobs ---
    AddState add_;
    DiffState diff_;
    EditState edit_;
    bool show_settings_ = false;
    bool show_about_ = false;
    bool show_filters_ = false;
    Job<SearchOutcome> search_job_;
    Job<FetchOutcome> add_job_;
    Job<UpdateOutcome> update_job_;

    // --- core (app.cpp) ---
    void apply_theme();
    void reload_model();
    void refresh();
    query::QueryRequest build_request() const;
    const Film* selected_film() const;
    void apply_filter(const std::string& field, const std::string& op,
                      const std::string& value);
    void store_edges(Id film_id, const std::vector<sources::RelatedRef>& relations,
                     const std::vector<sources::SimilarRef>& similars);
    void draw_toolbar();
    void draw_panels();
    void draw_settings_modal();
    void draw_about_modal();
    void poll_jobs();

    // --- views (separate .cpp files) ---
    void draw_table();         // table_view.cpp
    void draw_detail();        // detail_view.cpp
    void draw_add_modal();     // add_modal.cpp
    void draw_filter_modal();  // filter_modal.cpp

    // --- detail-panel actions (detail_view.cpp) ---
    void mark_seen(Id film_id);
    void remove_film(Id film_id);
    void start_update(Id film_id);
    void draw_diff_modal();
    void draw_edit_modal();
    void save_edit();
};

}  // namespace pfdb::gui
