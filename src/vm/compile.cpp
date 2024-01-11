#include "compile.h"

#include <ranges>

namespace tosuto::vm {
  std::unordered_map<node_type, std::expected<void, std::string>(compiler::*)(tosuto::node*)> compilers {
    {node_type::bin_op, &compiler::bin_op},
    {node_type::number, &compiler::number},
    {node_type::un_op, &compiler::un_op},
    {node_type::block, &compiler::block},
    {node_type::kw_literal, &compiler::kw_literal},
    {node_type::string, &compiler::string},
    {node_type::var_def, &compiler::var_def},
    {node_type::field_get, &compiler::field_get},
    {node_type::if_stmt, &compiler::if_stmt},
    {node_type::fn_def, &compiler::fn_def},
    {node_type::call, &compiler::call},
    {node_type::ret, &compiler::ret},
    {node_type::object, &compiler::object},
    {node_type::anon_fn_def, &compiler::anon_fn_def},
    {node_type::member_call, &compiler::member_call},
  };

#define TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(op) case tok_type::op: cur_ch().add(op_code::op); break;

  std::expected<void, std::string> compiler::bin_op(tosuto::node* n) {
    auto it = tosuto_dyn_cast(bin_op_node*, n);
    if (it->op == tok_type::assign) {
      if (it->lhs->type != node_type::field_get) {
        return std::unexpected{"Expected field get on lhs of assign!"};
      }

      auto lhs = tosuto_dyn_cast(field_get_node*, it->lhs.get());
      if (lhs->target) {
        return std::unexpected{"Only supports non-member assignment rn!"};
      }

      tosuto_discard(compile(it->rhs.get()));
      cur_ch().add(op_code::set_global);
      auto lit = cur_ch().add_lit(value{value::str{lhs->field}});
      cur_ch().add(lit);

      return {};
    }

    if (it->op == tok_type::sym_and) {
      tosuto_discard(compile(it->lhs.get()));
      auto else_jmp = emit_jump(op_code::jmpf);
      cur_ch().add(op_code::pop);
      tosuto_discard(compile(it->rhs.get()));
      tosuto_discard(patch_jump(else_jmp));

      return {};
    }

    if (it->op == tok_type::sym_or) {
      tosuto_discard(compile(it->lhs.get()));

      auto else_jmp = emit_jump(op_code::jmpf);
      auto end_jmp = emit_jump(op_code::jmp);
      tosuto_discard(patch_jump(else_jmp));
      cur_ch().add(op_code::pop);

      tosuto_discard(compile(it->rhs.get()));
      tosuto_discard(patch_jump(end_jmp));

      return {};
    }

    tosuto_discard(compile(it->lhs.get()));
    tosuto_discard(compile(it->rhs.get()));
    switch (it->op) {
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(add)
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(sub)
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(mul)
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(div)
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(mod)
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(eq)
      case tok_type::neq: cur_ch().add(op_code::eq); cur_ch().add(op_code::inv); break;
      case tok_type::greater_than: cur_ch().add(op_code::gt); break;
      case tok_type::less_than: cur_ch().add(op_code::lt); break;
      case tok_type::greater_than_equal: cur_ch().add(op_code::lt); cur_ch().add(op_code::inv); break;
      case tok_type::less_than_equal: cur_ch().add(op_code::gt); cur_ch().add(op_code::inv); break;
      default: return std::unexpected{"Unknown infix operator at " + n->pretty(0)};
    }

    return {};
  }

  std::expected<void, std::string> compiler::compile(node* n) {
    return (this->*compilers[n->type])(n);
  }

  std::expected<void, std::string> compiler::member_call(node* n) {
    auto it = tosuto_dyn_cast(member_call_node*, n);

    tosuto_discard(compile(it->callee.get()));

    cur_ch().add(op_code::get_prop);
    auto lit = cur_ch().add_lit(value{value::str{it->field}});
    cur_ch().add(lit);

    tosuto_discard(compile(it->callee.get()));

    for (auto const& arg : it->args) {
      tosuto_discard(compile(arg.get()));
    }

    cur_ch().add(op_code::call);
    if (it->args.size() + 1 > max_of<u8>) {
      return std::unexpected{"Too many args in call!"};
    }

    cur_ch().add(u8(it->args.size() + 1));

    return {};
  }

  std::expected<void, std::string> compiler::anon_fn_def(node* n) {
    tosuto_discard(function(value::fn::type::fn, n));

    return {};
  }

  std::expected<void, std::string> compiler::object(node* n) {
    auto it = tosuto_dyn_cast(object_node*, n);
    auto proto = std::make_shared<value::object::element_type>();
    auto proto_lit = cur_ch().add_lit(value{proto});
    cur_ch().add(op_code::lit);
    cur_ch().add(proto_lit);
    for (auto const& [k, v] : it->fields) {
      tosuto_discard(compile(v.get()));
      cur_ch().add(op_code::def_prop);
      auto field_name = cur_ch().add_lit(value{value::str{k}});
      cur_ch().add(field_name);
    }

    return {};
  }

  std::expected<void, std::string> compiler::ret(node* n) {
    if (fn_type == value::fn::type::script) {
      return std::unexpected{"Can't return from top level!"};
    }

    auto it = tosuto_dyn_cast(ret_node*, n);

    if (it->ret_val) {
      tosuto_discard(compile(it->ret_val.get()));
      cur_ch().add(op_code::ret);
    } else {
      cur_ch().add(op_code::key_nil);
      cur_ch().add(op_code::ret);
    }

    return {};
  }

  std::expected<void, std::string> compiler::function(value::fn::type type, node* n) {
    auto it = tosuto_dyn_cast(fn_def_node*, n);

    compiler comp{type};
    comp.begin_block();

    auto& args = it->args;
    if (args.size() > max_of<u8>)
      return std::unexpected{"Too many arguments in function!"};
    comp.fn.arity = u8(args.size());
    comp.fn.name = it->name;
    comp.fn.ch->name = it->name;
    for (auto const& arg : args) {
      comp.fn.ref.push_back(arg.second);
      tosuto_discard(comp.add_local(arg.first));
    }

    tosuto_discard(comp.exp_or_block_no_pop(it->body.get()));

    comp.cur_ch().add(op_code::ret);

    cur_ch().add(op_code::lit);
    auto lit = cur_ch().add_lit(value{comp.fn});
    cur_ch().add(lit);

    return {};
  }

  std::expected<void, std::string> compiler::fn_def(node* n) {
    auto it = tosuto_dyn_cast(fn_def_node*, n);

    tosuto_discard(function(value::fn::type::fn, n));
    cur_ch().add(op_code::def_global);
    auto lit = cur_ch().add_lit(value{value::str{it->name}});
    cur_ch().add(lit);

    return {};
  }

  std::expected<void, std::string> compiler::call(node* n) {
    auto it = tosuto_dyn_cast(call_node*, n);

    if (it->is_member) {
      return std::unexpected{"Don't support member calls yet!"};
    }

    tosuto_discard(compile(it->callee.get()));

    for (auto const& arg : it->args) {
      tosuto_discard(compile(arg.get()));
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
      cur_ch().add(op_code::pop_loc);
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
    auto it = tosuto_dyn_cast(if_node*, n);

    const size_t invalid_patch = max_of<size_t>;
    size_t else_jump = invalid_patch;
    for (size_t i = 0; i < it->cases.size(); i++) {
      tosuto_discard(compile(it->cases[i].first.get()));
      size_t then_jump = emit_jump(op_code::jmpf_pop);

      tosuto_discard(exp_or_block_no_pop(it->cases[i].second.get()));
      if (i == it->cases.size() - 1) {
        else_jump = emit_jump(op_code::jmp);
      }

      tosuto_discard(patch_jump(then_jump));
    }

    if (it->else_case) {
      tosuto_discard(exp_or_block_no_pop(it->else_case.get()));
    }

    tosuto_discard(patch_jump(else_jump));

    return {};
  }

  std::expected<void, std::string> compiler::kw_literal(node* n) {
    auto it = tosuto_dyn_cast(kw_literal_node*, n);
    switch (it->lit) {
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(key_true)
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(key_false)
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(key_nil)
      default: return std::unexpected{"Unknown keyword literal at " + n->pretty(0)};
    }

    return {};
  }

  std::expected<void, std::string> compiler::add_local(std::string const& name) {
    auto str = value::str{name};
    if (locals.size() == max_locals) return std::unexpected{"Too many locals!"};
    for (auto const& it : std::ranges::reverse_view(locals)) {
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
    auto it = tosuto_dyn_cast(var_def_node*, n);

    tosuto_discard(compile(it->value.get()));

    if (depth > 0) {
      tosuto_discard(add_local(it->name));
    } else {
      cur_ch().add(op_code::def_global);
      auto lit = cur_ch().add_lit(value{value::str{it->name}});
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
    auto it = tosuto_dyn_cast(field_get_node*, n);
    if (it->target) {
      tosuto_discard(compile(it->target.get()));
      cur_ch().add(op_code::get_prop);
      auto field_name = cur_ch().add_lit(value{value::str{it->field}});
      cur_ch().add(field_name);
      return {};
    }

    auto try_get_local = resolve_local(it->field);
    std::pair<op_code, u16> op =
      try_get_local.has_value() ?
        std::make_pair(op_code::get_local, *try_get_local)
      : std::make_pair(op_code::get_global, cur_ch().add_lit(value{value::str{it->field}}));

    cur_ch().add(op.first);
    cur_ch().add(op.second);

    return {};
  }

  std::expected<void, std::string> compiler::number(node* n) {
    auto it = tosuto_dyn_cast(number_node*, n);
    cur_ch().add(op_code::lit);
    auto lit = cur_ch().add_lit(value{it->value});
    cur_ch().add(lit);
    return {};
  }

  std::expected<void, std::string> compiler::string(node* n) {
    auto it = tosuto_dyn_cast(string_node*, n);
    cur_ch().add(op_code::lit);
    auto lit = cur_ch().add_lit(value{value::str{it->value}});
    cur_ch().add(lit);
    return {};
  }

  std::expected<void, std::string> compiler::un_op(node* n) {
    auto it = tosuto_dyn_cast(un_op_node*, n);
    tosuto_discard(compile(it->target.get()));
    switch (it->op) {
      case tok_type::sub: {
        cur_ch().add(op_code::neg);
        break;
      }
      case tok_type::exclaim: {
        cur_ch().add(op_code::inv);
        break;
      }
      default: return std::unexpected{"Unknown unary operator at " + n->pretty(0)};
    }
    return {};
  }

  void compiler::pop_for_exp_stmt(node* exp) {
    switch (exp->type) {
      case node_type::var_def:
      case node_type::fn_def:
        break;
      default:
        cur_ch().add(op_code::pop);
    }
  }

  std::expected<void, std::string> compiler::basic_block(node* n, bool pop_last) {
    auto it = tosuto_dyn_cast(block_node*, n);
    size_t i = 0;
    auto last = it->exprs.size() - 1;
    for (auto const& exp : it->exprs) {
      tosuto_discard(compile(exp.get()));
      if (pop_last || i != last) {
        pop_for_exp_stmt(exp.get());
      }
    }

    return {};
  }

  std::expected<void, std::string> compiler::block(node* n) {
    begin_block();
    tosuto_discard(basic_block(n, true));
    end_block();
    return {};
  }

  std::expected<value::fn, std::string> compiler::global(node* n) {
    tosuto_discard(basic_block(n, true));

    cur_ch().add(op_code::ret);
    return fn;
  }

  compiler::compiler(value::fn::type type) : fn_type(type) {
    locals.emplace_back(value::str{""}, 0);
  }

  std::expected<void, std::string> compiler::exp_or_block_no_pop(node* n) {
    if (n->type == node_type::block) {
      return basic_block(n, false);
    } else {
      return compile(n);
    }
  }
}