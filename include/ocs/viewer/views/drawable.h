//
// Created by Dennis Sitelew on 22.01.23.
//

#ifndef OCR_SUITE_DRAWABLE_H
#define OCR_SUITE_DRAWABLE_H

namespace ocs::viewer::views {

class drawable {
public:
   virtual ~drawable() = default;

public:
   virtual void draw() = 0;
   virtual const char *name() const = 0;
};

} // namespace ocs::viewer::views

#endif // OCR_SUITE_DRAWABLE_H
