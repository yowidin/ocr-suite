/**
 * @file   opengl2.h
 * @author Dennis Sitelew
 * @date   Jan. 14, 2021
 */
#ifndef INCLUDE_OCS_VIEWER_RENDER_IMPL_OPENGL2_H
#define INCLUDE_OCS_VIEWER_RENDER_IMPL_OPENGL2_H

#include <ocs/viewer/render/backend.h>

namespace ocs::viewer::render::impl {

class OpenGL2 : public backend {
public:
   using backend::backend;
   ~OpenGL2();

public:
   void init() override;
   void new_frame() override;
   void render() override;

   version get_version() override { return version::opengl2; }
};

} // namespace ocs::viewer::render::impl

#endif /* INCLUDE_OCS_VIEWER_RENDER_IMPL_OPENGL2_H */
