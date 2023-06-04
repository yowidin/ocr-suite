/**
 * @file   gles3.cpp
 * @author Dennis Sitelew
 * @date   Sep. 14, 2021
 */

#include <ocs/viewer/render/imgui/imgui_impl_sdl_es3.h>
#include <ocs/viewer/render/impl/gles3.h>

#include <ocs/viewer/render/frontend.h>

#include <stdexcept>

using namespace ocs::viewer::render::impl;

GLES3::~GLES3() {
   ImGui_ImplSdlGLES3_Shutdown();
}

void GLES3::init() {
   if (!ImGui_ImplSdlGLES3_Init()) {
      throw std::runtime_error("ImGui_ImplSdlGLES2_Init failed");
   }
}

void GLES3::new_frame() {
   ImGui_ImplSdlGLES3_NewFrame(&frontend_->window());
}

void GLES3::render() {
   ImGui_ImplSdlGLES3_RenderDrawLists(ImGui::GetDrawData());
}
