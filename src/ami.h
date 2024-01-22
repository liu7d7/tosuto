#pragma once

#include <cstddef>
#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>
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

#ifdef NDEBUG
#define ami_unwrap_fast(exp) (*exp)
#define ami_unwrap_move_fast(exp) std::move(*exp)
#define ami_discard_fast(exp) void(exp)
#define ami_dyn_cast_fast(type, exp) dynamic_cast<type>(exp)
#else
#define ami_unwrap_fast(exp) ami_unwrap(exp)
#define ami_unwrap_move_fast(exp) ami_unwrap_move(exp)
#define ami_discard_fast(exp) ami_discard(exp)
#define ami_dyn_cast_fast(type, exp) ami_dyn_cast(type, exp)
#endif

template<typename V>
struct temp_storage {
  static V held;
};

template<typename V>
V temp_storage<V>::held;

#include <stack>
#include <queue>
#include <ostream>

template<typename T>
concept printable = requires(T const& a) {
  { a.to_string() } -> std::same_as<std::string>;
};

template<typename T, typename Stream>
requires printable<T>
Stream& operator<<(Stream& outputstream, const T& adapter) {
  outputstream << adapter.to_string();
  return outputstream;
}

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

template<class Type, class Container>
const Container& container
  (const std::queue<Type, Container>& queue) {
  struct HackedQueue : private std::queue<Type, Container> {
    static const Container& container
      (const std::queue<Type, Container>& queue) {
      return queue.*&HackedQueue::c;
    }
  };

  return HackedQueue::container(queue);
}

template
  <class Type, template<class OtherType, class Container = std::deque<OtherType> > class Adapter, class Stream
  >
Stream& operator<<
  (Stream& outputstream, const Adapter<Type>& adapter) {
  return print_one_value_container(outputstream, container(adapter));
}

template<class Type, class Stream>
Stream& operator<<
  (Stream& outputstream, const std::vector<Type>& adapter) {
  return print_one_value_container(outputstream, adapter);
}

namespace ami {
  using u8 = uint8_t;
  using u16 = uint16_t;
  using u32 = uint32_t;

  template<typename T>
  constexpr T max_of = std::numeric_limits<T>::max();

  struct pos {
    size_t idx, col, row;

    [[nodiscard]] std::string to_string() const;

    static pos synthesized;
  };

  struct interned_string {
    size_t index;

    explicit interned_string(std::string const& str);

    bool operator==(interned_string const& other) const;

    interned_string operator+(interned_string const& other) const;

    /* implicit */ operator std::string const&() const;
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

  inline
  std::basic_string<char32_t, std::char_traits<char32_t>, std::allocator<char32_t>>
  to_utf32(const std::string& source) {
    std::wstring_convert<std::codecvt_utf8_utf16<char32_t>, char32_t> convertor;
    return convertor.from_bytes(source);
  }

  template<typename T>
  struct dummy_expected {
    T self;

    explicit dummy_expected(T&& thing) : self(std::move(thing)) {}

    inline T& operator*() {
      return self;
    }
  };

  template<>
  struct dummy_expected<void> {
    inline void operator*() {
    }
  };

  std::vector<std::string> const& get_interned_string_backing_array();
}

template<>
struct std::hash<ami::interned_string> {
  static constexpr auto hasher = std::hash<size_t>();

  size_t operator()(ami::interned_string const& str) const noexcept;
};
