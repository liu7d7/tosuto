#pragma once

#include <memory>
#include <unordered_map>
#include <numeric>
#include <variant>
#include <cmath>
#include "../tosuto.h"

namespace tosuto::vm {
  struct value {
    using num = double;
    constexpr static num epsilon = std::numeric_limits<num>::epsilon();

    using offset = size_t;
    using str = interned_string;
    using object = std::shared_ptr<std::unordered_map<str, value>>;
    using ref = std::shared_ptr<value>;
    struct nil {
    };

    using value_type = std::variant<
      num,
      offset,
      bool,
      object,
      ref,
      nil,
      str>;

    value_type val;

    template<typename T>
    inline bool is() {
      return std::holds_alternative<T>(val);
    }

    template<typename T>
    inline T& get() {
      return std::get<T>(val);
    }

    template<typename T>
    inline T const& get() const {
      return std::get<T>(val);
    }

    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] bool is_truthy() const;

    [[nodiscard]] bool eq(value const& other) const;
  };
}