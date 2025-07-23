/**
 * @file   ocr-results-viewer.cpp
 * @author Dennis Sitelew
 * @date   Jul. 21, 2025
 */

#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION

#include <ocs/common/ocr_result.h>
#include <ocs/viewer/render/window.h>
#include <ocs/viewer/views/util.h>

#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <boost/filesystem.hpp>
#include <lyra/lyra.hpp>
#include <nlohmann/json.hpp>

#include <glad/glad.h>

#include <dejavu.h>

#include <fstream>
#include <iostream>
#include <utility>

#include "ocs/recognition/options.h"

using namespace ocs::viewer::render;
using json = nlohmann::json;

ImVec2 operator+(const ImVec2 &lhs, const ImVec2 &rhs) {
   return {lhs.x + rhs.x, lhs.y + rhs.y};
}

ImVec2 operator-(const ImVec2 &lhs, const ImVec2 &rhs) {
   return {lhs.x - rhs.x, lhs.y - rhs.y};
}

ImVec2 operator/(const ImVec2 &lhs, const float rhs) {
   return {lhs.x / rhs, lhs.y / rhs};
}

struct texture {
   texture(const std::uint8_t *data, const int width, const int height)
      : width{width}
      , height{height}

   {
      // Create a OpenGL texture identifier
      glGenTextures(1, &handle);
      glBindTexture(GL_TEXTURE_2D, handle);

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
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

      GLenum ec;
      while ((ec = glad_glGetError()) != GL_NO_ERROR) {
         spdlog::warn("OpenGL error inside {}: 0x{:x}", "texture()", ec);
      }
   }

   ~texture() {
      if (handle != 0) {
         glDeleteTextures(1, &handle);
      }
   }

   [[nodiscard]] float fwidth() const { return static_cast<float>(width); }

   [[nodiscard]] float fheight() const { return static_cast<float>(height); }

public:
   GLuint handle{0};
   int width;
   int height;
};

struct options {
   static std::optional<options> parse(int argc, const char **argv) {
      auto cli = lyra::cli{};

      options result{};

      bool show_help{false};

      cli.add_argument(lyra::opt(result.image_file, "image_path")
                           .name("-i")
                           .name("--image")
                           .required()
                           .help("Image to display the OCR results for."));

      cli.add_argument(lyra::opt(result.json_file, "json_path")
                           .name("-j")
                           .name("--json")
                           .required()
                           .help("JSON file containing the OCR results."));

      cli.add_argument(lyra::help(show_help));

      if (const auto parse_result = cli.parse({argc, argv}); !parse_result) {
         std::cerr << "Error in command line: " << parse_result.message() << std::endl;
         std::cerr << cli << std::endl;
         return std::nullopt;
      }

      if (!boost::filesystem::exists(result.image_file)) {
         std::cerr << "Image file does not exist: " << result.image_file << std::endl;
         return std::nullopt;
      }

      if (!boost::filesystem::exists(result.json_file)) {
         std::cerr << "JSON file does not exist: " << result.json_file << std::endl;
         return std::nullopt;
      }

      if (show_help) {
         std::cerr << cli << std::endl;
         return {};
      }

      return result;
   }

   std::string image_file{};
   std::string json_file{};
};

class viewer {
public:
   explicit viewer(options opts)
      : opts_{std::move(opts)} {
      window::options win_opts{};
      win_opts.title = "OCR results";
      window_ = std::make_unique<window>(win_opts, [this] { draw(); });

      const auto io = ImGui::GetIO();
      constexpr auto font_height = 16.0f;
      io.Fonts->AddFontFromMemoryCompressedBase85TTF(DejaVu_compressed_data_base85, font_height, nullptr,
                                                     io.Fonts->GetGlyphRangesCyrillic());
   }

public:
   void run() {
      load_json();
      load_image();

      while (!window_->can_stop()) {
         window_->update();
      }
   }

private:
   void load_json() {
      ocr_result_.frame_number = 0;

      std::ifstream json_file(opts_.json_file);
      json data = json::parse(json_file);

      if (!data.contains("entries")) {
         throw std::runtime_error("Invalid JSON file: missing \"entries\"");
      }

      auto &entries = data["entries"];

      for (const auto &entry : entries) {
         auto ensure_field = [&](auto js, auto name) {
            if (js.contains(name)) {
               return js[name];
            }

            const auto counter = ocr_result_.entries.size();
            throw std::runtime_error(fmt::format("Invalid JSON file: missing \"{}\" at #{}", name, counter));
         };

         ocs::common::text_entry te;
         te.text = ensure_field(entry, "text").get<std::string>();

         // Make confidence optional
         if (entry.contains("confidence")) {
            te.confidence = entry["confidence"].get<float>();
         } else {
            te.confidence = 100.0f;
         }

         auto pos = ensure_field(entry, "position");
         te.left = ensure_field(pos, "left").get<int>();
         te.top = ensure_field(pos, "top").get<int>();
         te.right = ensure_field(pos, "right").get<int>();
         te.bottom = ensure_field(pos, "bottom").get<int>();

         ocr_result_.entries.emplace_back(te);
      }
   }

   void load_image() {
      int x, y, n;
      const auto data = stbi_load(opts_.image_file.c_str(), &x, &y, &n, 0);
      if (!data) {
         throw std::runtime_error(fmt::format("Failed to load image: {}", stbi_failure_reason()));
      }

      texture_ = std::make_unique<texture>(data, x, y);

      stbi_image_free(data);
   }

   void draw() {
      const ImGuiID dock_space_id = ImGui::GetID("Workspace");

      static bool dock_configured = false;
      if (!dock_configured) {
         ImGui::DockBuilderRemoveNode(dock_space_id); // Clear out existing layout
         ImGui::DockBuilderAddNode(dock_space_id);    // Add empty node

         ImGuiID frame;
         const ImGuiID entries = ImGui::DockBuilderSplitNode(dock_space_id, ImGuiDir_Left, 0.20f, nullptr, &frame);

         ImGui::DockBuilderDockWindow("Entries", entries);
         ImGui::DockBuilderDockWindow("Frame", frame);

         ImGui::DockBuilderGetNode(entries)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
         ImGui::DockBuilderGetNode(frame)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

         ImGui::DockBuilderFinish(dock_space_id);

         dock_configured = true;
      }

      const ImGuiViewport *viewport = ImGui::GetMainViewport();
      ImGui::DockBuilderSetNodeSize(dock_space_id, viewport->WorkSize);
      ImGui::DockBuilderSetNodePos(dock_space_id, viewport->WorkPos);

      draw_entries_list();
      draw_frame();
   }

   void draw_entries_list() {
      ocs::viewer::views::util::window w{"Entries", ImGuiWindowFlags_HorizontalScrollbar};

      ImGui::BeginListBox("##empty", ImVec2{-FLT_MIN, -FLT_MIN});

      int i = 0;
      for (const auto &entry : ocr_result_.entries) {
         if (ImGui::Selectable(entry.text.c_str(), i == selected_entry_)) {
            selected_entry_ = i;
            scrolled_ = false;
         }

         ++i;
      }

      ImGui::EndListBox();
   }

   void draw_frame() {
      ocs::viewer::views::util::window w{"Frame", ImGuiWindowFlags_HorizontalScrollbar};

      // Get the canvas-related variables before we start drawing anything
      const auto image_region = ImGui::GetContentRegionAvail();
      const auto cursor_pos = ImGui::GetCursorScreenPos();
      const auto draw_list = ImGui::GetForegroundDrawList();

      ImGui::Image(reinterpret_cast<void *>(texture_->handle), ImVec2(texture_->fwidth(), texture_->fheight()));

      handle_drag();
      handle_selection_change(image_region);
      draw_text_boxes(*draw_list, cursor_pos, image_region);
   }

   void draw_text_boxes(ImDrawList &draw_list, const ImVec2 &cursor_pos, const ImVec2 &image_region) {
      // "Interpolate" color between green and red (passing through yellow) - the higher value - the redder
      auto lerp_color = [&](const uint16_t value, const uint16_t max = 255) -> ImU32 {
         uint8_t r, g;
         const double dStep = 510.0 / max;
         const double dValue = dStep * value;
         if (dValue <= 255.0) {
            g = 255;
            r = static_cast<uint8_t>(dValue);
         } else {
            g = static_cast<uint8_t>(510.0 - dValue);
            r = 255;
         }
         return ImColor(r, g, 0, 255);
      };

      auto confidence_to_color = [&](const auto confidence) {
         // We want "greener" color for higher confidence
         auto reverted = 100.0 - confidence;
         return lerp_color(reverted, 100);
      };

      const auto local_mouse_pos = ImGui::GetMousePos() - cursor_pos;

      const auto scroll_position = ImVec2{ImGui::GetScrollX(), ImGui::GetScrollY()};
      const auto clip_min = cursor_pos + scroll_position;
      const auto clip_max = clip_min + image_region;
      draw_list.PushClipRect(clip_min, clip_max, true);

      for (std::size_t i = 0; i < ocr_result_.entries.size(); ++i) {
         const auto &entry = ocr_result_.entries[i];

         constexpr auto line_width = 2.0f;
         constexpr ImVec2 line_vec{line_width / 2.0f, line_width / 2.0f};
         const auto left = static_cast<float>(entry.left);
         const auto bottom = static_cast<float>(entry.bottom);

         const auto right = static_cast<float>(entry.right);
         const auto top = static_cast<float>(entry.top);

         const bool in_horizontal = (local_mouse_pos.x >= left && local_mouse_pos.x <= right);
         const bool in_vertical = (local_mouse_pos.y >= top && local_mouse_pos.y <= bottom);

         if (in_horizontal && in_vertical) {
            ImGui::SetTooltip("%s: %.2f", entry.text.c_str(), entry.confidence);
         }

         const auto top_left = (cursor_pos + ImVec2{left, top}) - line_vec;
         const auto bottom_right = (cursor_pos + ImVec2{right, bottom}) + line_vec;

         const auto color = confidence_to_color(entry.confidence);
         int flags = 0;
         if (selected_entry_ == i) {
            flags = ImDrawFlags_RoundCornersAll;
         }
         draw_list.AddRect(top_left, bottom_right, color, 0.1, flags, line_width);
      }

      draw_list.PopClipRect();
   }

   static void handle_drag() {
      static bool dragging = false;
      static ImVec2 scroll_position{};
      if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
         ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

         if (!dragging) {
            scroll_position.x = ImGui::GetScrollX();
            scroll_position.y = ImGui::GetScrollY();
            dragging = true;
         }

         const auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
         ImGui::SetScrollX(scroll_position.x - delta.x);
         ImGui::SetScrollY(scroll_position.y - delta.y);
      } else {
         dragging = false;
      }
   }

   void handle_selection_change(const ImVec2 &image_region) {
      if (selected_entry_ < 0) {
         return;
      }

      const auto entry = ocr_result_.entries[selected_entry_];
      if (!scrolled_) {
         scroll_to_text_entry(image_region, entry);
         scrolled_ = true;
      }
   }

   static void scroll_to_text_entry(const ImVec2 &image_region, const ocs::common::text_entry &entry) {
      const auto entry_top_left = ImVec2{static_cast<float>(entry.left), static_cast<float>(entry.top)};
      const auto entry_bottom_right = ImVec2{static_cast<float>(entry.right), static_cast<float>(entry.bottom)};

      auto entry_half_size = (entry_bottom_right - entry_top_left) / 2.0f;
      entry_half_size.x = std::abs(entry_half_size.x);
      entry_half_size.y = std::abs(entry_half_size.y);

      const auto view_port_middle = image_region / 2.0f;
      const auto entry_middle = entry_top_left + entry_half_size;

      const auto scroll_target = entry_middle - view_port_middle;

      ImGui::SetScrollX(scroll_target.x);
      ImGui::SetScrollY(scroll_target.y);
   }

private:
   options opts_;

   std::unique_ptr<window> window_{};
   ocs::common::ocr_result ocr_result_{};
   std::unique_ptr<texture> texture_{};

   int selected_entry_ = -1;
   bool scrolled_ = false;
};

int main_checked(const int argc, const char **argv) {
   const auto maybe_options = options::parse(argc, argv);

   if (!maybe_options) {
      return EXIT_FAILURE;
   }

   const auto &options = *maybe_options;

   viewer v{options};
   v.run();

   return EXIT_SUCCESS;
}

int main(const int argc, char **argv) {
   try {
      spdlog::set_level(spdlog::level::debug);
      spdlog::set_pattern("[%^%L%$][%t] %v");

      // srsly msvc, const cast?
      return main_checked(argc, const_cast<const char **>(argv));
   } catch (const std::exception &e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   } catch (...) {
      std::cerr << "Unknown error" << std::endl;
      return EXIT_FAILURE;
   }
}
