#pragma once

#include <stack>
#include "../ami.h"

namespace ami::vm {
  enum class op_code : u8 {
    ret,
    lit,
    neg,
    add,
    mul,
    sub,
    div,
    mod
  };

  using value = double;

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
      data.push_back(u8(dat));
      data.push_back(u8(dat >> 8));
    }

    inline void add(op_code dat) {
      data.push_back(std::to_underlying(dat));
    }

    inline u16 add_lit(value literal) {
      literals.push_back(literal);
      return literals.size() - 1;
    }

    void disasm(std::ostream& out);

    inline u8& operator[](size_t idx) {
      return data[idx];
    }

    inline op_code op(size_t idx) {
      return op_code(data[idx]);
    }

    inline value& literal(size_t idx) {
      return literals[(u16(data[idx + 1]) << 8) + u16(data[idx + 0])];
    }

    size_t disasm_instr(std::ostream& out, size_t idx);
  };

  struct vm {
    size_t ip;
    chunk ch;
    std::stack<value> stack;

    inline explicit vm(chunk ch) : ch(std::move(ch)), ip(0), stack() {}

    enum class result : u8 {
      ok,
      compile_error,
      runtime_error
    };

    inline void push(value const& val) {
      stack.push(val);
    }

    inline value pop() {
      value top = stack.top();
      stack.pop();
      return top;
    }

    inline op_code op() {
      return ch.op(ip++);
    }

    inline value lit() {
      auto l = ch.literal(ip);
      ip += 2;
      return l;
    }

    result run(std::ostream& out);
  };
}