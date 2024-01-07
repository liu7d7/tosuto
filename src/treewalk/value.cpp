#include <iostream>
#include <utility>
#include "value.h"
#include "interpret.h"

namespace tosuto::tree {
  value::value(value_type val, decltype(decos) decos) : val(std::move(val)), decos(std::move(decos)) {}

  std::string value::type_name() const {
    switch (val.index()) {
      case double_idx:return "number";
      case bool_idx:return "boolean";
      case string_idx:return "str";
      case object_idx:return "table";
      case array_idx:return "array";
      case function_idx:
      case builtin_function_idx:return "function";
      case range_idx:return "range";
      case range_iter_idx:return "range_iter";
      case array_iter_idx:return "array_iter";
      case nil_idx:return "nil";
    }

    std::unreachable();
  }

  std::expected<std::string, std::string> value::display(symbol_table& sym) {
    switch (val.index()) {
      case double_idx: {
        if (fabs(floor(get<double>()) - get<double>()) <
            std::numeric_limits<double>::epsilon()) {
          return std::to_string((long long) floor(get<double>()));
        }

        return std::to_string(get<double>());
      }
      case bool_idx:return std::to_string(get<bool>());
      case string_idx:return get<std::string>();
      case object_idx: {
        auto to_str = get("to_str");

        if (to_str.has_value()) {
          auto res = tosuto_unwrap(
            to_str.value()->call({shared_from_this()}, sym));
          if (!res->is<std::string>())
            return interpreter::fail("Expected str from to_str!");

          return res->get<std::string>();
        }

        std::string buf = "{";
        for (auto const& [k, v]: get<object>()) {
          buf += k;
          buf += '=';
          auto disp = tosuto_unwrap(v->display(sym));
          buf += disp;
          buf += ", ";
        }

        if (buf.back() != '{') buf = buf.substr(0, buf.length() - 2);
        return buf + '}';
      }
      case function_idx:
      case builtin_function_idx:return "function";
      case range_idx: {
        auto first = tosuto_unwrap(get<range>().first->display(sym));
        auto second = tosuto_unwrap(get<range>().second->display(sym));
        return first + ".." + second;
      }
      case range_iter_idx:
      case nil_idx:
      case array_iter_idx:return type_name();
    }

    std::unreachable();
  }

  interpret_result value::mod(value_ptr& other, symbol_table& sym) {
    // special case for if they're both numbers
    if (is<num>() && other->is<num>()) {
      return std::make_shared<value>(fmod(get<num>(), other->get<num>()));
    }

    auto mod = tosuto_unwrap(get("%"));
    if (!mod->is<function>() && !mod->is<builtin_function>()) {
      return interpreter::fail("Mod member not found on type " + type_name());
    }

    return mod->call({shared_from_this(), other}, sym);
  }

  interpret_result value::div(value_ptr& other, symbol_table& sym) {
    if (is<num>() && other->is<num>()) {
      return std::make_shared<value>(get<num>() / other->get<num>());
    }

    auto div = tosuto_unwrap(get("/"));
    if (!div->is<function>() && !div->is<builtin_function>()) {
      return interpreter::fail("Div member not found on type " + type_name());
    }

    return div->call({shared_from_this(), other}, sym);
  }

  interpret_result value::mul(value_ptr& other, symbol_table& sym) {
    if (is<num>() && other->is<num>()) {
      return std::make_shared<value>(get<num>() * other->get<num>());
    }

    auto mul = tosuto_unwrap(get("*"));
    if (!mul->is<function>() && !mul->is<builtin_function>()) {
      return interpreter::fail("Mul member not found on type " + type_name());
    }

    return mul->call({shared_from_this(), other}, sym);
  }

  interpret_result value::sub(value_ptr& other, symbol_table& sym) {
    if (is<num>() && other->is<num>()) {
      return std::make_shared<value>(get<num>() - other->get<num>());
    }

    auto sub = tosuto_unwrap(get("-"));
    if (!sub->is<function>() && !sub->is<builtin_function>()) {
      return interpreter::fail("Sub member not found on type " + type_name());
    }

    return sub->call({shared_from_this(), other}, sym);
  }

  interpret_result value::add(value_ptr& other, symbol_table& sym) {
    if (is<num>() && other->is<num>()) {
      return std::make_shared<value>(get<num>() + other->get<num>());
    }

    if (is<std::string>() && other->is<std::string>()) {
      return std::make_shared<value>(
        get<std::string>() + other->get<std::string>());
    }

    auto times = tosuto_unwrap(get("+"));
    if (!times->is<function>() && !times->is<builtin_function>()) {
      return interpreter::fail("Add member not found on type " + type_name());
    }

    return times->call({shared_from_this(), other}, sym);
  }

  interpret_result value::eq(value_ptr& other, symbol_table& sym) {
    if (is<num>() && other->is<num>()) {
      return std::make_shared<value>(fabs(get<num>() - other->get<num>()) <
                                     std::numeric_limits<double>::epsilon());
    }

    auto eq = tosuto_unwrap(get("=="));
    if (!eq->is<function>() && !eq->is<builtin_function>()) {
      return interpreter::fail("Eq member not found on type " + type_name());
    }

    return eq->call({shared_from_this(), other}, sym);
  }

  interpret_result value::invert() {
    return std::make_shared<value>(!is_truthy());
  }

  interpret_result value::neq(value_ptr& other, symbol_table& sym) {
    auto equ = tosuto_unwrap(eq(other, sym));
    return equ->invert();
  }

  interpret_result value::negate() {
    if (!is<num>()) return interpreter::fail("Can't negate a " + type_name());
    return std::make_shared<value>(-get<num>());
  }

  bool value::is_truthy() {
    if (is<bool>()) return get<bool>();
    if (is<num>()) return fabs(get<num>()) < std::numeric_limits<num>::epsilon();
    if (is<nil>()) return false;
    return true;
  }

  std::expected<bool, std::string> value::has_next(symbol_table& sym) {
    if (is<range_iterator>()) {
      auto it = get<range_iterator>();
      auto cont = tosuto_unwrap(it.first->neq(it.second.second, sym));
      return cont->is_truthy();
    }

    auto has_next = tosuto_unwrap(get("has_next"));
    auto res = tosuto_unwrap(has_next->call({shared_from_this()}, sym));

    return res->get<bool>();
  }

  interpret_result value::iterator(symbol_table& sym) {
    if (is<range>()) {
      auto ran = get<range>();
      return std::make_shared<value>(range_iterator{ran.first, ran});
    }

    if (is<array>()) {
      return std::make_shared<value>(get<array>().begin());
    }

    auto has_next = tosuto_unwrap(get("iterator"));
    auto res = tosuto_unwrap(has_next->call({shared_from_this()}, sym));

    return res;
  }

  interpret_result value::deref(symbol_table& sym) {
    if (is<range_iterator>()) {
      return get<range_iterator>().first;
    }

    if (is<array_iterator>()) {
      return *get<array_iterator>();
    }

    auto has_next = tosuto_unwrap(get("*deref"));
    auto res = tosuto_unwrap(has_next->call({shared_from_this()}, sym));

    return res;
  }

  interpret_result value::next(symbol_table& sym) {
    if (is<range_iterator>()) {
      auto& it = get<range_iterator>();
      auto next = tosuto_unwrap(it.first->add(sym_one, sym));
      it.first = next;
      return next;
    }

    if (is<array_iterator>()) {
      get<array_iterator>()++;
      return *get<array_iterator>();
    }

    auto next = tosuto_unwrap(get("next"));
    auto res = tosuto_unwrap(next->call({shared_from_this()}, sym));
    return res;
  }

  interpret_result
  value::set(std::string const& field, value_ptr const& value) {
    if (!is<object>())
      return interpreter::fail("Can't assign field to a " + type_name());

    return get<object>()[field] = value;
  }

  interpret_result value::get(std::string const& field) {
    if (!is<object>())
      return interpreter::fail("Can't assign field to a " + type_name());

    if (!get<object>().contains(field))
      return interpreter::fail(field + " not found in object!");

    return get<object>()[field];
  }

  value_ptr
    value::sym_nil = std::make_shared<value>(nil{}),
    value::sym_false = std::make_shared<value>(false),
    value::sym_true = std::make_shared<value>(true),
    value::sym_one = std::make_shared<value>(1.0),
    value::sym_zero = std::make_shared<value>(0.0);

  interpret_result
  value::call(std::vector<value_ptr> const& args, symbol_table& sym) {
    if (is<function>()) {
      if (get<function>()->args.size() != args.size())
        return interpreter::fail("Unmatching arity when calling functions!");

      interpreter interp;
      std::vector<std::pair<std::string, value_ptr>> arg_map;
      std::transform(
        args.begin(), args.end(),
        get<function>()->args.begin(),
        std::back_inserter(arg_map),
        [](value_ptr const& it,
           std::pair<std::string, bool> const& arg) {
          return std::make_pair(
            arg.first,
            !arg.second ? it->copy() : it);
        });

      symbol_table new_sym{std::unordered_map{arg_map.begin(), arg_map.end()},
                           &sym};

      return interp.interpret(get<function>()->body.get(), new_sym);
    }

    if (is<builtin_function>()) {
      if (get<builtin_function>().second.size() != args.size())
        return interpreter::fail("Unmatching arity when calling functions!");

      interpreter interp;
      std::vector<std::pair<std::string, value_ptr>> arg_map;
      std::transform(
        args.begin(), args.end(),
        get<builtin_function>().second.begin(),
        std::back_inserter(arg_map),
        [](value_ptr const& it,
           std::pair<std::string, bool> const& arg) {
          return std::make_pair(
            arg.first,
            !arg.second ? it->copy() : it);
        });
      symbol_table new_sym{std::unordered_map{arg_map.begin(), arg_map.end()},
                           &sym};

      return get<builtin_function>().first(new_sym);
    }

    auto call = tosuto_unwrap(get("()"));
    auto res = tosuto_unwrap(call->call(args, sym));
    return res;
  }

  std::expected<std::vector<std::pair<std::string, bool>>, std::string>
  value::args() {
    if (is<function>()) {
      return get<function>()->args;
    }

    if (is<builtin_function>()) {
      return get<builtin_function>().second;
    }

    return std::unexpected("Tried to get args from a non-function value!");
  }

  value_ptr value::copy() {
    return std::make_shared<value>(val, decos);
  }
}