#include <utility>
#include "value.h"
#include "vm.h"

namespace ami::vm {
  std::string value::to_string() const {
    switch (val.index()) {
      case 0: return std::to_string(get<num>());
      case 1: return std::to_string(get<bool>());
      case 2: {
        std::string s = "{";
        auto& obj = get<object>();
        for (auto const& [k, v]: *obj) {
          s += k;
          s += "=";
          s += v.to_string();
          s += ", ";
        }

        if (s.back() != '{') s = s.substr(0, s.length() - 2);
        return s + '}';
      }
      case 3: {
        return get<ref>()->to_string();
      }
      case 4: return "nil";
      case 5: return get<str>();
      case 6: return "<function " + std::string(get<function>().desc->chunk.name) + ">";
      case 7: return "<native function>";
      case 8: {
        std::string s = "[";
        auto& obj = get<array>();
        for (auto const& v: *obj) {
          s += v.to_string();
          s += ", ";
        }

        if (s.back() != '[') s = s.substr(0, s.length() - 2);
        return s + ']';
      }
      case 9: {
        return "upvalue";
      }
      default: std::unreachable();
    }
  }

  bool value::is_truthy() const {
    switch (val.index()) {
      case 1: return get<bool>();
      case 4: return false;
      default: return true;
    }
  }

  bool value::eq(const value& other) const {
    if (val.index() != other.val.index()) return false;
    switch (val.index()) {
      case 0: return fabs(get<num>() - other.get<num>()) < epsilon;
      case 1: return get<bool>() == other.get<bool>();
      case 4: return true;
      case 5: return get<str>() == other.get<str>();
      default: return false;
    }
  }

  value::function::function() : desc{std::make_shared<fn_desc>(chunk{}, 0xff, std::nullopt)}, upvals{nullptr} {

  }
}