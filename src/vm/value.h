#pragma once

#include <memory>
#include <unordered_map>
#include <numeric>
#include <variant>
#include <cmath>
#include <expected>
#include <span>
#include "../tosuto.h"

namespace tosuto::vm {
  struct chunk;
  struct fn_desc;

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

    struct function {
      std::shared_ptr<fn_desc> desc;
      std::shared_ptr<struct upvalue*[]> upvals;

      function();

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
      function,
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

  struct upvalue {
    value* loc;
    value closed = value{value::nil{}};
    upvalue* next = nullptr;

    inline explicit upvalue(value* loc) : loc(loc) {}
  };
}

//template<>
//struct std::hash<tosuto::vm::value> {
//  size_t operator()(tosuto::vm::value const& val) const {
//    switch (val.val.index()) {
//
//    }
//  }
//};
