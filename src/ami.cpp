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

  std::unordered_map<std::string, size_t>* map;
  std::vector<std::string>* backing_array;

  interned_string::interned_string(const std::string& str) {
    static std::unordered_map<std::string, size_t> my_map;
    static std::vector<std::string> my_backing_array;
    static int run_once = []{map = &my_map; backing_array = &my_backing_array; return 1;}();
    auto it = my_map.find(str);
    if (it != my_map.end()) {
      index = it->second;
    } else {
      my_backing_array.emplace_back(str);
      index = my_backing_array.size() - 1;
      my_map.emplace(str, index);
    }
  }

  bool interned_string::operator==(const interned_string& other) const {
    return index == other.index;
  }

  interned_string interned_string::operator+(const interned_string& other) const {
    return interned_string{
      backing_array->at(index) + backing_array->at(index)};
  }

  interned_string::operator std::string const&() const {
    return backing_array->at(index);
  }

  std::vector<std::string> const& get_interned_string_backing_array() {
    return *backing_array;
  }

  pos pos::synthesized{0, 0, 0};
}

size_t std::hash<ami::interned_string>::operator()(
  const ami::interned_string& str) const noexcept {
  return hasher(str.index);
}