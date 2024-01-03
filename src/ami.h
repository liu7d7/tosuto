#pragma once

#include <cstddef>
#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <utility>
#include <expected>
#include <source_location>
#include <variant>
#include <functional>
#include <optional>
#include <cmath>
#include <locale>
#include <codecvt>

#define ami_unwrap(exp) *(temp_storage<decltype(exp)>::held = exp, temp_storage<decltype(exp)>::held.has_value() ? *temp_storage<decltype(exp)>::held : decltype(exp)({})); if (!temp_storage<decltype(exp)>::held.has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define ami_discard(exp) if (!(temp_storage<decltype(exp)>::held = exp).has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define ami_dyn_cast(type, exp) temp_storage<type>::held = dynamic_cast<type>(exp); if (!temp_storage<type>::held) return std::unexpected("Unable to perform downcast!")

template<typename V>
struct temp_storage {
  static V held;
};

template<typename V>
V temp_storage<V>::held;

namespace ami {
  struct pos {
    size_t idx, col, row;

    [[nodiscard]] std::string to_string() const;
  };

  template<typename T>
  std::string to_utf8(
    const std::basic_string<T, std::char_traits<T>, std::allocator<T>>& source) {
    std::string result;

    std::wstring_convert<std::codecvt_utf8_utf16<T>, T> convertor;
    result = convertor.to_bytes(source);

    return result;
  }

  inline
  std::basic_string<char16_t, std::char_traits<char16_t>, std::allocator<char16_t>>
  to_utf16(const std::string& source) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convertor;
    return convertor.from_bytes(source);
  }
}