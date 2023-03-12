//
// Created by Dennis Sitelew on 14.01.23.
//

#include <ocs/viewer/options.h>

#include <ocs/ffmpeg/decoder.h>
#include <ocs/recognition/bmp.h>
#include <ocs/viewer/views/viewer.h>

#include <spdlog/spdlog.h>

#include <iostream>

using namespace ocs::viewer::render;
using namespace ocs::viewer::views;
using namespace ocs::viewer;

int main_checked(int argc, const char **const argv) {
   auto pres = options::parse(argc, argv);
   if (!pres) {
      return EXIT_FAILURE;
   }

   viewer v{pres.value()};
   v.run();

   return EXIT_SUCCESS;
}

int main(int argc, const char **const argv) {
   try {
      spdlog::set_level(spdlog::level::debug);
      spdlog::set_pattern("[%^%L%$][%t] %v");

      return main_checked(argc, argv);
   } catch (const std::exception &e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   } catch (...) {
      std::cerr << "Unknown error" << std::endl;
      return EXIT_FAILURE;
   }
}
