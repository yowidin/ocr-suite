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

   const bool search_focused = ImGui::IsItemFocused();
   const bool enter_pressed = ImGui::IsKeyPressed(ImGuiKey_Enter);
   const bool keypad_enter_pressed = ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);

   bool fire_callback = search_focused && (enter_pressed || keypad_enter_pressed);

   if (db_->is_finished()) {
      const bool button_pressed = ImGui::Button("Search");
      fire_callback = button_pressed || fire_callback;

      if (fire_callback && text_change_cb_ && last_search_text_ != search_text_) {
         text_change_cb_(search_text_);
         last_search_text_ = search_text_;
      }
   } else {
      ImGui::BeginDisabled();
      ImGui::Button("Searching");
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::Text("%05.2f%%", db_->get_progress());
   }

   ImGui::End();
}