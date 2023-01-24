//
// Created by Dennis Sitelew on 22.01.23.
//

#include <ocs/viewer/views/search_results_view.h>

#include <imgui.h>

using namespace ocs::viewer::views;

void search_results_view::draw() {
   ImGui::Begin(name());

   ImGui::Text("Search Results...");

   ImGui::End();
}
