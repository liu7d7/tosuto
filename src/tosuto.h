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

#define tosuto_unwrap(exp) *(temp_storage<decltype(exp)>::held = exp, temp_storage<decltype(exp)>::held.has_value() ? *temp_storage<decltype(exp)>::held : decltype(exp)({})); if (!temp_storage<decltype(exp)>::held.has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define tosuto_unwrap_move(exp) std::move(*(temp_storage<decltype(exp)>::held = std::move(exp), temp_storage<decltype(exp)>::held.has_value() ? std::move(*temp_storage<decltype(exp)>::held) : decltype(exp)({}))); if (!temp_storage<decltype(exp)>::held.has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define tosuto_discard(exp) if (!(temp_storage<decltype(exp)>::held = exp).has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define tosuto_dyn_cast(type, exp) temp_storage<type>::held = dynamic_cast<type>(exp); if (!temp_storage<type>::held) return std::unexpected("Unable to perform downcast!")

#ifdef NDEBUG
#define tosuto_unwrap_fast(exp) (*exp)
#define tosuto_unwrap_move_fast(exp) std::move(*exp)
#define tosuto_discard_fast(exp) void(exp)
#define tosuto_dyn_cast_fast(type, exp) dynamic_cast<type>(exp)
#else
#define tosuto_unwrap_fast(exp) *(temp_storage<decltype(exp)>::held = exp, temp_storage<decltype(exp)>::held.has_value() ? *temp_storage<decltype(exp)>::held : decltype(exp)({})); if (!temp_storage<decltype(exp)>::held.has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define tosuto_unwrap_move_fast(exp) std::move(*(temp_storage<decltype(exp)>::held = std::move(exp), temp_storage<decltype(exp)>::held.has_value() ? std::move(*temp_storage<decltype(exp)>::held) : decltype(exp)({}))); if (!temp_storage<decltype(exp)>::held.has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define tosuto_discard_fast(exp) if (!(temp_storage<decltype(exp)>::held = exp).has_value()) return std::unexpected(temp_storage<decltype(exp)>::held.error())
#define tosuto_dyn_cast_fast(type, exp) temp_storage<type>::held = dynamic_cast<type>(exp); if (!temp_storage<type>::held) return std::unexpected("Unable to perform downcast!")
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

namespace tosuto {
  using u8 = uint8_t;
  using u16 = uint16_t;
  using u32 = uint32_t;

  template<typename T>
  constexpr T max_of = std::numeric_limits<T>::max();

  template<typename T>
  constexpr T epsilon = std::numeric_limits<T>::epsilon();

  struct pos {
    size_t idx, col, row;

    [[nodiscard]] std::string to_string() const;
  };

  struct interned_string {
    size_t index;

    static std::unordered_map<std::string, size_t> map;
    static std::vector<std::pair<std::string, size_t>> backing_array;

    inline explicit interned_string(std::string const& str) {
      auto it = map.find(str);
      if (it != map.end()) {
        index = it->second;
      } else {
        backing_array.emplace_back(str, std::hash<std::string>()(str));
        index = backing_array.size() - 1;
        map.emplace(str, index);
      }
    }

    inline bool operator==(interned_string const& other) const {
      return index == other.index;
    }

    inline interned_string operator+(interned_string const& other) const {
      return interned_string{backing_array[index].first + backing_array[other.index].first};
    }

    inline operator std::string const&() const {
      return backing_array[index].first;
    }
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
    inline T& operator *() {
      return self;
    }
  };

  template<>
  struct dummy_expected<void> {
    inline void operator *() {
    }
  };
}

template<>
struct std::hash<tosuto::interned_string> {
  static constexpr auto hasher = std::hash<size_t>();
  size_t operator()(tosuto::interned_string const& str) const {
    return hasher(str.index);
  }
};