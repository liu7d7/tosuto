#include "vm.h"
#include <iomanip>
#include <iostream>

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
      out << name << "." << std::string(fn.name) << ":\n";
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
      TOSUTO_DISASM_SIMPLE_INSTR(mod);
      TOSUTO_DISASM_SIMPLE_INSTR(pop);
      TOSUTO_DISASM_SIMPLE_INSTR(pop_loc);
      TOSUTO_DISASM_SIMPLE_INSTR(eq);
      TOSUTO_DISASM_SIMPLE_INSTR(lt);
      TOSUTO_DISASM_SIMPLE_INSTR(gt);
      TOSUTO_DISASM_SIMPLE_INSTR(inv);
      TOSUTO_DISASM_SIMPLE_INSTR(ld_0);
      TOSUTO_DISASM_SIMPLE_INSTR(ld_1);
      TOSUTO_DISASM_SIMPLE_INSTR(new_obj);
      TOSUTO_DISASM_SIMPLE_INSTR(szd_arr);
      TOSUTO_DISASM_SIMPLE_INSTR(get_idx);
      TOSUTO_DISASM_SIMPLE_INSTR(set_idx);
      TOSUTO_DISASM_SIMPLE_INSTR_2(key_true, true);
      TOSUTO_DISASM_SIMPLE_INSTR_2(key_with, with);
      TOSUTO_DISASM_SIMPLE_INSTR_2(key_false, false);
      TOSUTO_DISASM_SIMPLE_INSTR_2(key_nil, nil);
      case op_code::lit_16: {
        out << std::left << std::setw(9) << "lit_16" << lit_16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::lit_8: {
        out << std::left << std::setw(9) << "lit_8" << lit_8(idx + 1) << '\n';
        return idx + 2;
      }
      case op_code::get_global: {
        out << std::left << std::setw(9) << "get_glob" << lit_16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::set_global: {
        out << std::left << std::setw(9) << "set_glob" << lit_16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::def_global: {
        out << std::left << std::setw(9) << "def_glob" << lit_16(idx + 1)
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
      case op_code::array: {
        out << std::left << std::setw(9) << "array" << rd_u16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::def_prop: {
        out << std::left << std::setw(9) << "def_prop" << lit_16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::get_prop: {
        out << std::left << std::setw(9) << "get_prop" << lit_16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::set_prop: {
        out << std::left << std::setw(9) << "set_prop" << lit_16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::jmpb_pop: {
        out << std::left << std::setw(9) << "jmpb_pop"
            << std::left << std::setw(10) << rd_u16(idx + 1)
            << "(" << idx << "->" << idx + 3 - rd_u16(idx + 1) << ")" << '\n';
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
  do {                    \
    static value::str op_name = value::str{#op};                      \
    auto b = std::move(pop_top); \
    auto a = std::move(pop_top); \
    if (a.is<value::num>() && b.is<value::num>()) { \
      inc_top = value{a.get<value::num>() op b.get<value::num>()}; \
    } else if (a.is<value::object>() && a.get<value::object>()->contains(op_name)) {    \
      inc_top = value{a.get<value::object>()->at(op_name)};        \
      inc_top = value{a};                        \
      inc_top = std::move(b);                        \
      frames.emplace_back(a.get<value::object>()->at(op_name).get<value::fn>(), 0, stack.size() - 2 - 1);\
    } else              \
      return std::unexpected{"Couldn't do " + a.to_string() + #op + b.to_string()}; \
  } while(false)

    auto* frame = &frames.back();
    u8* ip = frame->fn.ch->data.data();
    std::stack<u8*> ip_stack{};
    value* lits = frame->fn.ch->literals.data();
    size_t frame_offset = 0;
    value* stack_top = stack.data();
    auto update_stack_frame = [this, &ip, &frame, &lits, &frame_offset, &ip_stack](bool pop) {
      frame = &frames.back();
      if (!pop) {
        ip_stack.push(ip);
        ip = frame->fn.ch->data.data();
      } else {
        auto it = ip_stack.top();
        ip_stack.pop();
        ip = it;
      }
      lits = frame->fn.ch->literals.data();
      frame_offset = frame->offset;
    };

#define stack_size size_t(stack_top - stack.data() + 1)
#define inc_top (*(++stack_top))
#define peek_top (*stack_top)
#define peek_off_top(idx) (stack_top[-(idx)])
#define pop_top (*stack_top--)
#define rd_u16() (ip += 2, u16(ip[-2]) + u16(ip[-1] << 8))
#define rd_u8() (*ip++)
#define rd_lit_16() (ip += 2, lits[u16(ip[-2]) + u16(ip[-1] << 8)])
#define rd_lit_8() (lits[*ip++])
#define rd_op() (op_code(*ip++))

    for (;;) {
#ifndef NDEBUG
      if (!stack.empty()) {
        out << "[ ";
        for (value* it = stack.data(); it <= stack_top; it++) {
          out << it->to_string() << " ";
        }
        out << "]\n";
      }
      else out << "[ ]\n";
      frame->fn.ch->disasm_instr(out, ip - frame->fn.ch->data.data());
      out << '\n';
#endif

      switch (rd_op()) {
        case op_code::ret: {
          auto result = std::move(pop_top);
          auto last_frame = frames.back();
          frames.pop_back();
          if (frames.empty()) {
            return {};
          }

          update_stack_frame(true);
          stack_top = stack.data() + last_frame.offset - 1;
          inc_top = std::move(result);
          break;
        }
        case op_code::ld_0: {
          inc_top = value{0.0};
          break;
        }
        case op_code::ld_1: {
          inc_top = value{1.0};
          break;
        }
        case op_code::lit_16: {
          inc_top = rd_lit_16();
          break;
        }
        case op_code::lit_8: {
          inc_top = rd_lit_8();
          break;
        }
        case op_code::pop_loc:
        case op_code::pop: {
          stack_top--;
          break;
        }
        case op_code::neg: {
          value a = std::move(pop_top);
          if (a.is<value::num>()) {
            inc_top = value{-a.get<value::num>()};
          } else {
            return std::unexpected{
              "Tried to negate smth that was not a number!"};
          }
          break;
        }
        case op_code::add: {
          static value::str op_name = value::str{"+"};
          value b = std::move(pop_top);
          value a = std::move(pop_top);
          if (a.is<value::num>() && b.is<value::num>()) {
            inc_top = value{a.get<value::num>() + b.get<value::num>()};
          } else if (a.is<value::str>() && b.is<value::str>()) {
            inc_top = value{a.get<value::str>() + b.get<value::str>()};
          } else if (a.is<value::object>() && a.get<value::object>()->contains(op_name)) {    \
            inc_top = value{a.get<value::object>()->at(op_name)};        \
            inc_top = a;                        \
            inc_top = std::move(b);                        \
            frames.emplace_back(a.get<value::object>()->at(op_name).get<value::fn>(), 0, stack.size() - 2 - 1);\
            update_stack_frame(false);
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
          value b = std::move(pop_top);
          value a = std::move(pop_top);
          if (a.is<value::num>() && b.is<value::num>()) {
            inc_top = value{fmod(a.get<value::num>(), b.get<value::num>())};
          } else {
            return std::unexpected{
              "Couldn't do " + a.to_string() + " % " + b.to_string()};
          }
          break;
        }
        case op_code::eq: {
          value b = std::move(pop_top);
          value a = std::move(pop_top);
          inc_top = value{a.eq(b)};
          break;
        }
        case op_code::inv: {
          value a = std::move(pop_top);
          inc_top = value{!a.is_truthy()};
          break;
        }
        case op_code::key_false: inc_top = value{false};
          break;
        case op_code::key_true: inc_top = value{true};
          break;
        case op_code::key_nil: inc_top = value{value::nil{}};
          break;
        case op_code::set_global: {
          auto name = rd_lit_16().get<value::str>();
          auto it = globals.find(name);
          if (it != globals.end()) {
            it->second = peek_top;
          } else {
            return std::unexpected{"Could not find " + std::string(name) + " in globals!"};
          }

          break;
        }
        case op_code::get_global: {
          auto name = rd_lit_16().get<value::str>();
          auto it = globals.find(name);
          if (it != globals.end()) {
            inc_top = it->second;
          } else {
            return std::unexpected{"Could not find " + std::string(name) + " in globals!"};
          }

          break;
        }
        case op_code::def_global: {
          auto name = rd_lit_16().get<value::str>();
          globals[name] = std::move(pop_top);
          break;
        }
        case op_code::get_local: {
          auto slot = rd_u16();
          inc_top = stack[frame_offset + slot];
          break;
        }
        case op_code::set_local: {
          auto slot = rd_u16();
          stack[frame_offset + slot] = peek_top;
          break;
        }
        case op_code::jmpf: {
          u16 off = rd_u16();
          if (!peek_top.is_truthy()) ip += off;
          break;
        }
        case op_code::jmp: {
          u16 off = rd_u16();
          ip += off;
          break;
        }
        case op_code::jmpf_pop: {
          u16 off = rd_u16();
          value it = std::move(pop_top);
          if (!it.is_truthy()) ip += off;
          break;
        }
        case op_code::jmpb_pop: {
          u16 off = rd_u16();
          value it = std::move(pop_top);
          if (it.is_truthy()) ip -= off;
          break;
        }
        case op_code::call: {
          u8 arity = rd_u8();
          tosuto_discard_fast(call(peek_off_top(arity), arity, stack_top, update_stack_frame));
          break;
        }
        case op_code::new_obj: {
          inc_top = value{std::make_unique<value::object::element_type>()};
          break;
        }
        case op_code::def_prop: {
          value::str name = rd_lit_16().get<value::str>();
          value field_val = std::move(pop_top);
          value obj = peek_top;
          auto& obj_fields = obj.get<value::object>();
          obj_fields->operator[](name) = field_val;
          break;
        }
        case op_code::get_prop: {
          value::str name = rd_lit_16().get<value::str>();
          value obj = std::move(pop_top);
          auto& obj_fields = obj.get<value::object>();
          auto it = obj_fields->find(name);
          if (it == obj_fields->end()) {
            return std::unexpected{"Failed to find " + std::string(name) + " in " + obj.to_string()};
          }

          inc_top = it->second;
          break;
        }
        case op_code::set_prop: {
          value::str name = rd_lit_16().get<value::str>();
          value val = std::move(pop_top);
          value obj = std::move(pop_top);
          auto& obj_fields = obj.get<value::object>();

          obj_fields->operator[](name) = val;
          inc_top = std::move(val);
          break;
        }
        case op_code::get_idx: {
          value index = std::move(pop_top);
          value array = std::move(pop_top);

          if (!index.is<value::num>() || !array.is<value::array>()) {
            return std::unexpected{"Can't perform " + array.to_string() + "[" + index.to_string() + "]"};
          }

          inc_top = value{array.get<value::array>()->at(
            size_t(floor(index.get<value::num>())))};
          break;
        }
        case op_code::szd_arr: {
          value val = std::move(pop_top);
          value size = std::move(pop_top);

          if (!size.is<value::num>()) {
            return std::unexpected{"Size of szd_arr not a number!"};
          }

          (*stack_top++) = value{std::make_shared<value::array::element_type>(size_t(size.get<value::num>()), val)};
          break;
        }
        case op_code::key_with: {
          value b = std::move(pop_top);
          value a = std::move(pop_top);

          if (!a.is<value::object>() || !b.is<value::object>()) {
            return std::unexpected{"Can't do " + a.to_string() + " with " + b.to_string()};
          }

          auto a_fields = *a.get<value::object>();
          auto& b_fields = *b.get<value::object>();

          for (auto const& [k, v] : b_fields) {
            a_fields[k] = v;
          }

          inc_top =
            value{std::make_shared<value::object::element_type>(a_fields)};

          break;
        }
        case op_code::set_idx: {
          value val = std::move(pop_top);
          value index = std::move(pop_top);
          value array = std::move(pop_top);

          if (!index.is<value::num>() || !array.is<value::array>()) {
            return std::unexpected{"Can't perform " + array.to_string() + "[" + index.to_string() + "]"};
          }

          array.get<value::array>()->at(size_t(floor(index.get<value::num>()))) = val;
          inc_top = std::move(val);
          break;
        }
        case op_code::array: {
          u16 size = rd_u16();

          auto val = value{std::make_shared<value::array::element_type>(stack_top - size + 1, stack_top + 1)};

          stack_top -= size;
          inc_top = std::move(val);
          break;
        }
      }
    }
  }

  std::expected<void, std::string>
  vm::call(value& callee, u8 arity, value*& stack_top, std::function<void(bool)> const& update_stack_frame) {
    if (callee.is<value::native_fn>()) {
      auto fn = callee.get<value::native_fn>();
      if (arity != fn.second) {
        return std::unexpected{
          "Non-matching # of arguments! (exp: "
          + std::to_string(fn.second)
          + ", got: " + std::to_string(arity) + ")"};
      }

      auto res = tosuto_unwrap_move_fast(fn.first(std::span{stack_top + 1 - arity, stack_top + 1}));
      stack_top -= arity;
      stack_top--;
      inc_top = res;
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

      frames.emplace_back(fn, 0, stack_size - arity - 1);
      update_stack_frame(false);

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