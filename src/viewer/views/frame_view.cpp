//
// Created by Dennis Sitelew on 22.01.23.
//

#include <ocs/viewer/views/frame_view.h>

#include <imgui.h>

using namespace ocs::viewer::views;

void frame_view::draw() {
   ImGui::Begin(name());

   ImGui::Text("Frame View...");

   ImGui::End();
}