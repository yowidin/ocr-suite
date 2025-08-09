/**
 * @file   gles2.h
 * @author Dennis Sitelew
 * @date   Jan. 14, 2023
 */
#ifndef INCLUDE_OCS_VIEWER_RENDER_IMPL_GLES2_H
#define INCLUDE_OCS_VIEWER_RENDER_IMPL_GLES2_H

#include <ocs/viewer/render/backend.h>

namespace ocs::viewer::render::impl {

class GLES2 final : public backend {
public:
   using backend::backend;
   ~GLES2() override;

public:
   void init() override;
   void new_frame() override;
   void render() override;

   version get_version() override { return version::gles2; }
};

} // namespace ocs::viewer::render::impl

#endif /* INCLUDE_OCS_VIEWER_RENDER_IMPL_GLES2_H */
