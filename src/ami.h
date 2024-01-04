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
#include <memory>

#define ami_unwrap(exp) *(temp_storage<decltype(exp)>::held = exp, temp_storage<decltype(exp)>::held.has_value() ? *temp_storage<decltype(exp)>::held : decltype(exp)({})); if (!temp_storage<decltype(exp)>::held.has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define ami_unwrap_move(exp) std::move(*(temp_storage<decltype(exp)>::held = std::move(exp), temp_storage<decltype(exp)>::held.has_value() ? std::move(*temp_storage<decltype(exp)>::held) : decltype(exp)({}))); if (!temp_storage<decltype(exp)>::held.has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define ami_discard(exp) if (!(temp_storage<decltype(exp)>::held = exp).has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define ami_dyn_cast(type, exp) temp_storage<type>::held = dynamic_cast<type>(exp); if (!temp_storage<type>::held) return std::unexpected("Unable to perform downcast!")

template<typename V>
struct temp_storage {
  static V held;
};

template<typename V>
V temp_storage<V>::held;

#include <stack>
#include <queue>
#include <ostream>

template<class Container, class Stream>
Stream& print_one_value_container
  (Stream& outputstream, const Container& container) {
  typename Container::const_iterator beg = container.begin();

  outputstream << "[";

  while (beg != container.end()) {
    outputstream << " " << *beg++;
  }

  outputstream << " ]";

  return outputstream;
}

template<class Type, class Container>
const Container& container(const std::stack<Type, Container>& stack) {
  struct hacked_stack : private std::stack<Type, Container> {
    static const Container& container
      (const std::stack<Type, Container>& stack) {
      return stack.*&hacked_stack::c;
    }
  };

  return hacked_stack::container(stack);
}

template < class Type, class Container >
const Container& container
  (const std::queue<Type, Container>& queue)
{
  struct HackedQueue : private std::queue<Type, Container>
  {
    static const Container& container
      (const std::queue<Type, Container>& queue)
    {
      return queue.*&HackedQueue::c;
    }
  };

  return HackedQueue::container(queue);
}

template
  < class Type
    , template <class OtherType, class Container = std::deque<OtherType> > class Adapter
    , class Stream
  >
Stream& operator<<
  (Stream& outputstream, const Adapter<Type>& adapter)
{
  return print_one_value_container(outputstream, container(adapter));
}

  namespace ami {
    using u8 = uint8_t;
    using u16 = uint16_t;

    struct pos {
      size_t idx, col, row;

      [[nodiscard]] std::string to_string() const;
    };

    template<typename T, typename S, typename Deleter>
    std::unique_ptr<T, Deleter>
    dynamic_uptr_cast(std::unique_ptr<S, Deleter>&& p) noexcept {
      auto converted = std::unique_ptr<T, Deleter>{dynamic_cast<T*>(p.get())};
      if (converted) {
        std::swap(converted.get_deleter(), p.get_deleter());
        p.release();            // no longer owns the pointer
      }
      return converted;
    }

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

    inline
    std::basic_string<char32_t, std::char_traits<char32_t>, std::allocator<char32_t>>
    to_utf32(const std::string& source) {
      std::wstring_convert<std::codecvt_utf8_utf16<char32_t>, char32_t> convertor;
      return convertor.from_bytes(source);
    }
  }