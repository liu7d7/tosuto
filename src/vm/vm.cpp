#include "vm.h"
#include <iomanip>

namespace tosuto::vm {
  void chunk::disasm(std::ostream& out, bool add_name) {
    if (add_name) out << name << ":\n";

    for (size_t offset = 0; offset < data.size();) {
      offset = disasm_instr(out, offset);
    }

    out << "\n";

    for (auto const& it : literals) {
      if (!it.is<value::fn>()) continue;
      auto& fn = it.get<value::fn>();
      out << name << "." << fn.name << ":\n";
      fn.ch->disasm(out, false);
      out << "\n";
    }
  }

  size_t chunk::disasm_instr(std::ostream& out, size_t idx) {
    auto off = std::to_string(idx);
    auto padding = std::string(4 - off.size(), '0');
    out << padding << off << ' ';

#define TOSUTO_DISASM_SIMPLE_INSTR(op) case op_code::op: \
  do { \
    out << std::left << std::setw(9) << #op << '\n'; \
    return idx + 1; \
  } while(false)

#define TOSUTO_DISASM_SIMPLE_INSTR_2(op, name) case op_code::op: \
  do { \
    out << std::left << std::setw(9) << #name << '\n'; \
    return idx + 1; \
  } while(false)

    switch (rd_op(idx)) {
      TOSUTO_DISASM_SIMPLE_INSTR(ret);
      TOSUTO_DISASM_SIMPLE_INSTR(neg);
      TOSUTO_DISASM_SIMPLE_INSTR(add);
      TOSUTO_DISASM_SIMPLE_INSTR(sub);
      TOSUTO_DISASM_SIMPLE_INSTR(mul);
      TOSUTO_DISASM_SIMPLE_INSTR(div);
      TOSUTO_DISASM_SIMPLE_INSTR(pop);
      TOSUTO_DISASM_SIMPLE_INSTR(pop_loc);
      TOSUTO_DISASM_SIMPLE_INSTR(eq);
      TOSUTO_DISASM_SIMPLE_INSTR(lt);
      TOSUTO_DISASM_SIMPLE_INSTR(gt);
      TOSUTO_DISASM_SIMPLE_INSTR(inv);
      TOSUTO_DISASM_SIMPLE_INSTR_2(key_true, true);
      TOSUTO_DISASM_SIMPLE_INSTR_2(key_false, false);
      TOSUTO_DISASM_SIMPLE_INSTR_2(key_nil, nil);
      case op_code::lit: {
        out << std::left << std::setw(9) << "lit" << literal(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::get_global: {
        out << std::left << std::setw(9) << "get_glob" << literal(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::set_global: {
        out << std::left << std::setw(9) << "set_glob" << literal(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::def_global: {
        out << std::left << std::setw(9) << "def_glob" << literal(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::get_local: {
        out << std::left << std::setw(9) << "get_loc" << rd_u16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::set_local: {
        out << std::left << std::setw(9) << "set_loc" << rd_u16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::def_prop: {
        out << std::left << std::setw(9) << "def_prop" << literal(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::get_prop: {
        out << std::left << std::setw(9) << "get_prop" << literal(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::jmpf: {
        out << std::left << std::setw(9) << "jmpf"
            << std::left << std::setw(10) << rd_u16(idx + 1)
            << "(" << idx << "->" << idx + 3 + rd_u16(idx + 1) << ")" << '\n';
        return idx + 3;
      }
      case op_code::jmp: {
        out << std::left << std::setw(9) << "jmp"
            << std::left << std::setw(10) << rd_u16(idx + 1)
            << "(" << idx << "->" << idx + 3 + rd_u16(idx + 1) << ")" << '\n';
        return idx + 3;
      }
      case op_code::jmpf_pop: {
        out << std::left << std::setw(9) << "jmpf_pop"
            << std::left << std::setw(10) << rd_u16(idx + 1)
            << "(" << idx << "->" << idx + 3 + rd_u16(idx + 1) << ")" << '\n';
        return idx + 3;
      }
      case op_code::call: {
        out << std::left << std::setw(9) << "call" << std::to_string(rd_u8(idx + 1)) << '\n';
        return idx + 2;
      }
      default: {
        out << std::left << std::setw(9) << "uh oh!\n";
        return std::numeric_limits<size_t>::max();
      }
    }
  }

  std::expected<void, std::string> vm::run(std::ostream& out) {
#define TOSUTO_BIN_OP(op) \
  do { \
    auto b = tosuto_unwrap_move_fast(pop()); \
    auto a = tosuto_unwrap_move_fast(pop()); \
    if (a.is<value::num>() && b.is<value::num>()) { \
      tosuto_discard_fast(push(value{a.get<value::num>() op b.get<value::num>()})); \
    } else               \
      return std::unexpected{"Couldn't do " + a.to_string() + #op + b.to_string()}; \
  } while(false)

    auto* frame = &frames.back();

    for (;;) {

#ifndef NDEBUG
      if (!stack.empty())
        out << stack << '\n';
      else out << "[ ]\n";
      frame->fn.ch->disasm_instr(out, frame->ip);
      out << '\n' << std::flush;
#endif
      switch (rd_op()) {
        case op_code::ret: {
          auto result = tosuto_unwrap_move_fast(pop());
          auto last_frame = frames.back();
          frames.pop_back();
          if (frames.empty()) {
            return {};
          }

          frame = &frames.back();
          stack.resize(last_frame.offset);
          tosuto_discard_fast(push(value{result}));
          break;
        }
        case op_code::lit: {
          tosuto_discard_fast(push(rd_lit()));
          break;
        }
        case op_code::pop_loc:
        case op_code::pop: {
          tosuto_discard_fast(pop());
          break;
        }
        case op_code::neg: {
          value a = tosuto_unwrap_move_fast(pop());
          if (a.is<value::num>()) {
            tosuto_discard_fast(push(value{-a.get<value::num>()}));
          } else {
            return std::unexpected{
              "Tried to negate smth that was not a number!"};
          }
          break;
        }
        case op_code::add: {
          value b = tosuto_unwrap_move_fast(pop());
          value a = tosuto_unwrap_move_fast(pop());
          if (a.is<value::num>() && b.is<value::num>()) {
            tosuto_discard_fast(push(value{a.get<value::num>() + b.get<value::num>()}));
          } else if (a.is<value::str>() && b.is<value::str>()) {
            tosuto_discard_fast(push(value{a.get<value::str>() + b.get<value::str>()}));
          } else {
            return std::unexpected{
              "Couldn't do " + a.to_string() + " + " + b.to_string()};
          }
          break;
        }
        case op_code::sub:
          TOSUTO_BIN_OP(-);
          break;
        case op_code::mul:
          TOSUTO_BIN_OP(*);
          break;
        case op_code::div:
          TOSUTO_BIN_OP(/);
          break;
        case op_code::lt:
          TOSUTO_BIN_OP(<);
          break;
        case op_code::gt:
          TOSUTO_BIN_OP(>);
          break;
        case op_code::mod: {
          value b = tosuto_unwrap_move_fast(pop());
          value a = tosuto_unwrap_move_fast(pop());
          if (a.is<value::num>() && b.is<value::num>()) {
            tosuto_discard_fast(push(value{fmod(a.get<value::num>(), b.get<value::num>())}));
          } else {
            return std::unexpected{
              "Couldn't do " + a.to_string() + " % " + b.to_string()};
          }
          break;
        }
        case op_code::eq: {
          value b = tosuto_unwrap_move_fast(pop());
          value a = tosuto_unwrap_move_fast(pop());
          tosuto_discard_fast(push(value{a.eq(b)}));
          break;
        }
        case op_code::inv: {
          value a = tosuto_unwrap_move_fast(pop());
          tosuto_discard_fast(push(value{!a.is_truthy()}));
          break;
        }
        case op_code::key_false: tosuto_discard_fast(push(value{false}));
          break;
        case op_code::key_true: tosuto_discard_fast(push(value{true}));
          break;
        case op_code::key_nil: tosuto_discard_fast(push(value{value::nil{}}));
          break;
        case op_code::set_global: {
          auto name = rd_lit().get<value::str>();
          auto it = globals.find(name);
          if (it != globals.end()) {
            it->second = peek();
          } else {
            return std::unexpected{"Could not find " + std::string(name) + " in globals!"};
          }

          break;
        }
        case op_code::get_global: {
          auto name = rd_lit().get<value::str>();
          auto it = globals.find(name);
          if (it != globals.end()) {
            tosuto_discard_fast(push(value{it->second}));
          } else {
            return std::unexpected{"Could not find " + std::string(name) + " in globals!"};
          }

          break;
        }
        case op_code::def_global: {
          auto name = rd_lit().get<value::str>();
          globals[name] = tosuto_unwrap_move_fast(pop());
          break;
        }
        case op_code::get_local: {
          auto slot = rd_u16();
          tosuto_discard_fast(push(value{stack[frame->offset + slot]}));
          break;
        }
        case op_code::set_local: {
          auto slot = rd_u16();
          stack[frame->offset + slot] = peek();
          break;
        }
        case op_code::jmpf: {
          u16 off = rd_u16();
          if (!peek().is_truthy()) frame->ip += off;
          break;
        }
        case op_code::jmp: {
          u16 off = rd_u16();
          frame->ip += off;
          break;
        }
        case op_code::jmpf_pop: {
          u16 off = rd_u16();
          value it = tosuto_unwrap_move_fast(pop());
          if (!it.is_truthy()) frame->ip += off;
          break;
        }
        case op_code::call: {
          u8 arity = rd_u8();
          tosuto_discard_fast(call(peek(arity), arity));
          frame = &frames.back();
          break;
        }
        case op_code::def_prop: {
          value::str name = rd_lit().get<value::str>();
          value field_val = tosuto_unwrap_move_fast(pop());
          value obj = peek();
          auto& obj_fields = obj.get<value::object>();
          obj_fields->emplace(name, field_val);
          break;
        }
        case op_code::get_prop: {
          value::str name = rd_lit().get<value::str>();
          value obj = tosuto_unwrap_move_fast(pop());
          auto& obj_fields = obj.get<value::object>();
          auto it = obj_fields->find(name);
          if (it == obj_fields->end()) {
            return std::unexpected{"Failed to find " + std::string(name) + " in " + obj.to_string()};
          }

          tosuto_discard_fast(push(value{it->second}));
          break;
        }
      }
    }
  }

  std::expected<void, std::string>
  vm::call(value& callee, u8 arity) {
    if (callee.is<value::native_fn>()) {
      auto fn = callee.get<value::native_fn>();
      if (arity != fn.second) {
        return std::unexpected{
          "Non-matching # of arguments! (exp: "
          + std::to_string(fn.second)
          + ", got: " + std::to_string(arity) + ")"};
      }

      auto res = tosuto_unwrap_move_fast(fn.first(std::span{stack}.subspan(stack.size() - arity, arity)));
      stack.resize(stack.size() - arity - 1);
      tosuto_discard_fast(push(value{res}));
      return {};
    }

    if (callee.is<value::fn>()) {
      auto& fn = callee.get<value::fn>();
      if (fn.arity != arity) {
        return std::unexpected{
          "Non-matching # of arguments! (exp: "
          + std::to_string(fn.arity)
          + ", got: " + std::to_string(arity) + ")"};
      }

      frames.emplace_back(fn, 0, stack.size() - arity - 1);

      return {};
    }

    return std::unexpected{"Only allowed to call functions atm"};
  }

  void
  vm::def_native(const std::string& name, value::native_fn::second_type arity,
                 value::native_fn::first_type fn) {
    globals[value::str{name}] = value{std::make_pair(fn, arity)};
  }

  call_frame::call_frame(value::fn& fn, size_t ip, size_t offset)
    : fn(fn), ip(ip), offset(offset) {}
}