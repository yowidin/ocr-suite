//
// Created by Dennis Sitelew on 22.01.23.
//

#include <ocs/viewer/views/viewer.h>

#include <imgui_internal.h>
#include <dejavu.h>

using namespace ocs::viewer::views;
using namespace ocs::viewer::render;

viewer::viewer(options opts)
   : opts_{std::move(opts)}
   , db_{search_results_, opts_}
   , frame_view_{search_results_view_}
   , search_results_{opts_.in_memory_results}
   , search_results_view_{db_, frame_view_} {
   window::options win_opts = {};
   win_opts.title = "OCS Viewer";
   window_ = std::make_unique<window>(win_opts, [this]() { draw(); });

   db_.collect_files();

   search_view_.set_text_change_cb([this](const std::string &text) { search_text_changed(text); });
   search_view_.set_search_engine(db_);

   auto io = ImGui::GetIO();
   auto font_height = 16.0f;
   io.Fonts->AddFontFromMemoryCompressedBase85TTF(DejaVu_compressed_data_base85, font_height, nullptr,
                                                  io.Fonts->GetGlyphRangesCyrillic());
}

void viewer::run() {
   while (!window_->can_stop()) {
      window_->update();
   }
}

void viewer::draw() {
   // Prepare the initial split layout
   ImGuiID dock_space_id = ImGui::GetID("Workspace");

#if 0
   static bool flag=false;
   ImGui::BeginMainMenuBar();
   ImGui::MenuItem("Debug", NULL, &flag);
   if (flag) {
      ImGui::ShowMetricsWindow();
   }
   ImGui::EndMainMenuBar();
#endif

   // TODO: Only configure the dock if it it wasn't stored in the settings file (same goes for the window size)
   static bool dock_configured = false;
   if (!dock_configured) {
      ImGui::DockBuilderRemoveNode(dock_space_id); // Clear out existing layout
      ImGui::DockBuilderAddNode(dock_space_id);    // Add empty node

      ImGuiID search, search_results, frame;
      search = ImGui::DockBuilderSplitNode(dock_space_id, ImGuiDir_Left, 0.20f, nullptr, &frame);
      search = ImGui::DockBuilderSplitNode(search, ImGuiDir_Up, 0.20f, nullptr, &search_results);

      ImGui::DockBuilderDockWindow(search_view_.name(), search);
      ImGui::DockBuilderDockWindow(search_results_view_.name(), search_results);
      ImGui::DockBuilderDockWindow(frame_view_.name(), frame);

      ImGui::DockBuilderGetNode(search)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
      ImGui::DockBuilderGetNode(search_results)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
      ImGui::DockBuilderGetNode(frame)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

      ImGui::DockBuilderFinish(dock_space_id);

      dock_configured = true;
   }

   const ImGuiViewport *viewport = ImGui::GetMainViewport();
   ImGui::DockBuilderSetNodeSize(dock_space_id, viewport->WorkSize);
   ImGui::DockBuilderSetNodePos(dock_space_id, viewport->WorkPos);

   search_view_.draw();
   search_results_view_.draw();
   frame_view_.draw();
}

void viewer::search_text_changed(const std::string &text) {
   db_.find_text(text);

   // TODO React to enter press
}
