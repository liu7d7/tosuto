#include "compile.h"

#include <ranges>
#include <format>
#include <iostream>

namespace ami::vm {
  std::unordered_map<node_type, std::expected<void, std::string>(compiler::*)(
    ami::node*)> compilers{
    {node_type::bin_op,      &compiler::bin_op},
    {node_type::number,      &compiler::number},
    {node_type::un_op,       &compiler::un_op},
    {node_type::block,       &compiler::block},
    {node_type::kw_literal,  &compiler::kw_literal},
    {node_type::string,      &compiler::string},
    {node_type::var_def,     &compiler::var_def},
    {node_type::field_get,   &compiler::field_get},
    {node_type::if_stmt,     &compiler::if_stmt},
    {node_type::fn_def,      &compiler::fn_def},
    {node_type::call,        &compiler::call},
    {node_type::ret,         &compiler::ret},
    {node_type::object,      &compiler::object},
    {node_type::anon_fn_def, &compiler::anon_fn_def},
    {node_type::member_call, &compiler::member_call},
    {node_type::array,       &compiler::array},
    {node_type::for_loop,    &compiler::for_loop},
    {node_type::sized_array, &compiler::sized_array},
    {node_type::decorated,   &compiler::decorated},
  };

#define AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(op) case tok_type::op: cur_ch().add(op_code::op); break;

  std::expected<void, std::string> compiler::bin_op(ami::node* n) {
    auto it = ami_dyn_cast(bin_op_node*, n);
    if (it->op == tok_type::assign) {
      if (it->lhs->type == node_type::bin_op) {
        auto lhs = ami_dyn_cast(bin_op_node*, it->lhs.get());
        if (lhs->op == tok_type::l_square) {
          // a[b] = c
          ami_discard(compile(lhs->lhs.get()));
          ami_discard(compile(lhs->rhs.get()));
          ami_discard(compile(it->rhs.get()));
          cur_ch().add(op_code::idx_s);

          return {};
        }
      }

      if (it->lhs->type != node_type::field_get) {
        return std::unexpected{"Expected field get on lhs of assign!"};
      }

      auto lhs = ami_dyn_cast(field_get_node*, it->lhs.get());
      if (lhs->target) {
        ami_discard(compile(lhs->target.get()));
        ami_discard(compile(it->rhs.get()));
        cur_ch().add(op_code::prop_s);
        auto field_name = lhs->field;
        cur_ch().add(cur_ch().add_lit_get(value{value::str{field_name}}));

        return {};
      }

      ami_discard(compile(it->rhs.get()));
      auto op = set_instr(lhs->field);

      cur_ch().add(op.first);
      cur_ch().add(op.second);

      return {};
    }

    if (it->op == tok_type::sym_and) {
      ami_discard(compile(it->lhs.get()));
      auto else_jmp = emit_jump(op_code::jmpf);
      cur_ch().add(op_code::pop);
      ami_discard(compile(it->rhs.get()));
      ami_discard(patch_jump(else_jmp));

      return {};
    }

    if (it->op == tok_type::sym_or) {
      ami_discard(compile(it->lhs.get()));

      auto else_jmp = emit_jump(op_code::jmpf);
      auto end_jmp = emit_jump(op_code::jmp);
      ami_discard(patch_jump(else_jmp));
      cur_ch().add(op_code::pop);

      ami_discard(compile(it->rhs.get()));
      ami_discard(patch_jump(end_jmp));

      return {};
    }

    ami_discard(compile(it->lhs.get()));
    ami_discard(compile(it->rhs.get()));
    switch (it->op) {
      AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(add)
      AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(sub)
      AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(mul)
      AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(div)
      AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(mod)
      AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(eq)
      AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(key_with)
      case tok_type::neq: cur_ch().add(op_code::eq);
        cur_ch().add(op_code::inv);
        break;
      case tok_type::greater_than: cur_ch().add(op_code::gt);
        break;
      case tok_type::less_than: cur_ch().add(op_code::lt);
        break;
      case tok_type::greater_than_equal: cur_ch().add(op_code::lt);
        cur_ch().add(op_code::inv);
        break;
      case tok_type::less_than_equal: cur_ch().add(op_code::gt);
        cur_ch().add(op_code::inv);
        break;
      case tok_type::l_square: cur_ch().add(op_code::idx_g);
        break;
      default:
        return std::unexpected{"Unknown infix operator at " + n->pretty(0)};
    }

    return {};
  }

  std::expected<void, std::string> compiler::compile(node* n) {
    return (this->*compilers[n->type])(n);
  }

  std::expected<void, std::string> compiler::sized_array(node* n) {
    auto it = ami_dyn_cast(sized_array_node*, n);

    ami_discard(compile(it->size.get()));
    ami_discard(compile(it->val.get()));
    cur_ch().add(op_code::szd_arr);

    return {};
  }

  std::expected<void, std::string> compiler::for_loop(node* n) {
    auto it = ami_dyn_cast(for_node*, n);

    if (it->iterable->type != node_type::range) {
      return std::unexpected{"For only supports range rn!"};
    }

    auto iter = ami_dyn_cast(range_node*, it->iterable.get());

    begin_block();

    ami_discard(compile(iter->start.get()));
    ami_discard(add_local(it->id));
    auto slot = *resolve_local(it->id); // no way this fails, right?

    ami_discard(compile(iter->finish.get()));
    auto end_id = "@" + std::to_string(rand());
    ami_discard(add_local(end_id));
    auto end_slot = *resolve_local(end_id);

    begin_block();

    std::vector<size_t> next_jmps, break_jmps;
    auto body = ami_dyn_cast(block_node*, it->body.get());

    size_t block_start = cur_ch().data.size();

    for (auto const& stmt: body->exprs) {
      switch (stmt->type) {
        case node_type::next: {
          next_jmps.push_back(emit_jump(op_code::jmp));
          break;
        }
        case node_type::brk: {
          break_jmps.push_back(emit_jump(op_code::jmp));
          break;
        }
        default: {
          ami_discard(compile(stmt.get()));
          ami_discard(pop_for_exp_stmt(stmt.get()));
          break;
        }
      }
    }

    for (auto jmp: next_jmps) {
      ami_discard(patch_jump(jmp));
    }

    end_block();

    cur_ch().add(op_code::loc_g);
    cur_ch().add(slot);
    cur_ch().add(op_code::ld_1);
    cur_ch().add(op_code::add);
    cur_ch().add(op_code::loc_s);
    cur_ch().add(slot);
    cur_ch().add(op_code::loc_g);
    cur_ch().add(end_slot);
    cur_ch().add(op_code::lt);
    size_t loop_jmp = emit_jump(op_code::jmpb_pop);
    u16 jmp = loop_jmp - block_start + 2;
    cur_ch().data[loop_jmp] = u8(jmp & 0xff);
    cur_ch().data[loop_jmp + 1] = u8((jmp >> 8) & 0xff);

    for (auto break_jmp: break_jmps) {
      ami_discard(patch_jump(break_jmp));
    }

    end_block();

    return {};
  }

  std::expected<void, std::string> compiler::member_call(node* n) {
    auto it = ami_dyn_cast(member_call_node*, n);

    ami_discard(compile(it->callee.get()));

    cur_ch().add(op_code::prop_g);
    auto lit = cur_ch().add_lit_get(value{value::str{it->field}});
    cur_ch().add(lit);

    ami_discard(compile(it->callee.get()));

    for (auto const& arg: it->args) {
      ami_discard(compile(arg.get()));
    }

    cur_ch().add(op_code::call);
    if (it->args.size() + 1 > max_of<u8>) {
      return std::unexpected{"Too many args in call!"};
    }

    cur_ch().add(u8(it->args.size() + 1));

    return {};
  }

  std::expected<void, std::string> compiler::anon_fn_def(node* n) {
    ami_discard(function(value::fn::type::fn, n));

    return {};
  }

  std::expected<void, std::string> compiler::decorate_fn(decorated_node* decor, std::shared_ptr<node> const& fn_def) {
    // we need to translate this to setting fn_def->name to the result of these calls.

    // desugaring process:
    // add_one : original -> : -> original() + 1
    //
    // @add_one
    // test -> 1
    //
    // log(test())

    // becomes:
    // test := add_one(: -> 1)
    //
    // log(test())

    fn_def->type = node_type::anon_fn_def;
    auto first_arg = fn_def;
    for (auto const& erased_deco : decor->decos) {
      auto deco = ami_dyn_cast(deco_node*, erased_deco.get());
      std::vector<std::shared_ptr<node>> args;
      args.push_back(first_arg);
      for (auto const& it : deco->fields) {
        args.push_back(it);
      }

      first_arg = std::make_shared<call_node>(deco->deco, args, pos::synthesized, pos::synthesized);
    }

    auto fn_def_casted = ami_dyn_cast(fn_def_node*, fn_def.get());

    auto definition =
      std::make_shared<var_def_node>(
        fn_def_casted->name, first_arg, pos::synthesized, pos::synthesized);

    ami_discard(compile(definition.get()));

    return {};
  }

  std::expected<void, std::string> compiler::decorated(node* n) {
    auto it = ami_dyn_cast(decorated_node*, n);

    switch (it->target->type) {
      case node_type::fn_def: {
        ami_discard(decorate_fn(it, it->target));
        break;
      }
      default:; return std::unexpected{"Can't decorate " + it->target->pretty(0)};
    }

    return {};
  }

  std::expected<void, std::string> compiler::array(node* n) {
    auto it = ami_dyn_cast(array_node*, n);

    if (it->exprs.size() > max_of<u16>) {
      return std::unexpected{"Too many values in array!"};
    }

    for (auto const& val: it->exprs) {
      ami_discard(compile(val.get()));
    }

    cur_ch().add(op_code::array);
    cur_ch().add(u16(it->exprs.size()));

    return {};
  }

  std::expected<void, std::string> compiler::object(node* n) {
    auto it = ami_dyn_cast(object_node*, n);
    cur_ch().add(op_code::new_obj);
    auto r = rand();
    for (auto const& [k, v]: it->fields) {
      if (v->type == node_type::anon_fn_def) {
        auto fn_def = ami_dyn_cast(fn_def_node*, v.get());
        fn_def->name = k + "@" + std::format("{:04x}", r);
      }

      ami_discard(compile(v.get()));
      cur_ch().add(op_code::prop_d);
      auto field_name = cur_ch().add_lit_get(value{value::str{k}});
      cur_ch().add(field_name);
    }

    return {};
  }

  std::expected<void, std::string> compiler::ret(node* n) {
    if (fn_type == value::fn::type::script) {
      return std::unexpected{"Can't return from top level!"};
    }

    auto it = ami_dyn_cast(ret_node*, n);

    if (it->ret_val) {
      ami_discard(compile(it->ret_val.get()));
      cur_ch().add(op_code::ret);
    } else {
      cur_ch().add(op_code::key_nil);
      cur_ch().add(op_code::ret);
    }

    return {};
  }

  std::expected<void, std::string>
  compiler::function(value::fn::type type, node* n) {
    auto it = ami_dyn_cast(fn_def_node*, n);

    compiler comp{type};
    comp.enclosing = this;
    comp.begin_block();

    auto& args = it->args;
    if (args.size() > max_of<u8>)
      return std::unexpected{"Too many arguments in function!"};
    comp.fn.ch->second = u8(args.size());
    comp.fn.ch->first.name = it->name.empty() ? value::str{"anonymous"} : value::str{it->name};
    for (auto const& arg: args) {
      ami_discard(comp.add_local(arg.first));
    }

    ami_discard(comp.exp_or_block_no_pop(it->body.get()));

    comp.cur_ch().add(op_code::ret);

    u16 lit = cur_ch().add_lit_get(value{comp.fn});
    cur_ch().add(op_code::closure);
    cur_ch().add(lit);
    cur_ch().add(u16(comp.upvals.size()));

    for (auto const& upval : comp.upvals) {
      cur_ch().add(u8(upval.is_local));
      cur_ch().add(u16(upval.index));
    }

    return {};
  }

  std::expected<void, std::string> compiler::fn_def(node* n) {
    auto it = ami_dyn_cast(fn_def_node*, n);

    ami_discard(function(value::fn::type::fn, n));
    if (depth > 0) {
      ami_discard(add_local(it->name));
    } else {
      cur_ch().add(op_code::glob_d);
      auto lit = cur_ch().add_lit_get(value{value::str{it->name}});
      cur_ch().add(lit);
    }

    return {};
  }

  std::expected<void, std::string> compiler::call(node* n) {
    auto it = ami_dyn_cast(call_node*, n);

    ami_discard(compile(it->callee.get()));

    for (auto const& arg: it->args) {
      ami_discard(compile(arg.get()));
    }

    cur_ch().add(op_code::call);
    if (it->args.size() > max_of<u8>) {
      return std::unexpected{"Too many args in call!"};
    }

    cur_ch().add(u8(it->args.size()));

    return {};
  }

  void compiler::begin_block() {
    depth++;
  }

  void compiler::end_block() {
    depth--;

    auto it = int(locals.size() - 1);
    while (it != 0 && locals[it].depth > depth) {
      std::cout << "hi\n";
      if (locals[it].is_captured) {
        cur_ch().add(op_code::upval_c);
      } else {
        cur_ch().add(op_code::pop_loc);
      }
      it--;
      locals.pop_back();
    }
  }

  size_t compiler::emit_jump(op_code type) {
    cur_ch().add(type);
    cur_ch().add(u8(0xff));
    cur_ch().add(u8(0xff));
    return cur_ch().data.size() - 2;
  }

  std::expected<void, std::string> compiler::patch_jump(size_t offset) {
    size_t jump = cur_ch().data.size() - offset - 2;

    if (jump > max_of<u16>) {
      return std::unexpected{"Tried to jump farther than a rd_u16 can store!"};
    }

    cur_ch().data[offset] = u8(jump & 0xff);
    cur_ch().data[offset + 1] = u8(jump >> 8 & 0xff);

    return {};
  }

  std::expected<void, std::string> compiler::if_stmt(node* n) {
    auto it = ami_dyn_cast(if_node*, n);

    std::vector<size_t> else_jumps;
    for (auto& i: it->cases) {
      ami_discard(compile(i.first.get()));
      size_t then_jump = emit_jump(op_code::jmpf_pop);

      ami_discard(exp_or_block_no_pop(i.second.get()));
      else_jumps.push_back(emit_jump(op_code::jmp));

      ami_discard(patch_jump(then_jump));
    }

    if (it->else_case) {
      ami_discard(exp_or_block_no_pop(it->else_case.get()));
    }

    for (auto else_jump: else_jumps) {
      ami_discard(patch_jump(else_jump));
    }

    return {};
  }

  std::expected<void, std::string> compiler::kw_literal(node* n) {
    auto it = ami_dyn_cast(kw_literal_node*, n);
    switch (it->lit) {
      AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(key_true)
      AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(key_false)
      AMI_SIMPLE_CVT_TOKTYPE_TO_INSTR(key_nil)
      default:
        return std::unexpected{"Unknown keyword lit_16 at " + n->pretty(0)};
    }

    return {};
  }

  std::expected<void, std::string>
  compiler::add_local(std::string const& name) {
    auto str = value::str{name};
    if (locals.size() == max_locals) return std::unexpected{"Too many locals!"};
    for (auto const& it: std::ranges::reverse_view(locals)) {
      if (it.depth == local::invalid_depth || it.depth < depth) break;

      if (it.name == str) {
        return std::unexpected{
          "Variable " + name + " already exists in scope!"};
      }
    }

    locals.emplace_back(value::str{name}, depth);

    return {};
  }

  std::expected<void, std::string> compiler::var_def(node* n) {
    auto it = ami_dyn_cast(var_def_node*, n);

    ami_discard(compile(it->value.get()));

    if (depth > 0) {
      ami_discard(add_local(it->name));
    } else {
      cur_ch().add(op_code::glob_d);
      auto lit = cur_ch().add_lit_get(value{value::str{it->name}});
      cur_ch().add(lit);
    }

    return {};
  }

  std::optional<u16> compiler::resolve_local(std::string const& name) {
    auto str = value::str{name};
    for (int i = int(locals.size()) - 1; i >= 0; i--) {
      auto& loc = locals[i];
      if (loc.name == str) return i;
    }

    return std::nullopt;
  }

  std::expected<void, std::string> compiler::field_get(node* n) {
    auto it = ami_dyn_cast(field_get_node*, n);
    if (it->target) {
      ami_discard(compile(it->target.get()));
      cur_ch().add(op_code::prop_g);
      auto field_name = cur_ch().add_lit_get(value{value::str{it->field}});
      cur_ch().add(field_name);
      return {};
    }

    auto op = get_instr(it->field);

    cur_ch().add(op.first);
    cur_ch().add(op.second);

    return {};
  }

  std::expected<void, std::string> compiler::number(node* n) {
    auto it = ami_dyn_cast(number_node*, n);
    if (fabs(it->value) < value::epsilon) {
      cur_ch().add(op_code::ld_0);
    } else if (fabs(it->value - 1) < value::epsilon) {
      cur_ch().add(op_code::ld_1);
    } else {
      cur_ch().add_lit(value{it->value});
    }
    return {};
  }

  std::expected<void, std::string> compiler::string(node* n) {
    auto it = ami_dyn_cast(string_node*, n);
    cur_ch().add_lit(value{value::str{it->value}});
    return {};
  }

  std::expected<void, std::string> compiler::un_op(node* n) {
    auto it = ami_dyn_cast(un_op_node*, n);
    ami_discard(compile(it->target.get()));
    switch (it->op) {
      case tok_type::sub: {
        cur_ch().add(op_code::neg);
        break;
      }
      case tok_type::exclaim: {
        cur_ch().add(op_code::inv);
        break;
      }
      default:
        return std::unexpected{"Unknown unary operator at " + n->pretty(0)};
    }
    return {};
  }

  std::expected<void, std::string> compiler::pop_for_exp_stmt(node* exp) {
    switch (exp->type) {
      case node_type::var_def:
      case node_type::fn_def:
      case node_type::for_loop:break;
      case node_type::decorated: {
        auto decor = ami_dyn_cast(decorated_node*, exp);
        if (decor->target->type == node_type::fn_def || decor->target->type == node_type::anon_fn_def) {
          break;
        }
      }
      default:cur_ch().add(op_code::pop);
    }

    return {};
  }

  std::expected<void, std::string>
  compiler::basic_block(node* n, bool pop_last) {
    auto it = ami_dyn_cast(block_node*, n);
    size_t i = 0;
    auto last = it->exprs.size() - 1;
    for (auto const& exp: it->exprs) {
      ami_discard(compile(exp.get()));
      if (pop_last || i != last) {
        ami_discard(pop_for_exp_stmt(exp.get()));
      }
    }

    return {};
  }

  std::expected<void, std::string> compiler::block(node* n) {
    begin_block();
    ami_discard(basic_block(n, true));
    end_block();
    return {};
  }

  std::expected<value::fn, std::string> compiler::global(node* n) {
    ami_discard(basic_block(n, true));

    cur_ch().add(op_code::ret);
    return fn;
  }

  compiler::compiler(value::fn::type type) : fn_type(type), enclosing(nullptr) {
    locals.emplace_back(value::str{""}, 0);
  }

  std::expected<void, std::string> compiler::exp_or_block_no_pop(node* n) {
    if (n->type == node_type::block) {
      return basic_block(n, false);
    } else {
      return compile(n);
    }
  }

  std::optional<u16> compiler::resolve_upval(std::string const& name) {
    if (enclosing == nullptr) return std::nullopt;

    auto local = enclosing->resolve_local(name);
    if (local) {
      enclosing->locals[*local].is_captured = true;
      return add_upval(*local, true);
    }

    auto upval = enclosing->resolve_upval(name);
    if (upval) {
      return add_upval(*upval, false);
    }

    return std::nullopt;
  }

  u16 compiler::add_upval(u16 idx, bool is_local) {
    auto it = std::find_if(upvals.begin(), upvals.end(), [&](upvalue const& val) {
      return val.index == idx && val.is_local == is_local;
    });

    if (it != upvals.end()) {
      return std::distance(upvals.begin(), it);
    }

    upvals.emplace_back(idx, is_local);
    return upvals.size() - 1;
  }

  std::pair<op_code, u16> compiler::get_instr(const std::string& name) {
    auto try_get_local = resolve_local(name);
    if (try_get_local) {
      return {op_code::loc_g, *try_get_local};
    } else {
      auto try_get_upval = resolve_upval(name);
      if (try_get_upval) {
        return {op_code::upval_g, *try_get_upval};
      } else {
        return {op_code::glob_g, cur_ch().add_lit_get(value{value::str{name}})};
      }
    }
  }

  std::pair<op_code, u16> compiler::set_instr(const std::string& name) {
    auto try_get_local = resolve_local(name);
    if (try_get_local) {
      return {op_code::loc_s, *try_get_local};
    } else {
      auto try_get_upval = resolve_upval(name);
      if (try_get_upval) {
        return {op_code::upval_s, *try_get_upval};
      } else {
        return {op_code::glob_s, cur_ch().add_lit_get(value{value::str{name}})};
      }
    }
  }
}