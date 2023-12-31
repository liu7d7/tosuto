#include <iostream>
#include "ami.h"

namespace ami {
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
}