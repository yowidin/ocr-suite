//
// Created by Dennis Sitelew on 14.01.23.
//

#ifndef OCR_SUITE_VIEWER_RENDER_WINDOW_H
#define OCR_SUITE_VIEWER_RENDER_WINDOW_H

#include <ocs/viewer/render/frontend.h>

#include <SDL2/SDL.h>
#include <imgui.h>

#include <functional>
#include <string>

namespace ocs::viewer::render {

class window {
public:
   struct options {
      int width{1024};
      int height{768};
      bool full_screen{false};

      std::string title;

      ImVec4 background{0.45f, 0.55f, 0.60f, 1.00f};
   };

   using draw_cb_t = std::function<void()>;

public:
   window(options opts, draw_cb_t cb);
   ~window();

public:
   [[nodiscard]] bool can_stop() const { return stop_; }
   void update();

   void set_title(const std::string &title);

   static window &instance();

private:
   void draw() const;

private:
   options options_;
   draw_cb_t draw_cb_;

   bool stop_{false};

   SDL_Window *window_{nullptr};
   SDL_GLContext context_;
   SDL_Event event_{};
   std::unique_ptr<render::frontend> frontend_;

   std::string title_{};
};

} // namespace ocs::viewer::render

#endif // OCR_SUITE_VIEWER_RENDER_WINDOW_H
