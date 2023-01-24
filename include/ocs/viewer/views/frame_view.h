//
// Created by Dennis Sitelew on 22.01.23.
//

#ifndef OCR_SUITE_FRAME_VIEW_H
#define OCR_SUITE_FRAME_VIEW_H

#include <ocs/viewer/views/drawable.h>

namespace ocs::viewer::views {

class frame_view : public drawable {
public:
   void draw() override;
   const char *name() const override { return "FrameView"; }
};

} // namespace ocs::viewer::views

#endif // OCR_SUITE_FRAME_VIEW_H
