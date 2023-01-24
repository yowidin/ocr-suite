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

   if (ImGui::Button("Search")) {
      if (text_change_cb_) {
         text_change_cb_(search_text_);
      }
   }

   ImGui::End();
}