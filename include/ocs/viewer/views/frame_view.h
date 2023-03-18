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
   void set_current_frame(const frame_t &frame);

public:
   void draw() override;
   const char *name() const override { return "FrameView"; }

private:
   void load_image_from_frame();
   void make_texture(const ffmpeg::decoder::frame &decoded);
   void scroll_to_text_entry(const search_results_view::text &entry);

private:
   frame_t current_frame_;

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
