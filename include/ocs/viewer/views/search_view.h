//
// Created by Dennis Sitelew on 22.01.23.
//

#ifndef OCR_SUITE_SEARCH_VIEW_H
#define OCR_SUITE_SEARCH_VIEW_H

#include <ocs/viewer/views/drawable.h>

#include <functional>
#include <string>

namespace ocs::viewer::views {

class search_view : public drawable {
public:
   using text_change_cb_t = std::function<void(const std::string &)>;

public:
   void draw() override;
   const char *name() const override { return "Search"; }
   void set_text_change_cb(text_change_cb_t cb) { text_change_cb_ = std::move(cb); }

private:
   std::string search_text_;
   text_change_cb_t text_change_cb_;
};

} // namespace ocs::viewer::views

#endif // OCR_SUITE_SEARCH_VIEW_H
