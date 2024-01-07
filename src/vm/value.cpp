#include <utility>
#include "value.h"

namespace ami::vm {
  std::string vm::value::to_string() const {
    switch (val.index()) {
      case 0: return std::to_string(get<num>());
      case 1: return std::to_string(get<offset>());
      case 2: return std::to_string(get<bool>());
      case 3: {
        std::string s = "{";
        auto& obj = get<object>();
        for (auto const& [k, v] : *obj) {
          s += k;
          s += "=";
          s += v.to_string();
          s += ", ";
        }

        if (s.back() != '}') s = s.substr(0, s.length() - 2);
        return s;
      }
      case 4: {
        return get<ref>()->to_string();
      }
      case 5: return "nil";
      case 6: return get<str>();
      default: std::unreachable();
    }
  }

  bool vm::value::is_truthy() const {
    switch (val.index()) {
      case 0: return fabs(get<num>()) > epsilon;
      case 2: return get<bool>();
      case 5: return false;
      default: return true;
    }
  }

  bool value::eq(const value& other) const {
    if (val.index() != other.val.index()) return false;
    switch (val.index()) {
      case 0: return fabs(get<num>() - other.get<num>()) < epsilon;
      case 2: return get<bool>() == other.get<bool>();
      case 5: return true;
      case 6: return get<str>() == other.get<str>();
      default: return false;
    }
  }
}