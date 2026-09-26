#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "app/enrichment.hpp"
#include "app/player.hpp"
#include "gui/app.hpp"
#include "gui/columns.hpp"
#include "net/http_client.hpp"
#include "sources/source_registry.hpp"

namespace pfdb::gui {
namespace {

std::optional<std::string> ref_for(const Film& f, const std::string& source) {
    for (const auto& r : f.source_refs) {
        if (r.source == source) {
            return r.external_id;
        }
    }
    return std::nullopt;
}

// A small clickable "chip" that applies a filter when pressed.
bool chip(const std::string& label) {
    return ImGui::SmallButton(label.c_str());
}

}  // namespace

void App::mark_seen(Id film_id) {
    auto film = repo_.find(film_id);
    if (!film) {
        return;
    }
    film->user.watch_count += 1;
    film->user.date_watched = today_iso();
    repo_.update(*film);
    reload_model();
    toast_ = "Marked seen (" + std::to_string(film->user.watch_count) + "x)";
}

void App::remove_film(Id film_id) {
    repo_.remove(film_id);
    if (selected_ == film_id) {
        selected_ = kInvalidId;
    }
    textures_.invalidate(film_id);
    reload_model();
    toast_ = "Removed film";
}

void App::start_update(Id film_id) {
    const Film* f = model_.find(film_id);
    if (f == nullptr || update_job_.running()) {
        return;
    }
    const auto imdb = ref_for(*f, "imdb");
    const auto fa = ref_for(*f, "filmaffinity");
    const auto bom = ref_for(*f, "boxofficemojo");
    if (!imdb && !fa && !bom) {
        toast_ = "Film has no online sources to update from";
        return;
    }
    update_job_.start([film_id, imdb, fa, bom]() -> UpdateOutcome {
        UpdateOutcome o;
        o.film_id = film_id;
        try {
            net::CprHttpClient http;
            std::optional<Film> imf;
            std::optional<Film> faf;
            std::optional<Film> bomf;
            if (imdb) {
                imf = sources::make_source("imdb", http)->fetch(*imdb).film;
            }
            if (fa) {
                sources::SourceFetch r = sources::make_source("filmaffinity", http)->fetch(*fa);
                faf = std::move(r.film);
                o.relations = std::move(r.relations);
                o.similars = std::move(r.similars);
            }
            if (bom) {
                bomf = sources::make_source("boxofficemojo", http)->fetch(*bom).film;
            }
            o.film = app::merge_films(imf, faf, bomf);
        } catch (const std::exception& e) {
            o.error = e.what();
        }
        return o;
    });
    toast_ = "Updating from sources...";
}

void App::draw_detail() {
    const Film* f = selected_film();
    if (f == nullptr) {
        ImGui::TextDisabled("Select a film in the table to see its details.");
        return;
    }

    // --- Action buttons ---
    if (ImGui::Button("Update")) {
        start_update(f->id);
    }
    ImGui::SameLine();
    if (ImGui::Button("Edit")) {
        edit_ = EditState{};
        edit_.draft = *f;
        edit_.open = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Mark seen today")) {
        mark_seen(f->id);
        return;  // film pointer is now stale
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove")) {
        ImGui::OpenPopup("Confirm remove");
    }
    if (f->video && !f->video->path.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Play")) {
            if (!app::open_in_default_app(f->video->path)) {
                toast_ = "Could not launch a player";
            }
        }
    }
    ImGui::Separator();

    // --- Cover (cached once per film) + title block ---
    if (!textures_.has(f->id)) {
        if (auto cover = repo_.get_cover(f->id)) {
            textures_.get(f->id, cover->bytes);
        } else {
            textures_.get(f->id, "");
        }
    }
    const Texture& tex = textures_.get(f->id, "");
    if (tex.id != 0 && tex.width > 0) {
        const float w = 180.0F;
        const float h = static_cast<float>(tex.height) * (w / static_cast<float>(tex.width));
        ImGui::Image(static_cast<ImTextureID>(tex.id), ImVec2(w, h));
        ImGui::SameLine();
    }

    ImGui::BeginGroup();
    ImGui::TextWrapped("%s", f->title.c_str());
    if (!f->original_title.empty() && f->original_title != f->title) {
        ImGui::TextDisabled("%s", f->original_title.c_str());
    }
    if (f->year) {
        if (chip(std::to_string(*f->year))) {
            apply_filter("year", "=", std::to_string(*f->year));
        }
        ImGui::SameLine();
    }
    if (f->runtime_minutes) {
        ImGui::Text("%d min", *f->runtime_minutes);
    }
    // Director chips.
    {
        bool any = false;
        for (const auto& c : f->credits) {
            if (c.role != CreditRole::Director) {
                continue;
            }
            if (!any) {
                ImGui::TextUnformatted("Director:");
                any = true;
            }
            ImGui::SameLine();
            if (chip(c.person.name)) {
                apply_filter("director", "has", c.person.name);
            }
        }
    }
    // Genre chips.
    if (!f->genres.empty()) {
        ImGui::TextUnformatted("Genres:");
        for (const auto& g : f->genres) {
            ImGui::SameLine();
            if (chip(g)) {
                apply_filter("genre", "has", g);
            }
        }
    }
    ImGui::EndGroup();

    ImGui::Separator();

    // --- Detail sections ---
    if (ImGui::BeginTabBar("detail_tabs")) {
        if (ImGui::BeginTabItem("Overview")) {
            if (!f->synopsis.empty()) {
                ImGui::TextWrapped("%s", f->synopsis.c_str());
                ImGui::Separator();
            }
            for (const auto& r : f->ratings) {
                ImGui::Text("%s: %.1f/%.0f%s", r.source.c_str(), r.value, r.scale,
                            r.votes ? ("  (" + std::to_string(*r.votes) + " votes)").c_str()
                                    : "");
            }
            if (f->budget) {
                ImGui::Text("Budget: $%lld", static_cast<long long>(*f->budget));
            }
            if (f->gross) {
                ImGui::Text("Gross:  $%lld", static_cast<long long>(*f->gross));
            }
            if (!f->topics.empty()) {
                ImGui::TextWrapped("Topics: %s",
                                   format_field("topic", *f).c_str());
            }
            if (!f->groups.empty()) {
                ImGui::TextWrapped("Groups: %s", format_field("group", *f).c_str());
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Cast & crew")) {
            for (const auto& c : f->credits) {
                ImGui::BulletText("%s - %s%s", to_string(c.role).data(),
                                  c.person.name.c_str(),
                                  c.character.empty() ? ""
                                                      : (" as " + c.character).c_str());
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("My data")) {
            ImGui::Text("Watched: %s", f->user.date_watched.value_or("-").c_str());
            ImGui::Text("Watch count: %d", f->user.watch_count);
            ImGui::Text("My rating: %s",
                        f->user.personal_rating
                            ? std::to_string(*f->user.personal_rating).c_str()
                            : "-");
            ImGui::Text("Owned: %s  Wishlist: %s  Favourite: %s",
                        f->user.owned ? "yes" : "no", f->user.wishlist ? "yes" : "no",
                        f->user.favorite ? "yes" : "no");
            if (!f->user.notes.empty()) {
                ImGui::TextWrapped("Notes: %s", f->user.notes.c_str());
            }
            ImGui::EndTabItem();
        }
        if (f->video && ImGui::BeginTabItem("Video file")) {
            const auto& v = *f->video;
            ImGui::TextWrapped("%s", v.path.c_str());
            if (v.width && v.height) {
                ImGui::Text("Resolution: %dx%d", *v.width, *v.height);
            }
            if (v.framerate) {
                ImGui::Text("Frame rate: %.3f fps", *v.framerate);
            }
            if (v.video_bitrate) {
                ImGui::Text("Video bitrate: %d bps", *v.video_bitrate);
            }
            ImGui::SeparatorText("Audio tracks");
            for (const auto& a : v.audio_tracks) {
                ImGui::BulletText("%s %s %s %s", a.codec.c_str(), a.language.c_str(),
                                  a.channels ? (std::to_string(*a.channels) + "ch").c_str()
                                             : "",
                                  a.name.c_str());
            }
            ImGui::SeparatorText("Subtitle tracks");
            for (const auto& s : v.subtitle_tracks) {
                ImGui::BulletText("%s %s %s", s.format.c_str(), s.language.c_str(),
                                  s.name.c_str());
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // --- Confirm-remove modal (opened by the Remove button above) ---
    if (ImGui::BeginPopupModal("Confirm remove", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Remove \"%s\" from the collection?", f->title.c_str());
        ImGui::TextDisabled("This cannot be undone.");
        ImGui::Separator();
        if (ImGui::Button("Remove")) {
            const Id id = f->id;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            remove_film(id);
            return;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::draw_diff_modal() {
    ensure_open("Changes", diff_.open);
    if (ImGui::BeginPopupModal("Changes", &diff_.open,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Refreshed \"%s\" from its sources:", diff_.refreshed.title.c_str());
        ImGui::Separator();
        for (const auto& line : diff_.changes) {
            ImGui::BulletText("%s", line.c_str());
        }
        ImGui::Separator();
        if (ImGui::Button("Apply")) {
            const Film* existing = model_.find(diff_.film_id);
            Film r = diff_.refreshed;
            r.id = diff_.film_id;
            if (existing != nullptr) {
                r.created_at = existing->created_at;
                r.user = existing->user;
                r.video = existing->video;
            }
            repo_.update(r);
            store_edges(diff_.film_id, diff_.relations, diff_.similars);
            reload_model();
            diff_.open = false;
            toast_ = "Updated \"" + r.title + "\"";
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard")) {
            diff_.open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

namespace {

// A list editor: shows each item with a remove button, plus an add field.
void edit_list(const char* label, std::vector<std::string>& items, std::string& scratch) {
    ImGui::SeparatorText(label);
    int remove = -1;
    for (std::size_t i = 0; i < items.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton("x")) {
            remove = static_cast<int>(i);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(items[i].c_str());
        ImGui::PopID();
    }
    if (remove >= 0) {
        items.erase(items.begin() + remove);
    }
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(200.0F);
    ImGui::InputText("##add", &scratch);
    ImGui::SameLine();
    if (ImGui::Button("Add") && !scratch.empty()) {
        items.push_back(scratch);
        scratch.clear();
    }
    ImGui::PopID();
}

}  // namespace

void App::draw_edit_modal() {
    ensure_open("Edit film", edit_.open);
    if (ImGui::BeginPopupModal("Edit film", &edit_.open, ImGuiWindowFlags_AlwaysAutoResize)) {
        Film& d = edit_.draft;
        ImGui::InputText("Title", &d.title);
        ImGui::InputText("Original title", &d.original_title);
        ImGui::InputText("Spanish title", &d.spanish_title);

        int year = d.year.value_or(0);
        if (ImGui::InputInt("Year", &year)) {
            d.year = year > 0 ? std::optional<int>(year) : std::nullopt;
        }
        int runtime = d.runtime_minutes.value_or(0);
        if (ImGui::InputInt("Runtime (min)", &runtime)) {
            d.runtime_minutes = runtime > 0 ? std::optional<int>(runtime) : std::nullopt;
        }
        ImGui::InputTextMultiline("Synopsis", &d.synopsis, ImVec2(400, 80));

        auto edit_money = [](const char* label, std::optional<std::int64_t>& v) {
            long long tmp = v.value_or(0);
            if (ImGui::InputScalar(label, ImGuiDataType_S64, &tmp)) {
                v = tmp > 0 ? std::optional<std::int64_t>(tmp) : std::nullopt;
            }
        };
        edit_money("Budget", d.budget);
        edit_money("Gross", d.gross);

        ImGui::SeparatorText("My data");
        std::string dw = d.user.date_watched.value_or("");
        if (ImGui::InputText("Date watched", &dw)) {
            d.user.date_watched = dw.empty() ? std::nullopt : std::optional<std::string>(dw);
        }
        ImGui::InputInt("Watch count", &d.user.watch_count);
        double rating = d.user.personal_rating.value_or(0.0);
        if (ImGui::InputDouble("My rating", &rating, 0.0, 0.0, "%.1f")) {
            d.user.personal_rating =
                rating > 0.0 ? std::optional<double>(rating) : std::nullopt;
        }
        ImGui::Checkbox("Owned", &d.user.owned);
        ImGui::SameLine();
        ImGui::Checkbox("Wishlist", &d.user.wishlist);
        ImGui::SameLine();
        ImGui::Checkbox("Favourite", &d.user.favorite);
        ImGui::InputTextMultiline("Notes", &d.user.notes, ImVec2(400, 60));

        edit_list("Genres", d.genres, edit_.new_genre);
        edit_list("Topics", d.topics, edit_.new_topic);
        edit_list("Groups", d.groups, edit_.new_group);

        ImGui::Separator();
        if (ImGui::Button("Save")) {
            save_edit();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            edit_.open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::save_edit() {
    repo_.update(edit_.draft);
    reload_model();
    edit_.open = false;
    toast_ = "Saved changes";
}

}  // namespace pfdb::gui
