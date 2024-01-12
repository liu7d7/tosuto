#include <iostream>
#include "interpret.h"

namespace tosuto::tree {
  interpreter::interpret_fn interpreters[] {
    &interpreter::fn_def,
    &interpreter::block,
    &interpreter::call,
    &interpreter::un_op,
    &interpreter::bin_op,
    &interpreter::number,
    &interpreter::string,
    &interpreter::object,
    &interpreter::field_get,
    &interpreter::if_stmt,
    &interpreter::ret,
    &interpreter::do_nothing,
    &interpreter::do_nothing,
    &interpreter::var_def,
    &interpreter::range,
    &interpreter::for_loop,
    &interpreter::anon_fn_def,
    &interpreter::reject,
    &interpreter::decorated,
    &interpreter::kw_literal
  };

  std::string repeat(std::string const& input, size_t num) {
    std::string ret;
    ret.reserve(input.size() * num);
    while (num--)
      ret += input;
    return ret;
  }

  symbol_table::symbol_table(std::unordered_map<std::string, value_ptr> vals,
                             symbol_table* par) : vals(std::move(vals)),
                                                  par(par) {}

  interpret_result symbol_table::get(std::string const& name) {
    auto it = vals.find(name);
    if (it != vals.end()) return it->second;
    if (!par) return std::unexpected("Failed to find " + name);
    return par->get(name);
  }

  void symbol_table::set(std::string const& name, value_ptr const& val) {
    vals[name] = val;
  }

  symbol_table::symbol_table() : par(nullptr), vals() {
  }

  std::unexpected<std::string>
  interpreter::fail(node* nod, std::source_location loc) {
    return std::unexpected(
      "Failed to interpret " + nod->pretty(0) + " in " + loc.function_name() +
      " on line " + std::to_string(loc.line()));
  }

  std::unexpected<std::string>
  interpreter::fail(std::string const& nod, std::source_location loc) {
    return std::unexpected(
      nod + "\nin " + loc.function_name() +
      " on line " + std::to_string(loc.line()));
  }

  interpret_result interpreter::fn_def(node* nod, symbol_table& sym) {
    auto it = static_cast<fn_def_node*>(nod);

    sym.set(it->name, std::make_shared<value>(it));
    return value::sym_nil;
  }

  std::expected<std::pair<value_ptr, symbol_table>, std::string>
  interpreter::block_with_symbols(node* nod, symbol_table& sym) {
    auto it = static_cast<block_node*>(nod);

    symbol_table new_sym{{}, &sym};

    value_ptr val;
    for (auto const& stmt: it->exprs) {
      val = tosuto_unwrap_move(interpret(stmt.get(), sym));
      if (has_ret) {
        switch (blk_ctx.top()) {
          case block_context::global: {
            return fail("has_ret detected in global context");
          }
          case block_context::function: {
            has_ret = false;
            goto end;
          }
          case block_context::for_loop:
          case block_context::other: {
            goto end;
          }
        }
      }

      if (has_next) {
        switch (blk_ctx.top()) {
          case block_context::global: {
            return fail("has_next detected in global context");
          }
          case block_context::function: {
            return fail("has_next detected in function context");
          }
          case block_context::for_loop: {
            has_next = false;
            goto end;
          }
          case block_context::other: {
            goto end;
          }
        }
      }

      if (has_break) {
        switch (blk_ctx.top()) {
          case block_context::global: {
            return fail("has_break detected in global context");
          }
          case block_context::function: {
            return fail("has_break detected in function context");
          }
          case block_context::for_loop:
          case block_context::other: {
            goto end;
          }
        }
      }

      if (stmt->type == node_type::ret) {
        has_ret = true;
        return std::make_pair(val, new_sym);
      } else if (stmt->type == node_type::next) {
        has_next = true;
        return std::make_pair(val, new_sym);
      } else if (stmt->type == node_type::brk) {
        has_break = true;
        return std::make_pair(val, new_sym);
      }
    }

    end:;
    return std::make_pair(val, new_sym);
  }

  interpret_result interpreter::call(node* nod, symbol_table& sym) {
    auto it = static_cast<call_node*>(nod);

    value_ptr fn = tosuto_unwrap_move(interpret(it->callee.get(), sym));

    std::string failed;
    std::vector<value_ptr> args;
    if (it->is_member) {
      if (fn->is<value::object>()) {
        args.push_back(fn);
      } else if (it->callee->type == node_type::field_get) {
        auto field_get = static_cast<field_get_node*>(it->callee.get());
        if (!field_get->target) {
          if (fn->is<value::function>() || fn->is<value::builtin_function>()) {
            goto end_resolution;
          }

          return fail(nod);
        }
        value_ptr first = tosuto_unwrap_move(interpret(field_get->target.get(), sym));
        args.push_back(first);
      } else {
        return fail(
          "Don't know how to handle member on this one: " + nod->pretty(0));
      }
    }

    end_resolution:;

    for (auto const& arg : it->args) {
      auto attempt = tosuto_unwrap_move(interpret(arg.get(), sym));
      args.push_back(attempt);
    }

    value_ptr ret = tosuto_unwrap_move(fn->call(args, sym));
    return ret;
  }

  interpret_result interpreter::un_op(node* nod, symbol_table& sym) {
    auto it = static_cast<un_op_node*>(nod);

    switch (it->op) {
      case tok_type::sub: {
        value_ptr val = tosuto_unwrap_move(interpret(it->target.get(), sym));
        return val->negate();
      }
      case tok_type::add: {
        value_ptr val = tosuto_unwrap_move(interpret(it->target.get(), sym));
        if (!val->is<value::num>()) {
          return fail(nod);
        }

        return std::make_shared<value>(val->get<value::num>());
      }
      case tok_type::mul: {
        value_ptr val = tosuto_unwrap_move(interpret(it->target.get(), sym));
        return val->deref(sym);
      }
      case tok_type::inc: {
        value_ptr val = tosuto_unwrap_move(interpret(it->target.get(), sym));
        value_ptr res = tosuto_unwrap_move(val->add(value::sym_one, sym));
        *val = *res;
        return res;
      }
      case tok_type::dec: {
        value_ptr val = tosuto_unwrap_move(interpret(it->target.get(), sym));
        value_ptr res = tosuto_unwrap_move(val->sub(value::sym_one, sym));
        *val = *res;
        return res;
      }
      default:return fail(nod);
    }
  }

  interpret_result
  interpreter::assign(node* nod, value_ptr const& val, symbol_table& sym) {
    auto it = static_cast<field_get_node*>(nod);

    if (it->target) {
      value_ptr obj = tosuto_unwrap_move(interpret(it->target.get(), sym));
      return obj->set(it->field, val);
    } else {
      value_ptr existing = tosuto_unwrap_move(sym.get(it->field));
      *existing = *val;
      return val;
    }
  }

  interpret_result interpreter::bin_op(node* nod, symbol_table& sym) {
    auto it = static_cast<bin_op_node*>(nod);

#define TOSUTO_BIN_OP_ONE_CASE_NON_ASSIGN(op) \
      case tok_type::op:\
      {\
        value_ptr lhs = tosuto_unwrap_move(interpret(it->lhs.get(), sym));\
        value_ptr rhs = tosuto_unwrap_move(interpret(it->rhs.get(), sym));\
        return lhs->op(rhs, sym);\
      }
#define TOSUTO_BIN_OP_ONE_CASE_ASSIGN(op) \
      case tok_type::op##_assign:\
      {\
        value_ptr lhs = tosuto_unwrap_move(interpret(it->lhs.get(), sym));\
        value_ptr rhs = tosuto_unwrap_move(interpret(it->rhs.get(), sym));\
        value_ptr res = tosuto_unwrap_move(lhs->op(rhs, sym));\
        *lhs = *res; \
        return res; \
      }

    switch (it->op) {
      TOSUTO_BIN_OP_ONE_CASE_NON_ASSIGN(add)
      TOSUTO_BIN_OP_ONE_CASE_NON_ASSIGN(sub)
      TOSUTO_BIN_OP_ONE_CASE_NON_ASSIGN(mul)
      TOSUTO_BIN_OP_ONE_CASE_NON_ASSIGN(div)
      TOSUTO_BIN_OP_ONE_CASE_NON_ASSIGN(mod)
      TOSUTO_BIN_OP_ONE_CASE_NON_ASSIGN(eq)
      TOSUTO_BIN_OP_ONE_CASE_NON_ASSIGN(neq)
      TOSUTO_BIN_OP_ONE_CASE_ASSIGN(add)
      TOSUTO_BIN_OP_ONE_CASE_ASSIGN(sub)
      TOSUTO_BIN_OP_ONE_CASE_ASSIGN(mul)
      TOSUTO_BIN_OP_ONE_CASE_ASSIGN(div)
      TOSUTO_BIN_OP_ONE_CASE_ASSIGN(mod)
      case tok_type::assign: {
        value_ptr rhs = tosuto_unwrap_move(interpret(it->rhs.get(), sym));
        return assign(it->lhs.get(), rhs, sym);
      }
      case tok_type::sym_or: {
        value_ptr lhs = tosuto_unwrap_move(interpret(it->lhs.get(), sym));
        if (lhs->is_truthy()) return lhs;
        return interpret(it->rhs.get(), sym);
      }
      case tok_type::sym_and: {
        value_ptr lhs = tosuto_unwrap_move(interpret(it->lhs.get(), sym));
        if (!lhs->is_truthy()) return lhs;
        return interpret(it->rhs.get(), sym);
      }
      case tok_type::key_with: {
        value_ptr lhs = tosuto_unwrap_move(interpret(it->lhs.get(), sym));
        if (!lhs->is<value::object>()) return fail(nod);

        value_ptr rhs = tosuto_unwrap_move(interpret(it->rhs.get(), sym));
        if (!lhs->is<value::object>()) return fail(nod);

        auto lhs_fields = value::object{lhs->get<value::object>()};
        auto& rhs_fields = rhs->get<value::object>();

        for (auto const& [k, v]: rhs_fields) {
          lhs_fields[k] = v;
        }

        return std::make_shared<value>(lhs_fields);
      }
      default:return fail(nod);
    }
  }

  interpret_result interpreter::number(node* nod, symbol_table&) {
    auto it = static_cast<number_node*>(nod);

    return std::make_shared<value>(it->value);
  }

  interpret_result interpreter::kw_literal(node* nod, symbol_table&) {
    auto it = static_cast<kw_literal_node*>(nod);

    switch (it->lit) {
      case tok_type::key_false: return std::make_shared<value>(false);
      case tok_type::key_true: return std::make_shared<value>(true);
      case tok_type::key_nil: return std::make_shared<value>(value::nil{});
      default: return fail(nod);
    }
  }

  interpret_result interpreter::string(node* nod, symbol_table&) {
    auto it = static_cast<string_node*>(nod);

    return std::make_shared<value>(it->value);
  }

  interpret_result interpreter::object(node* nod, symbol_table& sym) {
    auto it = static_cast<object_node*>(nod);

    std::string failed;
    std::vector<std::pair<std::string, value_ptr>> fields;
    for (auto const& [k, v] : it->fields) {
      auto attempt = tosuto_unwrap_move(interpret(v.get(), sym));
      fields.emplace_back(k, attempt);
    }

    return std::make_shared<value>(
      std::unordered_map{fields.begin(), fields.end()});
  }

  interpret_result interpreter::field_get(node* nod, symbol_table& sym) {
    auto it = static_cast<field_get_node*>(nod);

    if (it->target) {
      auto lhs = tosuto_unwrap_move(interpret(it->target.get(), sym));
      return lhs->get(it->field);
    } else {
      return sym.get(it->field);
    }
  }

  interpret_result interpreter::if_stmt(node* nod, symbol_table& sym) {
    auto it = static_cast<if_node*>(nod);

    for (auto const& branch: it->cases) {
      value_ptr cond = tosuto_unwrap_move(interpret(branch.first.get(), sym));
      if (cond->is_truthy()) {
        blk_ctx.push(block_context::other);
        value_ptr res = tosuto_unwrap_move(interpret(branch.second.get(), sym));
        blk_ctx.pop();
        return res;
      }
    }

    if (it->else_case) {
      blk_ctx.push(block_context::other);
      value_ptr res = tosuto_unwrap_move(interpret(it->else_case.get(), sym));
      blk_ctx.pop();
      return res;
    }

    return value::sym_nil;
  }

  interpret_result interpreter::ret(node* nod, symbol_table& sym) {
    auto it = static_cast<ret_node*>(nod);

    if (it->ret_val) {
      auto ret_val = tosuto_unwrap_move(interpret(it->ret_val.get(), sym));
      return ret_val;
    }

    return value::sym_nil;
  }

  interpret_result interpreter::do_nothing(node*, symbol_table&) {
    return value::sym_nil;
  }

  interpret_result interpreter::reject(node* nod, symbol_table&) {
    return fail(nod);
  }

  interpret_result interpreter::var_def(node* nod, symbol_table& sym) {
    auto it = static_cast<var_def_node*>(nod);

    auto value = tosuto_unwrap_move(interpret(it->value.get(), sym));
    sym.set(it->name, value);
    return value;
  }

  interpret_result interpreter::range(node* nod, symbol_table& sym) {
    auto it = static_cast<range_node*>(nod);

    auto start = tosuto_unwrap_move(interpret(it->start.get(), sym));
    auto finish = tosuto_unwrap_move(interpret(it->finish.get(), sym));

    return std::make_shared<value>(value::range{start, finish});
  }

  interpret_result interpreter::for_loop(node* nod, symbol_table& sym) {
    auto it = static_cast<for_node*>(nod);

    auto iterable = tosuto_unwrap_move(interpret(it->iterable.get(), sym));
    auto begin = tosuto_unwrap_move(iterable->iterator(sym));
    auto begin_val = tosuto_unwrap_move(begin->deref(sym));
    symbol_table new_sym{{{it->id, begin_val}}, &sym};

    bool cont_val = tosuto_unwrap_move(begin->has_next(sym));

    while (cont_val) {
      blk_ctx.push(block_context::for_loop);
      auto val = tosuto_unwrap_move(interpret(it->body.get(), new_sym));
      blk_ctx.pop();
      if (has_ret) {
        return val;
      }

      if (has_break) {
        reset_state();
        break;
      }

      auto next = tosuto_unwrap_move(begin->next(sym));
      new_sym.set(it->id, next);

      cont_val = tosuto_unwrap_move(begin->has_next(sym));
    }

    return value::sym_nil;
  }

  interpret_result interpreter::interpret(node* nod, symbol_table& sym) {
    return (this->*interpreters[std::to_underlying(nod->type)])(nod, sym);
  }

  interpret_result interpreter::anon_fn_def(node* nod, symbol_table&) {
    auto it = static_cast<fn_def_node*>(nod);
    return std::make_shared<value>(it);
  }

  interpret_result interpreter::decorated(node* nod, symbol_table& sym) {
    auto it = static_cast<decorated_node*>(nod);
    std::vector<std::pair<std::string, value_ptr>> decos;
    for (auto const& deco_erased: it->decos) {
      auto deco = tosuto_dyn_cast(deco_node*, deco_erased.get());
      auto obj = std::make_shared<object_node>(std::move(deco->fields), pos{}, pos{});
      auto val = tosuto_unwrap_move(interpret(obj.get(), sym));
      decos.emplace_back(deco->id, val);
    }

    auto target = tosuto_unwrap_move(interpret(it->target.get(), sym));

    switch (it->target->type) {
      case node_type::fn_def: {
        auto fn_def = tosuto_dyn_cast(fn_def_node*, it->target.get());
        auto fn = tosuto_unwrap_move(sym.get(fn_def->name));
        fn->decos = std::unordered_map{decos.begin(), decos.end()};
        return value::sym_nil;
      }
      default: {
        target->decos = std::unordered_map{decos.begin(), decos.end()};
        return target;
      }
    }
  }

  interpret_result interpreter::block(node* nod, symbol_table& sym) {
    auto thing = tosuto_unwrap_move(block_with_symbols(nod, sym));
    return thing.first;
  }

  std::expected<symbol_table, std::string>
  interpreter::global(node* nod, symbol_table& sym) {
    blk_ctx.push(block_context::global);
    auto thing = tosuto_unwrap_move(block_with_symbols(nod, sym));
    blk_ctx.pop();
    return *thing.second.par;
  }
}
