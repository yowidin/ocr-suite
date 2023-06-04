//
// Created by Dennis Sitelew on 21.03.23.
//

#pragma once

#include <imgui.h>

namespace ocs::viewer::views::util {

struct window {
   explicit window(const char *name)
      : window{name, nullptr, 0} {}

   window(const char *name, bool *p_open)
      : window{name, p_open, 0} {}

   window(const char *name, ImGuiWindowFlags flags)
      : window{name, nullptr, flags} {}

   window(const char *name, bool *p_open, ImGuiWindowFlags flags) { ImGui::Begin(name, p_open, flags); }

   ~window() { ImGui::End(); }
};

} // namespace ocs::viewer::views::util