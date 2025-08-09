/**
 * @file   opengl3.h
 * @author Dennis Sitelew
 * @date   Jan. 14, 2021
 */
#ifndef INCLUDE_OCS_VIEWER_RENDER_IMPL_OPENGL3_H
#define INCLUDE_OCS_VIEWER_RENDER_IMPL_OPENGL3_H

#include <ocs/viewer/render/backend.h>

namespace ocs::viewer::render::impl {

class OpenGL3 final : public backend {
public:
   using backend::backend;
   ~OpenGL3() override;

public:
   void init() override;
   void new_frame() override;
   void render() override;

   version get_version() override { return version::opengl3; }
};

} // namespace asp::render

#endif /* INCLUDE_OCS_VIEWER_RENDER_IMPL_OPENGL3_H */
