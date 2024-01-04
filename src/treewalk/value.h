#pragma once

#include <memory>
#include <expected>
#include <unordered_map>
#include <vector>
#include <functional>
#include <variant>
#include "parse.h"

namespace ami::tree {
  struct value;
  struct symbol_table;
  using value_ptr = std::shared_ptr<value>;
  using interpret_result = std::expected<value_ptr, std::string>;

  struct value : std::enable_shared_from_this<value> {
    struct nil {
    };

    using num = double;
    using object = std::unordered_map<std::string, value_ptr>;
    using array = std::vector<value_ptr>;
    using builtin_function =
      std::pair<std::function<interpret_result(
        symbol_table&)>, std::vector<std::pair<std::string, bool>>>;
    using function = fn_def_node*;
    using range = std::pair<value_ptr, value_ptr>;
    using range_iterator = std::pair<value_ptr, std::pair<value_ptr, value_ptr>>;
    using array_iterator = std::vector<value_ptr>::iterator;

    using value_type =
      std::variant<
        num,
        bool,
        std::string,
        object,
        array,
        function,
        builtin_function,
        range,
        range_iterator,
        array_iterator,
        nil>;

    static const size_t
      double_idx = 0,
      bool_idx = 1,
      string_idx = 2,
      object_idx = 3,
      array_idx = 4,
      function_idx = 5,
      builtin_function_idx = 6,
      range_idx = 7,
      range_iter_idx = 8,
      array_iter_idx = 9,
      nil_idx = 10;

    value_type val;

    std::unordered_map<std::string, value_ptr> decos;

    explicit value(value_type val, decltype(decos) decos = {});

    value_ptr copy();

    std::string type_name() const;

    interpret_result mod(value_ptr& other, symbol_table& sym);

    interpret_result div(value_ptr& other, symbol_table& sym);

    interpret_result mul(value_ptr& other, symbol_table& sym);

    interpret_result sub(value_ptr& other, symbol_table& sym);

    interpret_result add(value_ptr& other, symbol_table& sym);

    interpret_result eq(value_ptr& other, symbol_table& sym);

    interpret_result neq(value_ptr& other, symbol_table& sym);

    inline interpret_result has_deco(std::string const& name) const {
      return std::make_shared<value>(decos.contains(name));
    }

    inline interpret_result get_deco(std::string const& name) {
      return decos.contains(name) ? decos[name] : value::sym_nil;
    }

    interpret_result negate(), invert();

    template<typename T>
    inline bool is() const {
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

    bool is_truthy();

    std::expected<bool, std::string> has_next(symbol_table& sym);

    interpret_result next(symbol_table& sym);

    interpret_result iterator(symbol_table& sym);

    interpret_result set(std::string const& field, value_ptr const& value);

    interpret_result get(std::string const& field);

    interpret_result
    call(std::vector<value_ptr> const& args, symbol_table& sym);

    std::expected<std::vector<std::pair<std::string, bool>>, std::string>
    args();

    static value_ptr sym_nil, sym_false, sym_true, sym_one, sym_zero;

    interpret_result deref(symbol_table& sym);

    std::expected<std::string, std::string> display(symbol_table& sym);
  };
}