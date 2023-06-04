//
// Created by Dennis Sitelew on 22.01.23.
//

#ifndef OCR_SUITE_STD_INPUT_TEXT_H
#define OCR_SUITE_STD_INPUT_TEXT_H

#include <imgui.h>

#include <string>

namespace ocr::viewer::render::imgui {

bool std_input_text(const char *label,
                    std::string &str,
                    ImGuiInputTextFlags flags = 0,
                    ImGuiInputTextCallback callback = nullptr,
                    void *user_data = nullptr);

} // namespace ocr::viewer::render::imgui

#endif // OCR_SUITE_STD_INPUT_TEXT_H
