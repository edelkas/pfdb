#include <array>
#include <string>
#include <vector>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "gui/app.hpp"
#include "query/field.hpp"

namespace pfdb::gui {

void App::draw_filter_modal() {
    ensure_open("Filters", show_filters_);
    if (!ImGui::BeginPopupModal("Filters", &show_filters_, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    static const std::vector<std::string> field_names = [] {
        std::vector<std::string> v;
        for (const auto& f : query::all_fields()) {
            v.push_back(f.name);
        }
        return v;
    }();
    static constexpr std::array<const char*, 8> kOps = {"~", "=~", "=", ">",
                                                        ">=", "<", "<=", "has"};

    ImGui::TextDisabled(
        "Each filter is 'field op value'. Refer to them as F1, F2... in Where.");

    int remove = -1;
    for (std::size_t i = 0; i < filters_.size(); ++i) {
        FilterRow& row = filters_[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text("F%zu", i + 1);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150.0F);
        if (ImGui::BeginCombo("##field", row.field.c_str())) {
            for (const auto& name : field_names) {
                if (ImGui::Selectable(name.c_str(), name == row.field)) {
                    row.field = name;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(56.0F);
        if (ImGui::BeginCombo("##op", row.op.c_str())) {
            for (const char* op : kOps) {
                if (ImGui::Selectable(op, row.op == op)) {
                    row.op = op;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0F);
        ImGui::InputText("##val", &row.value);
        ImGui::SameLine();
        if (ImGui::SmallButton("remove")) {
            remove = static_cast<int>(i);
        }
        ImGui::PopID();
    }
    if (remove >= 0) {
        filters_.erase(filters_.begin() + remove);
    }
    if (ImGui::Button("Add filter")) {
        filters_.push_back({});
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(360.0F);
    ImGui::InputTextWithHint("Where", "e.g. F1 AND (F2 OR F3) - blank means AND all",
                             &where_);
    ImGui::SetNextItemWidth(360.0F);
    ImGui::InputTextWithHint("Sort", "e.g. year:desc,title:asc", &sort_);

    if (!query_error_.empty()) {
        ImGui::TextColored(ImVec4(0.9F, 0.4F, 0.4F, 1.0F), "%s", query_error_.c_str());
    }

    ImGui::Separator();
    if (ImGui::Button("Apply")) {
        refresh();
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
        show_filters_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}  // namespace pfdb::gui
