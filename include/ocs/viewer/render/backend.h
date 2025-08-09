/**
 * @file   backend.h
 * @author Dennis Sitelew
 * @date   Jan. 14, 2023
 */
#ifndef INCLUDE_OCS_VIEWER_RENDER_BACKEND_H
#define INCLUDE_OCS_VIEWER_RENDER_BACKEND_H

#include <memory>

namespace ocs::viewer::render {

enum class version { opengl2 = 2, opengl3 = 3, gles2 = 20, gles3 = 30 };

class frontend;

class backend {
public:
   explicit backend(frontend &v)
      : frontend_{&v} {}

   virtual ~backend() = default;

public:
   virtual void init() = 0;
   virtual void new_frame() = 0;
   virtual void render() = 0;
   virtual version get_version() = 0;

   static std::unique_ptr<backend> make(version v, frontend &f);

protected:
   frontend *frontend_;
};

} // namespace ocs::viewer::render

#endif /* INCLUDE_OCS_VIEWER_RENDER_BACKEND_H */
