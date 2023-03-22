//
// Created by Dennis Sitelew on 22.01.23.
//

#ifndef OCR_SUITE_VIEWER_H
#define OCR_SUITE_VIEWER_H

#include <ocs/viewer/options.h>
#include <ocs/viewer/search.h>
#include <ocs/viewer/results.h>

#include <ocs/viewer/views/drawable.h>
#include <ocs/viewer/views/frame_view.h>
#include <ocs/viewer/views/search_results_view.h>
#include <ocs/viewer/views/search_view.h>

#include <ocs/viewer/render/window.h>

namespace ocs::viewer::views {

class viewer {
public:
   viewer(options opts);

public:
   void run();

private:
   void draw();
   void search_text_changed(const std::string &text);

private:
   options opts_;

   results search_results_;
   search db_;

   std::unique_ptr<render::window> window_{};

   frame_view frame_view_;
   search_view search_view_{};
   search_results_view search_results_view_;
};

} // namespace ocs::viewer::views

#endif // OCR_SUITE_VIEWER_H
