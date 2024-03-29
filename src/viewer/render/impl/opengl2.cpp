/**
 * @file   opengl2.cpp
 * @author Dennis Sitelew
 * @date   Sep. 14, 2021
 */

#include <ocs/viewer/render/frontend.h>
#include <ocs/viewer/render/impl/opengl2.h>

#include <imgui_impl_opengl2.h>

#include <stdexcept>

using namespace ocs::viewer::render::impl;

OpenGL2::~OpenGL2() {
   ImGui_ImplOpenGL2_Shutdown();
}

void OpenGL2::init() {
   if (!ImGui_ImplOpenGL2_Init()) {
      throw std::runtime_error("ImGui_ImplOpenGL2_Init failed");
   }
}

void OpenGL2::new_frame() {
   ImGui_ImplOpenGL2_NewFrame();
}

void OpenGL2::render() {
   ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}
