/**
 * @file   gles3.h
 * @author Dennis Sitelew
 * @date   Jan. 14, 2021
 */
#ifndef INCLUDE_OCS_VIEWER_RENDER_IMPL_GLES3_H
#define INCLUDE_OCS_VIEWER_RENDER_IMPL_GLES3_H

#include <ocs/viewer/render/backend.h>

namespace ocs::viewer::render::impl {

class GLES3 : public backend {
public:
   using backend::backend;
   ~GLES3();

public:
   void init() override;
   void new_frame() override;
   void render() override;

   version get_version() override { return version::gles3; }
};

} // namespace ocs::viewer::render::impl

#endif /* INCLUDE_OCS_VIEWER_RENDER_IMPL_GLES3_H */
