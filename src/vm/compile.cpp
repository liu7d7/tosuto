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
  };

#define TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(op) case tok_type::op: ch.add(op_code::op); break;

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
      ch.add(op_code::set_global);
      auto lit = ch.add_lit(value{value::str{lhs->field}});
      ch.add(lit);

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
      case tok_type::neq: ch.add(op_code::eq); ch.add(op_code::inv); break;
      case tok_type::greater_than: ch.add(op_code::gt); break;
      case tok_type::less_than: ch.add(op_code::lt); break;
      case tok_type::greater_than_equal: ch.add(op_code::lt); ch.add(op_code::inv); break;
      case tok_type::less_than_equal: ch.add(op_code::gt); ch.add(op_code::inv); break;
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(sym_or)
      TOSUTO_SIMPLE_CVT_TOKTYPE_TO_INSTR(sym_and)
      default: return std::unexpected{"Unknown infix operator at " + n->pretty(0)};
    }

    return {};
  }

  std::expected<void, std::string> compiler::compile(node* n) {
    return (this->*compilers[n->type])(n);
  }

  void compiler::begin_block() {
    depth++;
  }

  void compiler::end_block() {
    depth--;

    auto it = int(locals.size() - 1);
    while (it != 0 && locals[it].depth > depth) {
      ch.add(op_code::pop);
      it--;
      locals.pop_back();
    }
  }

  size_t compiler::emit_jump(op_code type) {
    ch.add(type);
    ch.add(u8(0xff));
    ch.add(u8(0xff));
    return ch.data.size() - 2;
  }

  std::expected<void, std::string> compiler::patch_jump(size_t offset) {
    size_t jump = ch.data.size() - offset - 2;

    if (jump > max_of<u16>) {
      return std::unexpected{"Tried to jump farther than a u16 can store!"};
    }

    ch.data[offset] = u8(jump & 0xff);
    ch.data[offset + 1] = u8(jump >> 8 & 0xff);

    return {};
  }

  std::expected<void, std::string> compiler::if_stmt(node* n) {
    auto it = tosuto_dyn_cast(if_node*, n);

    const size_t invalid_patch = max_of<size_t>;
    size_t else_jump = invalid_patch;
    for (size_t i = 0; i < it->cases.size(); i++) {
      tosuto_discard(compile(it->cases[i].first.get()));
      size_t then_jump = emit_jump(op_code::jmpf);
      ch.add(op_code::pop);
      tosuto_discard(compile(it->cases[i].second.get()));
      if (i == it->cases.size() - 1) {
        else_jump = emit_jump(op_code::jmp);
      }

      tosuto_discard(patch_jump(then_jump));
      if (i != it->cases.size() - 1) {
        ch.add(op_code::pop);
      }
    }

    if (it->else_case) {
      tosuto_discard(compile(it->else_case.get()));
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
      ch.add(op_code::key_nil);
      tosuto_discard(add_local(it->name));
    } else {
      ch.add(op_code::def_global);
      auto lit = ch.add_lit(value{value::str{it->name}});
      ch.add(lit);
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
    if (it->target)
      return std::unexpected{"TODO: implement field get for fields of objs"};

    auto try_get_local = resolve_local(it->field);
    std::pair<op_code, u16> op =
      try_get_local.has_value() ?
        std::make_pair(op_code::get_local, *try_get_local)
      : std::make_pair(op_code::get_global, ch.add_lit(value{value::str{it->field}}));

    ch.add(op.first);
    ch.add(op.second);

    return {};
  }

  std::expected<void, std::string> compiler::number(node* n) {
    auto it = tosuto_dyn_cast(number_node*, n);
    ch.add(op_code::lit);
    auto lit = ch.add_lit(value{it->value});
    ch.add(lit);
    return {};
  }

  std::expected<void, std::string> compiler::string(node* n) {
    auto it = tosuto_dyn_cast(string_node*, n);
    ch.add(op_code::lit);
    auto lit = ch.add_lit(value{value::str{it->value}});
    ch.add(lit);
    return {};
  }

  std::expected<void, std::string> compiler::un_op(node* n) {
    auto it = tosuto_dyn_cast(un_op_node*, n);
    tosuto_discard(compile(it->target.get()));
    switch (it->op) {
      case tok_type::sub: {
        ch.add(op_code::neg);
        break;
      }
      case tok_type::exclaim: {
        ch.add(op_code::inv);
        break;
      }
      default: return std::unexpected{"Unknown unary operator at " + n->pretty(0)};
    }
    return {};
  }

  std::expected<void, std::string> compiler::block(node* n) {
    begin_block();
    auto it = tosuto_dyn_cast(block_node*, n);
    for (auto const& exp : it->exprs) {
      tosuto_discard(compile(exp.get()));
      ch.add(op_code::pop);
    }

    end_block();
    return {};
  }

  std::expected<void, std::string> compiler::global(node* n) {
    auto it = tosuto_dyn_cast(block_node*, n);
    auto len = it->exprs.size();
    auto i = 0;
    for (auto const& exp : it->exprs) {
      tosuto_discard(compile(exp.get()));
      i++;
      if (i != len) ch.add(op_code::pop);
    }

    ch.add(op_code::ret);
    return {};
  }
}