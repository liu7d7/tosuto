#pragma once

#include <memory>
#include <expected>
#include <unordered_map>
#include <vector>
#include <functional>
#include <variant>
#include "parse.h"
#include "value.h"

namespace ami {
  using namespace std::string_literals;

  std::string repeat(std::string const& input, size_t num);

  struct symbol_table {
    std::unordered_map<std::string, value_ptr> vals;
    symbol_table* par = nullptr;

    explicit symbol_table(
      std::unordered_map<std::string, value_ptr> vals,
      symbol_table* par = nullptr);

    symbol_table();

    interpret_result get(std::string const& name);

    void set(std::string const& name, value_ptr const& val);
  };

  struct interpreter {
    bool has_ret = false, has_next = false, has_break = false;

    void reset_state() {
      has_ret = false, has_next = false, has_break = false;
    }

    static std::unexpected<std::string>
    fail(node* nod, std::source_location loc = std::source_location::current());

    interpret_result interpret(node* nod, symbol_table& sym);

    interpret_result fn_def(node* nod, symbol_table& sym);

    interpret_result block(node* nod, symbol_table& sym);

    interpret_result call(node* nod, symbol_table& sym);

    interpret_result un_op(node* nod, symbol_table& sym);

    interpret_result assign(node* nod, value_ptr const& val, symbol_table& sym);

    interpret_result bin_op(node* nod, symbol_table& sym);

    interpret_result number(node* nod, symbol_table&);

    interpret_result string(node* nod, symbol_table&);

    interpret_result object(node* nod, symbol_table& sym);

    interpret_result field_get(node* nod, symbol_table& sym);

    interpret_result if_stmt(node* nod, symbol_table& sym);

    interpret_result ret(node* nod, symbol_table& sym);

    interpret_result do_nothing(node*, symbol_table&);

    interpret_result var_def(node* nod, symbol_table& sym);

    interpret_result range(node* nod, symbol_table& sym);

    interpret_result for_loop(node* nod, symbol_table& sym);

    interpret_result anon_fn_def(node* nod, symbol_table& sym);

    static std::unordered_map<
      node_type,
      interpret_result(interpreter::*)(node*, symbol_table&)> interpreters;

    static std::unexpected<std::string>
    fail(std::string const& nod,
         std::source_location loc = std::source_location::current());

    std::expected<std::pair<value_ptr, symbol_table>, std::string>
    block_with_symbols(node* nod, symbol_table& sym);

    std::expected<symbol_table, std::string>
    global(node* nod, symbol_table& sym);
  };
}
