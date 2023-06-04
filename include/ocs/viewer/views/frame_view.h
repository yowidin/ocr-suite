//
// Created by Dennis Sitelew on 22.01.23.
//

#ifndef OCR_SUITE_FRAME_VIEW_H
#define OCR_SUITE_FRAME_VIEW_H

#include <ocs/viewer/views/drawable.h>
#include <ocs/viewer/views/search_results_view.h>

#include <ocs/ffmpeg/decoder.h>

#include <glad/glad.h>
#include <imgui.h>

#include <optional>

namespace ocs::viewer::views {

class frame_view : public drawable {
public:
   using frame_t = std::optional<search_results_view::frame>;

public:
   frame_view(search_results_view &search_results);

public:
   void set_current_frame(const frame_t &frame);

public:
   void draw() override;
   const char *name() const override { return "FrameView"; }

private:
   void load_image_from_frame();
   void make_texture(const ffmpeg::decoder::frame &decoded);

   void scroll_to_text_entry(const search_results_view::text &entry) const;

   void jump_to_next_frame();
   void jump_to_previous_frame();

   void jump_to_next_minute();
   void jump_to_previous_minute();

   void jump_to_next_hour();
   void jump_to_previous_hour();

   void jump_to_next_day();
   void jump_to_previous_day();

   void handle_scroll_to_text_hotkeys();
   void handle_jump_hotkeys();

   static bool is_shift_pressed();

private:
   frame_t current_frame_;
   search_results_view *search_results_view_;

   GLuint texture_handle_{0};
   int texture_width_{0};
   int texture_height_{0};

   bool was_dragging_{false};
   ImVec2 initial_scroll_position_{};

   ImVec2 view_port_size_;
   int scroll_to_idx_{-1};
};

} // namespace ocs::viewer::views

#endif // OCR_SUITE_FRAME_VIEW_H
