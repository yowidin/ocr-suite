//
// Created by Dennis Sitelew on 14.01.23.
//

#include <ocs/config.h>
#include <ocs/viewer/render/backend.h>
#include <ocs/viewer/render/frontend.h>

#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <imgui.h>

#include <iostream>
#include <memory>
#include <sstream>

using namespace ocs::viewer;

namespace {

void throw_sdl_error(const std::string &func) {
   throw std::runtime_error(func + " error: " + SDL_GetError());
}

void throw_errors(const char *context) {
   if (!glad_glGetError) {
      throw std::runtime_error(context);
   }

   std::ostringstream ostream;

   GLenum ec;
   while ((ec = glad_glGetError()) != GL_NO_ERROR) {
      ostream << "OpenGL error inside " << context << ": 0x" << std::hex << ec << "\n";
   }

   auto res = ostream.str();
   if (!res.empty()) {
      throw std::runtime_error(res);
   }
}

void glad_callback(const char *name, void *, int, ...) {
   throw_errors(name);
}

void consume_errors(std::string_view context) {
   GLenum ec;
   while ((ec = glad_glGetError()) != GL_NO_ERROR) {
      std::cout << "OpenGL error inside " << context << ": 0x" << std::hex << ec;
   }
}

} // namespace

int main(int argc, const char **const argv) {
   for (int i = 0; i < argc; ++i) {
      std::cout << "argv[" << i << "] = " << argv[i] << "\n";
   }
   bool stop_{false};

   SDL_Window *window_{nullptr};
   SDL_GLContext context_;
   SDL_Event event_{};
   std::unique_ptr<render::frontend> frontend_;
   ImVec4 clear_color_{0.45f, 0.55f, 0.60f, 1.00f};

   // Setup SDL
   if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      throw_sdl_error("SDL_Init");
   }

#if OCS_TARGET_OS(APPLE)
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#elif OCS_TARGET_OS(UNIX)
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#endif

   const bool full_screen = false;
   auto window_flags =
       (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);
   if (full_screen) {
      window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_FULLSCREEN);
   }

   int width = 1024;
   int height = 768;

   window_ =
       SDL_CreateWindow("OCS Viewer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, window_flags);

   context_ = SDL_GL_CreateContext(window_);
   if (!context_) {
      throw_sdl_error("SDL_GL_CreateContext");
   }

   if (SDL_GL_MakeCurrent(window_, context_) != 0) {
      throw_sdl_error("SDL_GL_MakeCurrent");
   }
   SDL_GL_SetSwapInterval(1); // Enable vsync

   // Initialize OpenGL loader
   if (gladLoadGL() == 0) {
      throw_errors("gladLoadGL");
      throw std::runtime_error("gladLoadGL failed");
   }

#ifdef GLAD_DEBUG
   glad_set_post_callback(&glad_callback);
#endif

   consume_errors("setup");

   int profile;
   SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
   const char *suffix = "";
   if (profile & SDL_GL_CONTEXT_PROFILE_ES) {
      suffix = " ES";
   }

   std::cout << "OpenGL version: " << GLVersion.major << "." << GLVersion.minor << suffix << std::endl;

   frontend_ = std::make_unique<render::frontend>(*window_, context_);

   consume_errors("imgui");

   auto draw = [&]() {
      ImGui::SetNextWindowPos({0, 0});
      ImGui::SetNextWindowSize({static_cast<float>(width), static_cast<float>(height)});

      ImGui::Begin("Main", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);

      // TODO: Actual draw
      ImGui::ShowDemoWindow();

      ImGui::End();
   };

   auto update = [&]() {
      while (SDL_PollEvent(&event_)) {
         frontend_->process_event(event_);
         if (event_.type == SDL_QUIT) {
            stop_ = true;
         } else if (event_.type == SDL_KEYUP) {
            if (event_.key.keysym.sym == SDLK_AC_BACK || event_.key.keysym.sym == SDLK_ESCAPE) {
               stop_ = true;
            }
         } else if (event_.type == SDL_WINDOWEVENT) {
            switch (event_.window.event) {
               case (SDL_WINDOWEVENT_RESIZED):
                  SDL_GetWindowSize(window_, &width, &height);
                  break;
            }
         }
      }

      if (ImGui::GetIO().WantTextInput) {
         SDL_StartTextInput();
      } else {
         SDL_StopTextInput();
      }

      frontend_->new_frame();

      auto &io = ImGui::GetIO();
      glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
      glClearColor(clear_color_.x, clear_color_.y, clear_color_.z, clear_color_.w);
      glClear(GL_COLOR_BUFFER_BIT);

      draw();

      frontend_->render();

      SDL_GL_SwapWindow(window_);
   };

   while (!stop_) {
      update();
   }

   frontend_.reset();

   if (context_ != nullptr) {
      SDL_GL_DeleteContext(context_);
      context_ = nullptr;
   }

   if (window_ != nullptr) {
      SDL_DestroyWindow(window_);
      window_ = nullptr;
   }

   return 0;
}