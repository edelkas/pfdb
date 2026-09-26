#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "gui/app.hpp"
#include "gui/columns.hpp"

namespace pfdb::gui {

void App::draw_table() {
    // --- Filter bar ---
    ImGui::SetNextItemWidth(280.0F);
    if (ImGui::InputTextWithHint("##quick", "Filter by title...", &quick_title_)) {
        refresh();
    }
    ImGui::SameLine();
    if (ImGui::Button("Filters...")) {
        show_filters_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        filters_.clear();
        where_.clear();
        quick_title_.clear();
        refresh();
    }
    if (!query_error_.empty()) {
        ImGui::TextColored(ImVec4(0.9F, 0.4F, 0.4F, 1.0F), "query error: %s",
                           query_error_.c_str());
    }

    const std::vector<Column>& cols = all_columns();
    const ImGuiTableFlags flags =
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersInnerV;

    if (!ImGui::BeginTable("films", static_cast<int>(cols.size()), flags)) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    for (const auto& col : cols) {
        ImGuiTableColumnFlags cf = col.default_visible ? 0 : ImGuiTableColumnFlags_DefaultHide;
        if (!col.sortable) {
            cf |= ImGuiTableColumnFlags_NoSort;
        }
        ImGui::TableSetupColumn(col.header.c_str(), cf);
    }
    ImGui::TableHeadersRow();

    // --- Sorting: translate ImGui's multi-sort specs into our sort string ---
    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
        if (specs->SpecsDirty) {
            std::string spec;
            for (int i = 0; i < specs->SpecsCount; ++i) {
                const ImGuiTableColumnSortSpecs& s = specs->Specs[i];
                const Column& col = cols[static_cast<std::size_t>(s.ColumnIndex)];
                if (!col.sortable) {
                    continue;
                }
                if (!spec.empty()) {
                    spec += ",";
                }
                spec += col.token;
                spec += (s.SortDirection == ImGuiSortDirection_Descending) ? ":desc" : ":asc";
            }
            if (!spec.empty() && spec != sort_) {
                sort_ = spec;
                refresh();
            }
            specs->SpecsDirty = false;
        }
    }

    // --- Rows (clipped: only visible ones are built) ---
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(results_.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const Film* f = results_[static_cast<std::size_t>(row)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(row);
            const bool selected = f->id == selected_;
            const std::string first = format_field(cols[0].token, *f);
            if (ImGui::Selectable(first.c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns)) {
                selected_ = f->id;
            }
            ImGui::PopID();
            for (std::size_t c = 1; c < cols.size(); ++c) {
                if (ImGui::TableSetColumnIndex(static_cast<int>(c))) {
                    ImGui::TextUnformatted(format_field(cols[c].token, *f).c_str());
                }
            }
        }
    }
    ImGui::EndTable();
}

}  // namespace pfdb::gui
