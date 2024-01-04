#include "vm.h"
#include "scan.h"

namespace ami::vm {

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

#define AMI_DISASM_SIMPLE_INSTR(op) \
  do { \
    out << #op << '\n'; \
    return idx + 1; \
  } while(false) \

    switch (op(idx)) {
      case op_code::ret: AMI_DISASM_SIMPLE_INSTR(ret);
      case op_code::neg: AMI_DISASM_SIMPLE_INSTR(neg);
      case op_code::add: AMI_DISASM_SIMPLE_INSTR(add);
      case op_code::sub: AMI_DISASM_SIMPLE_INSTR(sub);
      case op_code::mul: AMI_DISASM_SIMPLE_INSTR(mul);
      case op_code::div: AMI_DISASM_SIMPLE_INSTR(div);
      case op_code::lit: {
        out << "value ";
        out << literal(idx + 1);
        out << '\n';
        return idx + 3;
      }
      default: {
        out << "uh oh!\n";
        return std::numeric_limits<size_t>::max();
      }
    }
  }

  vm::result vm::run(std::ostream& out) {
#define AMI_BIN_OP(op) \
  do { \
    double a = pop(); \
    double b = pop(); \
    push(a op b); \
  } while(false)

    for (;;) {
      ch.disasm_instr(out, ip);
      if (!stack.empty())
        out << stack << '\n';
      switch (op()) {
        case op_code::ret: {
          out << pop() << '\n';
          return result::ok;
        }
        case op_code::lit: {
          push(lit());
          break;
        }
        case op_code::neg: {
          push(-pop());
          break;
        }
        case op_code::add: AMI_BIN_OP(+); break;
        case op_code::sub: AMI_BIN_OP(-); break;
        case op_code::mul: AMI_BIN_OP(*); break;
        case op_code::div: AMI_BIN_OP(/); break;
        case op_code::mod: {
          double a = pop();
          double b = pop();
          push(fmod(a, b));
          break;
        }
      }
    }
  }
}