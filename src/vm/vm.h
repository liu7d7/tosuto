#pragma once

#include <stack>
#include "../tosuto.h"
#include "value.h"

namespace tosuto::vm {
  enum class op_code : u8 {
    ret,
    lit,
    neg,
    add,
    mul,
    sub,
    div,
    mod,
    pop,
    eq,
    gt,
    lt,
    sym_or,
    sym_and,
    inv,
    key_nil, key_false, key_true,
    get_global, set_global, def_global,
    get_local, set_local,
    jmpf,
    jmp
  };

  struct chunk {
    std::vector<u8> data;
    std::vector<value> literals;
    std::string name;

    inline explicit chunk(std::string&& name) : name(std::move(name)) {}

    inline chunk() : name("anonymous") {}

    inline void add(u8 dat) {
      data.push_back(dat);
    }

    inline void add(u16 dat) {
      data.push_back(uint8_t(dat));
      data.push_back(uint8_t(dat >> 8));
    }

    inline void add(op_code dat) {
      data.push_back(std::to_underlying(dat));
    }

    inline u16 add_lit(value&& literal) {
      literals.push_back(std::move(literal));
      return literals.size() - 1;
    }

    void disasm(std::ostream& out);

    inline u8& u8(size_t idx) {
      return data[idx];
    }

    inline u16 u16(size_t idx) {
      return (uint16_t(data[idx + 1]) << 8) + uint16_t(data[idx + 0]);
    }

    inline op_code op(size_t idx) {
      return op_code(data[idx]);
    }

    inline value& literal(size_t idx) {
      return literals[u16(idx)];
    }

    size_t disasm_instr(std::ostream& out, size_t idx);
  };

  struct vm {
    size_t ip;
    chunk ch;
    std::vector<value> stack;
    std::unordered_map<value::str, value> globals;

    inline explicit vm(chunk ch) : ch(std::move(ch)), ip(0), stack() {}

    inline void push(value&& val) {
      stack.push_back(std::move(val));
    }

    inline value pop() {
      value top = stack.back();
      stack.pop_back();
      return top;
    }

    inline value peek(int off = 0) {
      return stack[stack.size() - 1 - off];
    }

    inline op_code op() {
      return ch.op(ip++);
    }

    inline value lit() {
      auto l = ch.literal(ip);
      ip += 2;
      return l;
    }

    std::expected<void, std::string> run(std::ostream& out);
  };
}