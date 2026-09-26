#include <optional>
#include <string>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "app/cover.hpp"
#include "app/enrichment.hpp"
#include "gui/app.hpp"
#include "net/http_client.hpp"
#include "sources/source_registry.hpp"

namespace pfdb::gui {

void App::draw_add_modal() {
    ensure_open("Add movie", add_.open);
    if (!ImGui::BeginPopupModal("Add movie", &add_.open, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::SetNextItemWidth(360.0F);
    const bool enter = ImGui::InputTextWithHint(
        "##q", "Search query...", &add_.query, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Checkbox("IMDb", &add_.use_imdb);
    ImGui::SameLine();
    ImGui::Checkbox("FilmAffinity", &add_.use_fa);
    ImGui::SameLine();
    ImGui::Checkbox("Financials", &add_.financials);
    ImGui::SameLine();
    ImGui::Checkbox("Cover", &add_.cover);

    const bool searching = search_job_.running();
    ImGui::BeginDisabled(searching || add_.query.empty() || (!add_.use_imdb && !add_.use_fa));
    const bool go = ImGui::Button("Search") || enter;
    ImGui::EndDisabled();
    if (go) {
        search_job_.start([q = add_.query, imdb = add_.use_imdb,
                           fa = add_.use_fa]() -> SearchOutcome {
            SearchOutcome o;
            try {
                net::CprHttpClient http;
                if (imdb) {
                    o.sources.push_back(
                        {"imdb", "IMDb", sources::make_source("imdb", http)->search(q)});
                }
                if (fa) {
                    o.sources.push_back({"filmaffinity", "FilmAffinity",
                                         sources::make_source("filmaffinity", http)->search(q)});
                }
            } catch (const std::exception& e) {
                o.error = e.what();
            }
            return o;
        });
        add_.status = "Searching...";
    }
    if (searching) {
        ImGui::SameLine();
        ImGui::TextDisabled("searching...");
    }
    if (!add_.status.empty() && !searching) {
        ImGui::TextWrapped("%s", add_.status.c_str());
    }

    // --- Per-source result tabs ---
    if (!add_.sources.empty() && ImGui::BeginTabBar("add_sources")) {
        for (std::size_t si = 0; si < add_.sources.size(); ++si) {
            const auto& src = add_.sources[si];
            const std::string label =
                src.display + " (" + std::to_string(src.results.size()) + ")";
            if (ImGui::BeginTabItem(label.c_str())) {
                if (ImGui::BeginTable("res", 3,
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_BordersInnerV,
                                      ImVec2(0, 220))) {
                    ImGui::TableSetupColumn("Title");
                    ImGui::TableSetupColumn("Year", ImGuiTableColumnFlags_WidthFixed, 50);
                    ImGui::TableSetupColumn("Director / cast");
                    ImGui::TableHeadersRow();
                    for (std::size_t ri = 0; ri < src.results.size(); ++ri) {
                        const auto& r = src.results[ri];
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::PushID(static_cast<int>(ri));
                        const bool sel = add_.selected[si] == static_cast<int>(ri);
                        if (ImGui::Selectable(r.title.c_str(), sel,
                                              ImGuiSelectableFlags_SpanAllColumns)) {
                            add_.selected[si] = static_cast<int>(ri);
                        }
                        ImGui::PopID();
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(r.year ? std::to_string(*r.year).c_str() : "");
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(r.subtitle.c_str());
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    // --- Confirm: fetch the selected id from each source and merge ---
    ImGui::Separator();
    bool any_selected = false;
    for (int s : add_.selected) {
        any_selected = any_selected || s >= 0;
    }
    const bool adding = add_job_.running();
    ImGui::BeginDisabled(!any_selected || adding);
    if (ImGui::Button("Add selected")) {
        std::optional<std::string> imdb_id;
        std::optional<std::string> fa_id;
        for (std::size_t si = 0; si < add_.sources.size(); ++si) {
            const int sel = add_.selected[si];
            if (sel < 0) {
                continue;
            }
            const std::string& id = add_.sources[si].results[static_cast<std::size_t>(sel)]
                                        .external_id;
            if (add_.sources[si].id == "imdb") {
                imdb_id = id;
            } else if (add_.sources[si].id == "filmaffinity") {
                fa_id = id;
            }
        }
        std::optional<std::string> bom_id =
            (add_.financials && imdb_id) ? imdb_id : std::nullopt;
        const bool want_cover = add_.cover;
        add_job_.start([imdb_id, fa_id, bom_id, want_cover]() -> FetchOutcome {
            FetchOutcome o;
            try {
                net::CprHttpClient http;
                std::optional<Film> imf;
                std::optional<Film> faf;
                std::optional<Film> bomf;
                std::string cover_url;
                if (imdb_id) {
                    sources::SourceFetch r = sources::make_source("imdb", http)->fetch(*imdb_id);
                    imf = std::move(r.film);
                    cover_url = r.cover_url;
                }
                if (fa_id) {
                    sources::SourceFetch r =
                        sources::make_source("filmaffinity", http)->fetch(*fa_id);
                    faf = std::move(r.film);
                    o.relations = std::move(r.relations);
                    o.similars = std::move(r.similars);
                    if (cover_url.empty()) {
                        cover_url = r.cover_url;
                    }
                }
                if (bom_id) {
                    bomf = sources::make_source("boxofficemojo", http)->fetch(*bom_id).film;
                }
                o.film = app::merge_films(imf, faf, bomf);
                if (want_cover && !cover_url.empty()) {
                    net::HttpResponse resp = http.get(cover_url);
                    if (resp.ok() && !resp.body.empty()) {
                        o.cover_bytes = resp.body;
                        o.cover_mime = app::mime_from_url(cover_url);
                    }
                }
            } catch (const std::exception& e) {
                o.error = e.what();
            }
            return o;
        });
        add_.status = "Fetching selected film...";
    }
    ImGui::EndDisabled();
    if (adding) {
        ImGui::SameLine();
        ImGui::TextDisabled("fetching...");
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        add_.open = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}  // namespace pfdb::gui
