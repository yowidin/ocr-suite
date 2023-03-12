//
// Created by Dennis Sitelew on 22.01.23.
//

#include <ocs/viewer/render/imgui/std_input_text.h>
#include <ocs/viewer/views/search_view.h>

#include <imgui.h>

using namespace ocs::viewer::views;
using namespace ocr::viewer::render;

void search_view::draw() {
   ImGui::Begin(name());

   imgui::std_input_text("##search", search_text_);

   if (db_->is_finished()) {
      if (ImGui::Button("Search")) {
         if (text_change_cb_) {
            text_change_cb_(search_text_);
         }
      }

#if 0
      const auto &search_result = db_->get_results();
      if (!search_result.empty()) {
         std::size_t total = 0;
         for (const auto &e : search_result) {
            total += e.entries.size();
         }

         ImGui::Text("Total results: %lu", total);
      }
#endif
   } else {
      ImGui::BeginDisabled();
      ImGui::Button("Searching");
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::Text("%05.2f%%", db_->get_progress());
   }

   ImGui::End();
}