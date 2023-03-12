//
// Created by Dennis Sitelew on 22.01.23.
//

#include <ocs/ffmpeg/decoder.h>
#include <ocs/viewer/views/frame_view.h>

#include <imgui.h>

#include <spdlog/spdlog.h>

using namespace ocs::viewer::views;

// TODO: Improve navigation
//  - Better image movements
//    - Zoom
//    - Pan
//  - Jump no next/previous
//    - Frame
//    - Minute
//    - Hour
//    - Day

namespace {

// Simple helper function to load an image into a OpenGL texture with common settings
bool LoadTextureFromFile(const char *filename, GLuint *out_texture, int *out_width, int *out_height) {
   // Load from file
   int image_width = 0;
   int image_height = 0;
   unsigned char *image_data = stbi_load(filename, &image_width, &image_height, nullptr, 4);
   if (image_data == nullptr) {
      return false;
   }

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
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
   stbi_image_free(image_data);

   *out_texture = image_texture;
   *out_width = image_width;
   *out_height = image_height;

   return true;
}

} // namespace

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
   }
}

void frame_view::draw() {
   static uint32_t alpha_factor = 255;
   static bool decrease = true;
   const int step = 8;
   alpha_factor -= (decrease ? step : -step);
   if (alpha_factor < 64) {
      decrease = false;
      alpha_factor = 64;
   }

   if (alpha_factor > 255) {
      decrease = true;
      alpha_factor = 255;
   }

   ImGui::Begin(name());

   const auto p = ImGui::GetCursorScreenPos();

   auto draw_list = ImGui::GetForegroundDrawList();

   auto confidence_to_color = [&](const auto confidence) {
      auto r = static_cast<std::uint32_t>(255.0 * (1.0 - confidence));
      auto g = static_cast<std::uint32_t>(255.0 * (confidence));
      std::uint32_t result = 0x00000000;
      result = result | (alpha_factor << 24);
      result = result | (r << 16);
      result = result | (g << 8);
      return result;
   };

   if (!current_frame_) {
      ImGui::Text("Frame View...");
   } else {
      if (texture_handle_ != 0) {
         ImGui::Image(reinterpret_cast<void *>(texture_handle_),
                      ImVec2(static_cast<float>(texture_width_), static_cast<float>(texture_height_)));

         const auto &frame = current_frame_.value();

         for (const auto &entry : frame.texts) {
            const float line_width = 2.0f;
            auto left = p.x + static_cast<float>(entry.left);
            auto bottom = p.y + static_cast<float>(entry.bottom);

            auto right = p.x + static_cast<float>(entry.right);
            auto top = p.y + static_cast<float>(entry.top);

            left -= line_width / 2.0f;
            top -= line_width / 2.0f;
            right += line_width / 2.0f;
            bottom += line_width / 2.0f;

            const auto color = confidence_to_color(entry.confidence);

            draw_list->AddRect(ImVec2(left, top), ImVec2(right, bottom), color, 0.1, 0, line_width);
         }
      } else {
         ImGui::Text("Error loading image...");
      }
   }

   ImGui::End();
}