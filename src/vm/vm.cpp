#include "vm.h"
#include <iomanip>

namespace tosuto::vm {
  void chunk::disasm(std::ostream& out) {
    out << name << ":\n";

    for (size_t offset = 0; offset < data.size();) {
      offset = disasm_instr(out, offset);
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

    switch (op(idx)) {
      TOSUTO_DISASM_SIMPLE_INSTR(ret);
      TOSUTO_DISASM_SIMPLE_INSTR(neg);
      TOSUTO_DISASM_SIMPLE_INSTR(add);
      TOSUTO_DISASM_SIMPLE_INSTR(sub);
      TOSUTO_DISASM_SIMPLE_INSTR(mul);
      TOSUTO_DISASM_SIMPLE_INSTR(div);
      TOSUTO_DISASM_SIMPLE_INSTR(pop);
      TOSUTO_DISASM_SIMPLE_INSTR(eq);
      TOSUTO_DISASM_SIMPLE_INSTR(lt);
      TOSUTO_DISASM_SIMPLE_INSTR(gt);
      TOSUTO_DISASM_SIMPLE_INSTR_2(sym_and, and);
      TOSUTO_DISASM_SIMPLE_INSTR_2(sym_or, or);
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
        out << std::left << std::setw(9) << "get_loc" << u16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::set_local: {
        out << std::left << std::setw(9) << "set_loc" << u16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::jmpf: {
        out << std::left << std::setw(9) << "jmpf" << u16(idx + 1) << '\n';
        return idx + 3;
      }
      case op_code::jmp: {
        out << std::left << std::setw(9) << "jmp" << u16(idx + 1) << '\n';
        return idx + 3;
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
    auto b = pop(); \
    auto a = pop(); \
    if (a.is<value::num>() && b.is<value::num>()) \
      push(value{a.get<value::num>() op b.get<value::num>()}); \
    else               \
      return std::unexpected{"Couldn't do " + a.to_string() + #op + b.to_string()}; \
  } while(false)

    for (;;) {
      if (!stack.empty())
        out << stack << '\n';
      else out << "[ ]\n";
      ch.disasm_instr(out, ip);
      out << '\n';
      switch (op()) {
        case op_code::ret: {
          out << pop() << '\n';
          return {};
        }
        case op_code::lit: {
          push(lit());
          break;
        }
        case op_code::pop: {
          pop();
          break;
        }
        case op_code::neg: {
          value a = pop();
          if (a.is<value::num>()) {
            push(value{-a.get<value::num>()});
          } else {
            return std::unexpected{
              "Tried to negate smth that was not a number!"};
          }
          break;
        }
        case op_code::add: {
          value b = pop();
          value a = pop();
          if (a.is<value::num>() && b.is<value::num>()) {
            push(value{a.get<value::num>() + b.get<value::num>()});
          } else if (a.is<value::str>() && b.is<value::str>()) {
            push(value{a.get<value::str>() + b.get<value::str>()});
          } else {
            return std::unexpected{
              "Couldn't do " + a.to_string() + " + " + b.to_string()};
          }
          break;
        }
        case op_code::sub: TOSUTO_BIN_OP(-);
          break;
        case op_code::mul: TOSUTO_BIN_OP(*);
          break;
        case op_code::div: TOSUTO_BIN_OP(/);
          break;
        case op_code::lt: TOSUTO_BIN_OP(<);
          break;
        case op_code::gt: TOSUTO_BIN_OP(>);
          break;
        case op_code::mod: {
          value b = pop();
          value a = pop();
          if (a.is<value::num>() && b.is<value::num>()) {
            push(value{fmod(a.get<value::num>(), b.get<value::num>())});
          } else {
            return std::unexpected{
              "Couldn't do " + a.to_string() + " % " + b.to_string()};
          }
          break;
        }
        case op_code::eq: {
          value b = pop();
          value a = pop();
          push(value{a.eq(b)});
          break;
        }
        case op_code::sym_or: {
          throw std::runtime_error("TODO: not implemented yet");
        }
        case op_code::sym_and: {
          throw std::runtime_error("TODO: not implemented yet");
        }
        case op_code::inv: {
          value a = pop();
          push(value{!a.is_truthy()});
          break;
        }
        case op_code::key_false: push(value{false});
          break;
        case op_code::key_true: push(value{true});
          break;
        case op_code::key_nil: push(value{value::nil{}});
          break;
        case op_code::set_global: {
          auto name = lit().get<value::str>();
          auto it = globals.find(name);
          if (it != globals.end()) {
            it->second = peek();
          } else {
            return std::unexpected{"Could not find " + std::string(name) + " in globals!"};
          }

          break;
        }
        case op_code::get_global: {
          auto name = lit().get<value::str>();
          auto it = globals.find(name);
          if (it != globals.end()) {
            push(value{it->second});
          } else {
            return std::unexpected{"Could not find " + std::string(name) + " in globals!"};
          }

          break;
        }
        case op_code::def_global: {
          auto name = lit().get<value::str>();
          globals[name] = peek();
          break;
        }
        case op_code::get_local: {
          auto slot = ch.u16(ip);
          ip += 2;
          push(value{stack[slot]});
          break;
        }
        case op_code::set_local: {
          auto slot = ch.u16(ip);
          ip += 2;
          stack[slot] = peek();
          break;
        }
        case op_code::jmpf: {
          u16 off = ch.u16(ip);
          ip += 2;
          if (!peek().is_truthy()) ip += off;
          break;
        }
        case op_code::jmp: {
          u16 off = ch.u16(ip);
          ip += 2;
          ip += off;
          break;
        }
      }
    }
  }
}