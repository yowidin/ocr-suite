//
// Created by Dennis Sitelew on 22.01.23.
//

#include <ocs/ffmpeg/decoder.h>
#include <ocs/viewer/views/frame_view.h>

#include <imgui.h>

#include <spdlog/spdlog.h>

using namespace ocs::viewer::views;

// TODO: Improve navigation
//  - Fix time decoding
//  - Jump no next/previous
//    - Text entry
//    - Frame
//    - Minute
//    - Hour
//    - Day

void frame_view::load_image_from_frame() {
   const auto &frame = current_frame_.value();

   using decoder_t = ocs::ffmpeg::decoder;
   decoder_t *decoder_ptr;
   auto frame_cb = [&decoder_ptr, this](const AVFrame &ffmpeg_frame, std::int64_t frame_number) {
      decoder_t::frame frame;
      decoder_ptr->to_frame(ffmpeg_frame, frame_number, frame);
      make_texture(frame);
      return decoder_t::action::stop;
   };

   decoder_t decoder(frame.video_file, ocs::ffmpeg::decoder::frame_filter::all_frames, frame_cb, frame.number);
   decoder_ptr = &decoder;
   decoder.run();
}

void frame_view::make_texture(const ffmpeg::decoder::frame &decoded) {
   // Create a OpenGL texture identifier
   GLuint image_texture;
   glGenTextures(1, &image_texture);
   glBindTexture(GL_TEXTURE_2D, image_texture);

   // Setup filtering parameters for display
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   // This is required on WebGL for non power-of-two textures
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
   glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, decoded.width, decoded.height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                decoded.data.data());

   texture_handle_ = image_texture;
   texture_width_ = decoded.width;
   texture_height_ = decoded.height;
}

void frame_view::set_current_frame(const frame_t &frame) {
   if (texture_handle_ != 0) {
      glDeleteTextures(1, &texture_handle_);
      texture_handle_ = 0;
   }

   current_frame_ = frame;
   if (current_frame_) {
      load_image_from_frame();
      was_dragging_ = false;
      scroll_to_idx_ = 0;
   }
}

void frame_view::scroll_to_text_entry(const search_results_view::text &entry) {
   const auto view_port_middle = ImVec2{view_port_size_.x / 2.0f, view_port_size_.y / 2.0f};
   const auto entry_half_size =
       ImVec2{static_cast<float>(entry.right - entry.left) / 2.0f, static_cast<float>(entry.top - entry.bottom) / 2.0f};
   const auto entry_middle =
       ImVec2{static_cast<float>(entry.left) + entry_half_size.x, static_cast<float>(entry.top) + entry_half_size.y};

   ImGui::SetScrollX(entry_middle.x - view_port_middle.x);
   ImGui::SetScrollY(entry_middle.y - view_port_middle.y);
}

void frame_view::draw() {
   static int alpha_factor = 255;
   static bool decrease = true;
   const int step = 16;
   alpha_factor -= (decrease ? step : -step);
   if (alpha_factor < 64) {
      decrease = false;
      alpha_factor = 64;
   }

   if (alpha_factor > 255) {
      decrease = true;
      alpha_factor = 255;
   }

   ImGui::Begin(name(), nullptr, ImGuiWindowFlags_HorizontalScrollbar);

   const auto p = ImGui::GetCursorScreenPos();

   view_port_size_ = ImGui::GetContentRegionAvail();

   auto draw_list = ImGui::GetForegroundDrawList();

   // "Interpolate" color between green and red (passing through yellow) - the higher value - the redder
   auto lerp_color = [&](uint16_t value, uint16_t max = 255) -> ImU32 {
      uint8_t r, g;
      double dStep = 510.0 / max;
      double dValue = dStep * value;
      if (dValue <= 255.0) {
         g = 255;
         r = static_cast<uint8_t>(dValue);
      } else {
         g = static_cast<uint8_t>(510.0 - dValue);
         r = 255;
      }
      return ImColor(r, g, 0, alpha_factor);
   };

   auto confidence_to_color = [&](const auto confidence) {
      // We want "greener" color for higher confidence
      auto reverted = 100.0 - confidence;
      return lerp_color(reverted, 100);
   };

   if (!current_frame_) {
      ImGui::Text("Frame View...");
   } else {
      if (texture_handle_ != 0) {
         ImGui::Image(reinterpret_cast<void *>(texture_handle_),
                      ImVec2(static_cast<float>(texture_width_), static_cast<float>(texture_height_)));

         const auto &frame = current_frame_.value();

         auto global_mouse_pos = ImGui::GetMousePos();
         ImVec2 local_mouse_pos{global_mouse_pos.x - p.x, global_mouse_pos.y - p.y};

         if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

            if (!was_dragging_) {
               initial_scroll_position_.x = ImGui::GetScrollX();
               initial_scroll_position_.y = ImGui::GetScrollY();
               was_dragging_ = true;
            }

            auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            ImGui::SetScrollX(initial_scroll_position_.x - delta.x);
            ImGui::SetScrollY(initial_scroll_position_.y - delta.y);
         } else {
            was_dragging_ = false;
         }

         auto clip_min = ImVec2(p.x + ImGui::GetScrollX(), p.y + ImGui::GetScrollY());
         auto clip_max = ImVec2(clip_min.x + view_port_size_.x, clip_min.y + view_port_size_.y);
         draw_list->PushClipRect(clip_min, clip_max, true);

         for (int i = ImGuiKey_1; i <= ImGuiKey_9; ++i) {
            const auto idx = (i - ImGuiKey_1);
            if (idx >= frame.texts.size()) {
               break;
            }

            if (ImGui::IsKeyPressed(i)) {
               scroll_to_idx_ = idx;
               break;
            }
         }

         if (scroll_to_idx_ == -1 && ImGui::IsKeyPressed(ImGuiKey_Space)) {
            scroll_to_idx_ = 0;
         }

         bool scrolled = false;
         for (std::size_t i = 0; i < frame.texts.size(); ++i) {
            const auto &entry = frame.texts[i];

            const float line_width = 8.0f;
            auto left = p.x + static_cast<float>(entry.left);
            auto bottom = p.y + static_cast<float>(entry.bottom);

            auto right = p.x + static_cast<float>(entry.right);
            auto top = p.y + static_cast<float>(entry.top);

            left -= line_width / 2.0f;
            top -= line_width / 2.0f;
            right += line_width / 2.0f;
            bottom += line_width / 2.0f;

            const auto color = confidence_to_color(entry.confidence);

            // Show tooltip if mouse is inside the text box (ImGui coordinates are reversed)
            const bool in_horizontal = (local_mouse_pos.x >= entry.left && local_mouse_pos.x <= entry.right);
            const bool in_vertical = (local_mouse_pos.y >= entry.top && local_mouse_pos.y <= entry.bottom);
            if (in_horizontal && in_vertical) {
               ImGui::SetTooltip("%s: %.2f", entry.text.c_str(), entry.confidence);
            }

            draw_list->AddRect(ImVec2(left, top), ImVec2(right, bottom), color, 0.1, 0, line_width);

            if (!scrolled && scroll_to_idx_ == i) {
               scroll_to_text_entry(entry);
               scroll_to_idx_ = -1;
               scrolled = true;
            }
         }

         draw_list->PopClipRect();
      } else {
         ImGui::Text("Error loading image...");
      }
   }

   ImGui::End();
}