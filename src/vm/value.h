#pragma once

#include <memory>
#include <unordered_map>
#include <numeric>
#include <variant>
#include <cmath>
#include <span>
#include "../tosuto.h"

namespace tosuto::vm {
  struct chunk;

  struct value {
    using num = double;
    constexpr static num epsilon = std::numeric_limits<num>::epsilon();

    using str = interned_string;
    using object = std::shared_ptr<std::unordered_map<str, value>>;
    using ref = std::shared_ptr<value>;
    using native_fn = std::pair<std::expected<value, std::string>(*)(std::span<value> args), u8>;
    using array = std::shared_ptr<std::vector<value>>;
    struct nil {
    };

    struct fn {
      u8 arity;
      std::shared_ptr<chunk> ch;
      std::string name;
      std::vector<bool> ref;

      fn();

      enum class type {
        script,
        fn
      };
    };

    using value_type = std::variant<
      num,
      bool,
      object,
      ref,
      nil,
      str,
      fn,
      native_fn,
      array>;

    value_type val;

    template<typename T>
    [[nodiscard]] inline bool is() const {
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