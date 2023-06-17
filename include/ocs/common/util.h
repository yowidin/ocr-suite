//
// Created by Dennis Sitelew on 26.12.22.
//

#pragma once

#include <algorithm>
#include <string>

namespace ocs::common::util {

// trim from start (in place)
inline void ltrim(std::string &s) {
   s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
inline void rtrim(std::string &s) {
   s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

// trim from both ends (in place)
inline void trim(std::string &s) {
   rtrim(s);
   ltrim(s);
}

} // namespace ocs::util
