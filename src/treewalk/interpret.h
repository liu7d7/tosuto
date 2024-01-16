#pragma once

#include <memory>
#include <expected>
#include <unordered_map>
#include <vector>
#include <functional>
#include <variant>
#include "../parse.h"
#include "value.h"

namespace tosuto::tree {
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

    template<typename F>
    inline interpret_result find_first(F&& pred) {
      for (auto const& [k, v]: vals) {
        if (pred(k, v)) return v;
      }

      if (!par)
        return std::unexpected
          {"Failed to find something in the symbol table matching pred!"};

      return par->find_first(pred);
    }
  };

  struct interpreter {
    bool has_ret = false, has_next = false, has_break = false;

    void reset_state() {
      has_ret = false, has_next = false, has_break = false;
    }

    enum class block_context {
      global,
      function,
      for_loop,
      other
    };

    std::stack<block_context> blk_ctx;

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

    using interpret_fn = interpret_result(interpreter::*)(node*, symbol_table&);

    static std::unexpected<std::string>
    fail(std::string const& nod,
         std::source_location loc = std::source_location::current());

    std::expected<std::pair<value_ptr, symbol_table>, std::string>
    block_with_symbols(node* nod, symbol_table& sym);

    std::expected<symbol_table, std::string>
    global(node* nod, symbol_table& sym);

    interpret_result decorated(node* nod, symbol_table& sym);

    interpret_result reject(node*, symbol_table&);

    interpret_result kw_literal(node* nod, symbol_table&);
  };
}
