/**
 * @file   backend.cpp
 * @author Dennis Sitelew
 * @date   Jan. 14, 2021
 */

#include <ocs/viewer/render/backend.h>
#include <ocs/viewer/render/impl/gles2.h>
#include <ocs/viewer/render/impl/gles3.h>
#include <ocs/viewer/render/impl/opengl2.h>
#include <ocs/viewer/render/impl/opengl3.h>

#include <stdexcept>
#include <string>

using namespace ocs::viewer::render;

std::unique_ptr<backend> backend::make(version v, frontend &f) {
   switch (v) {
      case version::opengl2:
         return std::make_unique<impl::OpenGL2>(f);

      case version::opengl3:
         return std::make_unique<impl::OpenGL3>(f);

      case version::gles2:
         return std::make_unique<impl::GLES2>(f);

      case version::gles3:
         return std::make_unique<impl::GLES3>(f);
   }

   throw std::runtime_error("Unexpected GL Version: " + std::to_string(static_cast<int>(v)));
}
