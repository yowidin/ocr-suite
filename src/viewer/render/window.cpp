//
// Created by Dennis Sitelew on 14.01.23.
//

#include <ocs/viewer/render/window.h>

#include <ocs/config.h>
#include <ocs/viewer/render/backend.h>
#include <ocs/viewer/render/frontend.h>

#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <imgui.h>

#include <iostream>
#include <memory>
#include <sstream>

using namespace ocs::viewer::render;

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

window::window(options opts, draw_cb_t cb)
   : options_{std::move(opts)}
   , draw_cb_{std::move(cb)} {
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

   auto window_flags =
       (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);
   if (options_.full_screen) {
      window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_FULLSCREEN);
   }

   window_ = SDL_CreateWindow(options_.title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, options_.width,
                              options_.height, window_flags);

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

   auto &io = ImGui::GetIO();
   io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
   io.ConfigDockingWithShift = true;

   consume_errors("imgui");
}

window::~window() {
   frontend_.reset();

   if (context_ != nullptr) {
      SDL_GL_DeleteContext(context_);
      context_ = nullptr;
   }

   if (window_ != nullptr) {
      SDL_DestroyWindow(window_);
      window_ = nullptr;
   }
}

void window::update() {
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
               SDL_GetWindowSize(window_, &options_.width, &options_.height);
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
   glClearColor(options_.background.x, options_.background.y, options_.background.z, options_.background.w);
   glClear(GL_COLOR_BUFFER_BIT);

   draw();

   frontend_->render();

   SDL_GL_SwapWindow(window_);
}

void window::draw() {
   //   ImGui::SetNextWindowSize({static_cast<float>(options_.width), static_cast<float>(options_.height)});
   const ImGuiViewport *viewport = ImGui::GetMainViewport();
   ImGui::SetNextWindowPos(viewport->WorkPos);
   ImGui::SetNextWindowSize(viewport->WorkSize);
   ImGui::SetNextWindowViewport(viewport->ID);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

   static bool p_open = true;
   ImGui::Begin("Main", &p_open,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                    ImGuiWindowFlags_NoNavFocus);

   ImGui::PopStyleVar(3);

   auto dock_space_id = ImGui::GetID("Workspace");
   static ImGuiDockNodeFlags dock_space_flags = ImGuiDockNodeFlags_None;
   ImGui::DockSpace(dock_space_id, ImVec2(0.0f, 0.0f), dock_space_flags);

   ImGui::End();

   draw_cb_();
}
