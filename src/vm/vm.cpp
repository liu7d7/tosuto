#include "vm.h"
#include <iomanip>
#include <iostream>

namespace tosuto::vm {
  void chunk::disasm(std::ostream& out, bool add_name) {
    if (add_name) out << std::string(name) << ":\n";

    for (size_t offset = 0; offset < data.size();) {
      offset = disasm_instr(out, offset);
    }

    out << "\n";

    for (auto const& it: literals) {
      if (!it.is<value::function>()) continue;
      auto& fn = it.get<value::function>();
      out << std::string(name) << "." << std::string(fn.desc->chunk.name) << ":\n";
      fn.desc->chunk.disasm(out, false);
      out << "\n";
    }
  }

  size_t chunk::disasm_instr(std::ostream& out, size_t idx) {
    auto off = std::to_string(idx);
    auto padding = std::string(4 - off.size(), '0');
    out << padding << off << ' ';

#define AMI_DISASM_SIMPLE_INSTR(op) case op_code::op: \
  do { \
    out << std::left << std::setw(9) << #op << '\n'; \
    return idx + 1; \
  } while(false)

#define AMI_DISASM_SIMPLE_INSTR_2(op, name) case op_code::op: \
  do { \
    out << std::left << std::setw(9) << #name << '\n'; \
    return idx + 1; \
  } while(false)

    switch (rd_op(idx)) {
      AMI_DISASM_SIMPLE_INSTR(ret);
      AMI_DISASM_SIMPLE_INSTR(neg);
      AMI_DISASM_SIMPLE_INSTR(add);
      AMI_DISASM_SIMPLE_INSTR(sub);
      AMI_DISASM_SIMPLE_INSTR(mul);
      AMI_DISASM_SIMPLE_INSTR(div);
      AMI_DISASM_SIMPLE_INSTR(mod);
      AMI_DISASM_SIMPLE_INSTR(pop);
      AMI_DISASM_SIMPLE_INSTR(pop_loc);
      AMI_DISASM_SIMPLE_INSTR(eq);
      AMI_DISASM_SIMPLE_INSTR(lt);
      AMI_DISASM_SIMPLE_INSTR(gt);
      AMI_DISASM_SIMPLE_INSTR(inv);
      AMI_DISASM_SIMPLE_INSTR(ld_0);
      AMI_DISASM_SIMPLE_INSTR(ld_1);
      AMI_DISASM_SIMPLE_INSTR(new_obj);
      AMI_DISASM_SIMPLE_INSTR(szd_arr);
      AMI_DISASM_SIMPLE_INSTR(idx_g);
      AMI_DISASM_SIMPLE_INSTR(idx_s);
      AMI_DISASM_SIMPLE_INSTR(upval_c);
      AMI_DISASM_SIMPLE_INSTR_2(key_true, true);
      AMI_DISASM_SIMPLE_INSTR_2(key_with, with);
      AMI_DISASM_SIMPLE_INSTR_2(key_false, false);
      AMI_DISASM_SIMPLE_INSTR_2(key_nil, nil);
      case op_code::lit_16: {
        out << std::left << std::setw(9) << "lit_16" << lit_16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::lit_8: {
        out << std::left << std::setw(9) << "lit_8" << lit_8(idx + 1) << '\n';
        return idx + 2;
      }
      case op_code::glob_g: {
        out << std::left << std::setw(9) << "glob_g" << lit_16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::glob_s: {
        out << std::left << std::setw(9) << "glob_s" << lit_16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::glob_d: {
        out << std::left << std::setw(9) << "glob_d" << lit_16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::loc_g: {
        out << std::left << std::setw(9) << "loc_g" << rd_u16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::loc_s: {
        out << std::left << std::setw(9) << "loc_s" << rd_u16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::upval_g: {
        out << std::left << std::setw(9) << "upval_g" << rd_u16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::upval_s: {
        out << std::left << std::setw(9) << "upval_s" << rd_u16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::array: {
        out << std::left << std::setw(9) << "array" << rd_u16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::prop_d: {
        out << std::left << std::setw(9) << "prop_d" << lit_16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::prop_g: {
        out << std::left << std::setw(9) << "prop_g" << lit_16(idx + 1)
            << '\n';
        return idx + 3;
      }
      case op_code::prop_s: {
        out << std::left << std::setw(9) << "prop_s" << lit_16(idx + 1)
            << '\n';
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
        out << std::left << std::setw(9) << "call"
            << std::to_string(rd_u8(idx + 1)) << '\n';
        return idx + 2;
      }
      case op_code::closure: {
        out << std::left << std::setw(9) << "closure"
            << lit_16(idx + 1).to_string() << '\n';

        static auto pad_to_operands = std::string(5, ' ');
        u16 num_upvals = rd_u16(idx + 3);

        for (size_t i = idx + 5; i < idx + 5 + num_upvals * 3; i += 3) {
          u8 is_local = rd_u8(i);
          u16 index = rd_u16(i + 1);
          std::cout << pad_to_operands << "| " << (is_local ? "local " : "upvalue ") << std::to_string(index) << '\n';
        }

        return idx + 5 + num_upvals * 3;
      }
      default: {
        out << std::left << std::setw(9) << "uh oh!\n";
        return std::numeric_limits<size_t>::max();
      }
    }
  }

  std::expected<void, std::string> vm::run(std::ostream& out) {
#define AMI_BIN_OP(op) \
  do {                    \
    static value::str op_name = value::str{#op};                      \
    auto b = pop_top(); \
    auto a = pop_top(); \
    if (a.is<value::num>() && b.is<value::num>()) { \
      push_top() = value{a.get<value::num>() op b.get<value::num>()}; \
    } else if (a.is<value::object>() && a.get<value::object>()->contains(op_name)) {    \
      push_top() = value{a.get<value::object>()->at(op_name)};        \
      push_top() = value{a};                        \
      push_top() = std::move(b);                        \
      frames.emplace_back(a.get<value::object>()->at(op_name).get<value::function>(), 0, stack.size() - 2 - 1);\
    } else              \
      return std::unexpected{"Couldn't do " + a.to_string() + #op + b.to_string()}; \
  } while(false)

    auto* frame = &frames.back();
    u8* ip = frame->fn.desc->chunk.data.data();
    u8** ip_stack;
    u8* internal[max_of<u8>];
    ip_stack = &internal[0];
    value* lits = frame->fn.desc->chunk.literals.data();
    size_t frame_offset = 0;
    value* stack_top = stack.data();
    auto update_stack_frame =
      [this, &ip, &frame, &lits, &frame_offset, &ip_stack](bool pop) {
        frame = &frames.back();
        if (!pop) {
          *(ip_stack++) = ip;
          ip = frame->fn.desc->chunk.data.data();
        } else {
          ip = *--ip_stack;
        }
        lits = frame->fn.desc->chunk.literals.data();
        frame_offset = frame->offset;
      };

#define push_top() (*(++stack_top))
#define peek_top() (*stack_top)
#define peek_off_top(idx) (stack_top[-(idx)])
#define pop_top() std::move(*stack_top--)
#define rd_u16() (ip += 2, *(u16*)(&ip[-2]))
#define rd_u8() (*ip++)
#define rd_lit_16() (ip += 2, lits[*(u16*)(&ip[-2])])
#define rd_lit_8() (lits[*ip++])
#define rd_op() (op_code(*ip++))

    for (;;) {
#ifndef NDEBUG
      out << "[ ";
      for (value* it = stack.data(); it <= stack_top; it++) {
        out << it->to_string() << " ";
      }
      out << "]\n";
      frame->function.desc->first.disasm_instr(out, ip - frame->function.desc->first.data.data());
      out << '\n';
#endif

      switch (rd_op()) {
        case op_code::ret: {
          auto result = pop_top();
          close_upvals(stack.data() + frame_offset - 1);
          auto last_frame = frames.back();
          frames.pop_back();
          if (frames.empty()) {
            return {};
          }

          update_stack_frame(true);
          stack_top = stack.data() + last_frame.offset - 1;
          push_top() = std::move(result);
          break;
        }
        case op_code::ld_0: {
          push_top() = value{0.0};
          break;
        }
        case op_code::ld_1: {
          push_top() = value{1.0};
          break;
        }
        case op_code::lit_16: {
          push_top() = rd_lit_16();
          break;
        }
        case op_code::lit_8: {
          push_top() = rd_lit_8();
          break;
        }
        case op_code::pop_loc:
        case op_code::pop: {
          stack_top--;
          break;
        }
        case op_code::neg: {
          value a = pop_top();
          if (a.is<value::num>()) {
            push_top() = value{-a.get<value::num>()};
          } else {
            return std::unexpected{
              "Tried to negate smth that was not a number!"};
          }
          break;
        }
        case op_code::add: {
          static value::str op_name = value::str{"+"};
          value b = pop_top();
          value a = pop_top();
          if (a.is<value::num>() && b.is<value::num>()) {
            push_top() = value{a.get<value::num>() + b.get<value::num>()};
          } else if (a.is<value::str>() && b.is<value::str>()) {
            push_top() = value{a.get<value::str>() + b.get<value::str>()};
          } else if (a.is<value::object>() &&
                     a.get<value::object>()->contains(op_name)) {
            \
            push_top() = value{a.get<value::object>()->at(op_name)};        \
            push_top() = a;                        \
            push_top() = std::move(b);                        \
            frames.emplace_back(
              a.get<value::object>()->at(op_name).get<value::function>(), 0,
              stack.size() - 2 - 1);\
            update_stack_frame(false);
          } else {
            return std::unexpected{
              "Couldn't do " + a.to_string() + " + " + b.to_string()};
          }
          break;
        }
        case op_code::sub:AMI_BIN_OP(-);
          break;
        case op_code::mul:AMI_BIN_OP(*);
          break;
        case op_code::div:AMI_BIN_OP(/);
          break;
        case op_code::lt:AMI_BIN_OP(<);
          break;
        case op_code::gt:AMI_BIN_OP(>);
          break;
        case op_code::mod: {
          static value::str op_name = value::str{"%"};
          value b = pop_top();
          value a = pop_top();
          if (a.is<value::num>() && b.is<value::num>()) {
            push_top() = value{fmod(a.get<value::num>(), b.get<value::num>())};
          } else if (a.is<value::object>() &&
                     a.get<value::object>()->contains(op_name)) {
            \
            push_top() = value{a.get<value::object>()->at(op_name)};        \
            push_top() = a;                        \
            push_top() = std::move(b);                        \
            frames.emplace_back(
              a.get<value::object>()->at(op_name).get<value::function>(), 0,
              stack.size() - 2 - 1);\
            update_stack_frame(false);
          } else {
            return std::unexpected{
              "Couldn't do " + a.to_string() + " % " + b.to_string()};
          }
          break;
        }
        case op_code::eq: {
          value b = pop_top();
          value a = pop_top();
          push_top() = value{a.eq(b)};
          break;
        }
        case op_code::inv: {
          value a = pop_top();
          push_top() = value{!a.is_truthy()};
          break;
        }
        case op_code::key_false: push_top() = value{false};
          break;
        case op_code::key_true: push_top() = value{true};
          break;
        case op_code::key_nil: push_top() = value{value::nil{}};
          break;
        case op_code::glob_s: {
          auto name = rd_lit_16().get<value::str>();
          auto it = globals.find(name);
          if (it != globals.end()) {
            it->second = peek_top();
          } else {
            return std::unexpected{
              "Could not find " + std::string(name) + " in globals!"};
          }

          break;
        }
        case op_code::glob_g: {
          auto name = rd_lit_16().get<value::str>();
          auto it = globals.find(name);
          if (it != globals.end()) {
            push_top() = it->second;
          } else {
            return std::unexpected{
              "Could not find " + std::string(name) + " in globals!"};
          }

          break;
        }
        case op_code::glob_d: {
          auto name = rd_lit_16().get<value::str>();
          globals[name] = pop_top();
          break;
        }
        case op_code::loc_g: {
          auto slot = rd_u16();
          push_top() = stack[frame_offset + slot];
          break;
        }
        case op_code::loc_s: {
          auto slot = rd_u16();
          stack[frame_offset + slot] = peek_top();
          break;
        }
        case op_code::jmpf: {
          u16 off = rd_u16();
          if (!peek_top().is_truthy()) ip += off;
          break;
        }
        case op_code::jmp: {
          u16 off = rd_u16();
          ip += off;
          break;
        }
        case op_code::jmpf_pop: {
          u16 off = rd_u16();
          value it = pop_top();
          if (!it.is_truthy()) ip += off;
          break;
        }
        case op_code::jmpb_pop: {
          u16 off = rd_u16();
          value it = pop_top();
          if (it.is_truthy()) ip -= off;
          break;
        }
        case op_code::call: {
          u8 arity = rd_u8();
          ami_discard_fast(
            call(peek_off_top(arity), arity, stack_top, update_stack_frame));
          break;
        }
        case op_code::new_obj: {
          push_top() = value{std::make_unique<value::object::element_type>()};
          break;
        }
        case op_code::prop_d: {
          value::str name = rd_lit_16().get<value::str>();
          value field_val = pop_top();
          value obj = peek_top();
          auto& obj_fields = obj.get<value::object>();
          obj_fields->operator[](name) = field_val;
          break;
        }
        case op_code::prop_g: {
          value::str name = rd_lit_16().get<value::str>();
          value obj = pop_top();
          auto& obj_fields = obj.get<value::object>();
          auto it = obj_fields->find(name);
          if (it == obj_fields->end()) {
            return std::unexpected{
              "Failed to find " + std::string(name) + " in " + obj.to_string()};
          }

          push_top() = it->second;
          break;
        }
        case op_code::prop_s: {
          value::str name = rd_lit_16().get<value::str>();
          value val = pop_top();
          value obj = pop_top();
          auto& obj_fields = obj.get<value::object>();

          obj_fields->operator[](name) = val;
          push_top() = std::move(val);
          break;
        }
        case op_code::idx_g: {
          value index = pop_top();
          value array = pop_top();

          if (!index.is<value::num>() || !array.is<value::array>()) {
            return std::unexpected{
              "Can't perform " + array.to_string() + "[" + index.to_string() +
              "]"};
          }

          push_top() = value{array.get<value::array>()->at(
            size_t(floor(index.get<value::num>())))};
          break;
        }
        case op_code::szd_arr: {
          value val = pop_top();
          value size = pop_top();

          if (!size.is<value::num>()) {
            return std::unexpected{"Size of szd_arr not a number!"};
          }

          push_top() = value{std::make_shared<value::array::element_type>(
            size_t(size.get<value::num>()), val)};
          break;
        }
        case op_code::key_with: {
          value b = pop_top();
          value a = pop_top();

          if (!a.is<value::object>() || !b.is<value::object>()) {
            return std::unexpected{
              "Can't do " + a.to_string() + " with " + b.to_string()};
          }

          auto a_fields = *a.get<value::object>();
          auto& b_fields = *b.get<value::object>();

          for (auto const& [k, v]: b_fields) {
            a_fields[k] = v;
          }

          push_top() =
            value{std::make_shared<value::object::element_type>(a_fields)};

          break;
        }
        case op_code::idx_s: {
          value val = pop_top();
          value index = pop_top();
          value array = pop_top();

          if (!index.is<value::num>() || !array.is<value::array>()) {
            return std::unexpected{
              "Can't perform " + array.to_string() + "[" + index.to_string() +
              "]"};
          }

          array.get<value::array>()->at(
            size_t(floor(index.get<value::num>()))) = val;
          push_top() = std::move(val);
          break;
        }
        case op_code::array: {
          u16 size = rd_u16();

          auto val = value{
            std::make_shared<value::array::element_type>(stack_top - size + 1,
                                                         stack_top + 1)};

          stack_top -= size;
          push_top() = std::move(val);
          break;
        }
        case op_code::upval_g: {
          u16 slot = rd_u16();
          push_top() = *frame->fn.upvals[slot]->loc;
          break;
        }
        case op_code::upval_s: {
          u16 slot = rd_u16();
          *frame->fn.upvals[slot]->loc = peek_top();
          break;
        }
        case op_code::upval_c: {
          close_upvals(stack_top - 1);
          stack_top--;
          break;
        }
        case op_code::closure: {
          value::function fn = rd_lit_16().get<value::function>(); // must copy
          u16 num_upvals = rd_u16();
          make_closure(fn, num_upvals);
          for (int i = 0; i < num_upvals; i++) {
            u8 is_local = rd_u8();
            u16 index = rd_u16();
            if (is_local) {
              fn.upvals[i] = capture_upval(&stack[frame_offset + index]);
            } else {
              fn.upvals[i] = frame->fn.upvals[index];
            }
          }

          push_top() = value{std::move(fn)};
          break;
        }
      }
    }
  }

  void vm::close_upvals(value* last) {
    while (open_upvals && open_upvals->loc >= last) {
      auto* upval = open_upvals;
      upval->closed = std::move(*upval->loc);
      upval->loc = &upval->closed;
      open_upvals = upval->next;
    }
  }

  void
  vm::def_native(const std::string& name, value::native_fn::second_type arity,
                 value::native_fn::first_type fn) {
    globals[value::str{name}] = value{std::make_pair(fn, arity)};
  }

  void vm::make_closure(value::function& fn, u16 num_upvals) {
    fn.upvals = std::make_shared<upvalue*[]>(num_upvals, nullptr);
  }

  upvalue* vm::capture_upval(value* local) {
    upvalue* prev = nullptr;
    auto* upval = open_upvals;
    while (upval && upval->loc > local) {
      prev = upval;
      upval = upval->next;
    }

    if (upval && upval->loc == local) return upval;

    // TODO: leak!
    // well, technically not, because it's kept in our linked list, but
    // leak nonetheless
    auto* new_upval = new upvalue{local};
    new_upval->next = upval;

    if (!prev) {
      open_upvals = new_upval;
    } else {
      prev->next = new_upval;
    }

    return new_upval;
  }

  call_frame::call_frame(value::function& fn, size_t ip, size_t offset)
    : fn(fn), ip(ip), offset(offset) {}
}
