#include <iostream>
#include "interpret.h"

namespace ami
{
  std::string repeat(std::string const& input, size_t num)
  {
    std::string ret;
    ret.reserve(input.size() * num);
    while (num--)
      ret += input;
    return ret;
  }

  symbol_table::symbol_table(std::unordered_map<std::string, value_ptr> vals,
                             symbol_table* par) : vals(std::move(vals)),
                                                  par(par)
  {}

  interpret_result symbol_table::get(std::string const& name)
  {
    if (vals.contains(name)) return vals[name];
    if (!par) return std::unexpected("Failed to find " + name);
    return par->get(name);
  }

  void symbol_table::set(std::string const& name, value_ptr const& val)
  {
    vals[name] = val;
  }

  symbol_table::symbol_table() : par(nullptr), vals()
  {
  }

  std::unexpected<std::string>
  interpreter::fail(node* nod, std::source_location loc)
  {
    return std::unexpected(
      "Failed to interpret " + nod->pretty(0) + " in " + loc.function_name() +
      " on line " + std::to_string(loc.line()));
  }

  std::unexpected<std::string>
  interpreter::fail(std::string const& nod, std::source_location loc)
  {
    return std::unexpected(
      nod + "\nin " + loc.function_name() +
      " on line " + std::to_string(loc.line()));
  }

  interpret_result interpreter::fn_def(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(fn_def_node*, nod);

    sym.set(it->name, std::make_shared<value>(it));
    return value::sym_nil;
  }

  std::expected<std::pair<value_ptr, symbol_table>, std::string>
  interpreter::block_with_symbols(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(block_node*, nod);

    symbol_table new_sym{{}, &sym};

    value_ptr val;
    for (auto const& stmt: it->exprs)
    {
      val = ami_unwrap(interpret(stmt, sym));
      // TODO: figure out what to do with has_ret & has_next
      if (stmt->type == node_type::ret)
      {
        return std::make_pair(val, new_sym);
      }
      else if (stmt->type == node_type::next)
      {
        return std::make_pair(val, new_sym);
      }
      else if (stmt->type == node_type::brk)
      {
        has_break = true;
        return std::make_pair(val, new_sym);
      }
    }

    return std::make_pair(val, new_sym);
  }

  interpret_result interpreter::call(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(call_node*, nod);

    value_ptr fn = ami_unwrap(interpret(it->callee, sym));

    std::string failed;
    std::vector<value_ptr> args;
    if (it->is_member)
    {
      if (it->callee->type == node_type::field_get)
      {
        auto fget = ami_dyn_cast(field_get_node*, it->callee);
        if (!fget->target) return fail(nod);
        value_ptr first = ami_unwrap(interpret(fget->target, sym));
        args.push_back(first);
      }
      else if (fn->is<value::object>())
      {
        args.push_back(fn);
      }
      else
      {
        return fail(
          "Don't know how to handle member on this one: " + nod->pretty(0));
      }
    }

    std::transform(it->args.begin(),
                   it->args.end(),
                   std::back_inserter(args),
                   [this, &sym, &failed](node* const& a)
                   {
                     auto attempt = interpret(a, sym);
                     if (!attempt.has_value())
                     {
                       failed = attempt.error();
                       return value::sym_nil;
                     }

                     return attempt.value();
                   });

    if (!failed.empty())
      return std::unexpected{
        failed + " in " + std::source_location::current().function_name()};

    value_ptr ret = ami_unwrap(fn->call(args, sym));
    return ret;
  }

  interpret_result interpreter::un_op(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(un_op_node*, nod);

    switch (it->op)
    {
      case tok_type::sub:
      {
        value_ptr val = ami_unwrap(interpret(it->target, sym));
        return val->negate();
      }
      case tok_type::add:
      {
        value_ptr val = ami_unwrap(interpret(it->target, sym));
        if (!val->is<value::num>())
        {
          return fail(nod);
        }

        return std::make_shared<value>(val->get<value::num>());
      }
      case tok_type::mul:
      {
        value_ptr val = ami_unwrap(interpret(it->target, sym));
        return val->deref(sym);
      }
      default:
        return fail(nod);
    }
  }

  interpret_result
  interpreter::assign(node* nod, value_ptr const& val, symbol_table& sym)
  {
    auto it = ami_dyn_cast(field_get_node*, nod);

    if (it->target)
    {
      value_ptr obj = ami_unwrap(interpret(it->target, sym));
      return obj->set(it->field, val);
    }
    else
    {
      value_ptr existing = ami_unwrap(sym.get(it->field));
      // TODO: is this the right semantic?
      // if you assign to a reference value, that value will change, but
      // if you assign to the thing that you just assigned then it won't change.
      // that sounds right?
      *existing = *val;
      return val;
    }
  }

  interpret_result interpreter::bin_op(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(bin_op_node*, nod);

#define AMI_BIN_OP_ONE_CASE_NON_ASSIGN(op) \
      case tok_type::op:\
      {\
        value_ptr lhs = ami_unwrap(interpret(it->lhs, sym));\
        value_ptr rhs = ami_unwrap(interpret(it->rhs, sym));\
        return lhs->op(rhs, sym);\
      }
#define AMI_BIN_OP_ONE_CASE_ASSIGN(op) \
      case tok_type::op##_assign:\
      {\
        value_ptr lhs = ami_unwrap(interpret(it->lhs, sym));\
        value_ptr rhs = ami_unwrap(interpret(it->rhs, sym));\
        value_ptr res = ami_unwrap(lhs->op(rhs, sym));\
        return assign(it->lhs, res, sym);\
      }

    switch (it->op)
    {
      AMI_BIN_OP_ONE_CASE_NON_ASSIGN(add)
      AMI_BIN_OP_ONE_CASE_NON_ASSIGN(sub)
      AMI_BIN_OP_ONE_CASE_NON_ASSIGN(mul)
      AMI_BIN_OP_ONE_CASE_NON_ASSIGN(div)
      AMI_BIN_OP_ONE_CASE_NON_ASSIGN(mod)
      AMI_BIN_OP_ONE_CASE_NON_ASSIGN(eq)
      AMI_BIN_OP_ONE_CASE_NON_ASSIGN(neq)
      AMI_BIN_OP_ONE_CASE_ASSIGN(add)
      AMI_BIN_OP_ONE_CASE_ASSIGN(sub)
      AMI_BIN_OP_ONE_CASE_ASSIGN(mul)
      AMI_BIN_OP_ONE_CASE_ASSIGN(div)
      AMI_BIN_OP_ONE_CASE_ASSIGN(mod)
      case tok_type::assign:
      {
        value_ptr rhs = ami_unwrap(interpret(it->rhs, sym));
        return assign(it->lhs, rhs, sym);
      }
      case tok_type::sym_or:
      {
        value_ptr lhs = ami_unwrap(interpret(it->lhs, sym));
        if (lhs->is_truthy()) return lhs;
        return interpret(it->rhs, sym);
      }
      case tok_type::sym_and:
      {
        value_ptr lhs = ami_unwrap(interpret(it->lhs, sym));
        if (!lhs->is_truthy()) return lhs;
        return interpret(it->rhs, sym);
      }
      case tok_type::key_with:
      {
        value_ptr lhs = ami_unwrap(interpret(it->lhs, sym));
        if (!lhs->is<value::object>()) return fail(nod);

        value_ptr rhs = ami_unwrap(interpret(it->rhs, sym));
        if (!lhs->is<value::object>()) return fail(nod);

        auto lhs_fields = value::object{lhs->get<value::object>()};
        auto& rhs_fields = rhs->get<value::object>();

        for (auto const& [k, v] : rhs_fields)
        {
          lhs_fields[k] = v;
        }

        return std::make_shared<value>(lhs_fields);
      }
      default:
        return fail(nod);
    }
  }

  interpret_result interpreter::number(node* nod, symbol_table&)
  {
    auto it = ami_dyn_cast(number_node*, nod);

    return std::make_shared<value>(it->value);
  }

  interpret_result interpreter::string(node* nod, symbol_table&)
  {
    auto it = ami_dyn_cast(string_node*, nod);

    return std::make_shared<value>(it->value);
  }

  interpret_result interpreter::object(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(object_node*, nod);

    std::string failed;
    std::vector<std::pair<std::string, value_ptr>> fields;
    std::transform(
      it->fields.begin(),
      it->fields.end(),
      std::back_inserter(fields),
      [this, &sym, &failed](std::pair<std::string, node*> const& field)
      {
        auto attempt = interpret(field.second, sym);
        if (!attempt.has_value())
        {
          failed = attempt.error();
          return std::pair{""s, value::sym_nil};
        }

        return std::pair{field.first, attempt.value()};
      });

    if (!failed.empty())
      return std::unexpected{
        failed + " at " + std::source_location::current().function_name()};
    return std::make_shared<value>(
      std::unordered_map{fields.begin(), fields.end()});
  }

  interpret_result interpreter::field_get(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(field_get_node*, nod);

    if (it->target)
    {
      auto lhs = ami_unwrap(interpret(it->target, sym));
      return lhs->get(it->field);
    }
    else
    {
      return sym.get(it->field);
    }
  }

  interpret_result interpreter::if_stmt(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(if_node*, nod);

    for (auto const& branch: it->cases)
    {
      value_ptr cond = ami_unwrap(interpret(branch.first, sym));
      if (cond->is_truthy())
      {
        value_ptr res = ami_unwrap(interpret(branch.second, sym));
        return res;
      }
    }

    if (it->else_case)
    {
      value_ptr res = ami_unwrap(interpret(it->else_case, sym));
      return res;
    }

    return value::sym_nil;
  }

  interpret_result interpreter::ret(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(ret_node*, nod);

    if (it->ret_val)
    {
      auto ret_val = ami_unwrap(interpret(it->ret_val, sym));
      return ret_val;
    }

    return value::sym_nil;
  }

  interpret_result interpreter::do_nothing(node*, symbol_table&)
  {
    return value::sym_nil;
  }

  interpret_result interpreter::var_def(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(var_def_node*, nod);

    auto value = ami_unwrap(interpret(it->value, sym));
    sym.set(it->name, value);
    return value;
  }

  interpret_result interpreter::range(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(range_node*, nod);

    auto start = ami_unwrap(interpret(it->start, sym));
    auto finish = ami_unwrap(interpret(it->finish, sym));

    return std::make_shared<value>(value::range{start, finish});
  }

  interpret_result interpreter::for_loop(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(for_node*, nod);

    auto iterable = ami_unwrap(interpret(it->iterable, sym));
    auto begin = ami_unwrap(iterable->iterator(sym));
    auto begin_val = ami_unwrap(begin->deref(sym));
    symbol_table new_sym{{{it->id, begin_val}}, &sym};

    bool cont_val = ami_unwrap(begin->has_next(sym));

    while (cont_val)
    {
      ami_discard(interpret(it->body, new_sym));
      if (has_break)
      {
        reset_state();
        break;
      }

      auto next = ami_unwrap(begin->next(sym));
      new_sym.set(it->id, next);

      cont_val = ami_unwrap(begin->has_next(sym));
    }

    return value::sym_nil;
  }

  interpret_result interpreter::interpret(node* nod, symbol_table& sym)
  {
    return (this->*interpreters[nod->type])(nod, sym);
  }

  std::unordered_map<
    node_type,
    interpret_result(interpreter::*)(node*,
                                     symbol_table&)> interpreter::interpreters
    {
      {node_type::fn_def,      &interpreter::fn_def},
      {node_type::block,       &interpreter::block},
      {node_type::call,        &interpreter::call},
      {node_type::un_op,       &interpreter::un_op},
      {node_type::bin_op,      &interpreter::bin_op},
      {node_type::number,      &interpreter::number},
      {node_type::string,      &interpreter::string},
      {node_type::object,      &interpreter::object},
      {node_type::field_get,   &interpreter::field_get},
      {node_type::if_stmt,     &interpreter::if_stmt},
      {node_type::ret,         &interpreter::ret},
      {node_type::next,        &interpreter::do_nothing},
      {node_type::brk,         &interpreter::do_nothing},
      {node_type::var_def,     &interpreter::var_def},
      {node_type::range,       &interpreter::range},
      {node_type::for_loop,    &interpreter::for_loop},
      {node_type::anon_fn_def, &interpreter::anon_fn_def},
    };

  interpret_result interpreter::anon_fn_def(node* nod, symbol_table& sym)
  {
    auto it = ami_dyn_cast(fn_def_node*, nod);
    return std::make_shared<value>(it);
  }

  interpret_result interpreter::block(node* nod, symbol_table& sym)
  {
    auto thing = ami_unwrap(block_with_symbols(nod, sym));
    return thing.first;
  }

  std::expected<symbol_table, std::string> interpreter::global(node* nod, symbol_table& sym)
  {
    auto thing = ami_unwrap(block_with_symbols(nod, sym));
    return thing.second;
  }
}