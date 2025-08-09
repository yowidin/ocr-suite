/**
 * @file   frontend.h
 * @author Dennis Sitelew
 * @date   Jan. 14, 2021
 */
#ifndef INCLUDE_OCS_VIEWER_RENDER_FRONTEND_H
#define INCLUDE_OCS_VIEWER_RENDER_FRONTEND_H

#include <SDL2/SDL.h>

#include <memory>
#include <string>

namespace ocs::viewer::render {

class backend;

class frontend {
public:
   frontend(SDL_Window &window, SDL_GLContext context);
   ~frontend();

public:
   void new_frame() const;
   void render() const;
   static void process_event(const SDL_Event &event);

   [[nodiscard]] const std::string &shader_version() const { return shader_version_; }

   [[nodiscard]] SDL_Window &window() const { return *window_; }

private:
   SDL_Window *window_;
   std::unique_ptr<backend> backend_;
   std::string shader_version_;
};

} // namespace ocs::viewer::render

#endif /* INCLUDE_OCS_VIEWER_RENDER_FRONTEND_H */
