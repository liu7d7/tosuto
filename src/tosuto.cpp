#include <iostream>
#include "tosuto.h"

namespace tosuto {
  std::string pos::to_string() const {
    std::string buf = "pos{idx=";
    buf += std::to_string(idx);
    buf += ", col=";
    buf += std::to_string(col);
    buf += ", row=";
    buf += std::to_string(row);
    buf += "}";
    return buf;
  }

  std::unordered_map<std::string, size_t> interned_string::map;
  std::vector<std::pair<std::string, size_t>> interned_string::backing_array;
}