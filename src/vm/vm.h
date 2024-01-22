#pragma once

#include <stack>
#include "../ami.h"
#include "value.h"
#include <utility>
#include <variant>

namespace ami::vm {
  enum class op_code : u8 {
    ret,
    neg,
    add,
    mul,
    sub,
    div,
    mod,
    pop,
    pop_loc,
    eq,
    gt,
    lt,
    inv,
    key_nil,
    key_false,
    key_true,
    glob_g,
    glob_s,
    glob_d,
    loc_g,
    loc_s,
    jmpf,
    jmp,
    jmpf_pop,
    jmpb_pop,
    call,
    prop_d,
    prop_g,
    prop_s,
    idx_g,
    idx_s,
    array,
    szd_arr,
    key_with,
    new_obj,
    lit_8,
    lit_16,
    ld_0,
    ld_1,
    closure,
    upval_g,
    upval_s,
    upval_c,
  };

  struct chunk {
    std::vector<u8> data;
    std::vector<value> literals;
    value::str name;

    inline explicit chunk(value::str&& name) : name(std::move(name)) {}

    inline chunk() : name("anonymous") {}

    inline void add(u8 dat) {
      data.push_back(dat);
    }

    inline void add(u16 dat) {
      data.push_back(uint8_t(dat));
      data.push_back(uint8_t(dat >> 8));
    }

    inline void add(u32 dat) {
      data.push_back(uint8_t(dat));
      data.push_back(uint8_t(dat >> 8));
      data.push_back(uint8_t(dat >> 16));
      data.push_back(uint8_t(dat >> 24));
    }

    inline void add(op_code dat) {
      data.push_back(std::to_underlying(dat));
    }

    inline void add_lit(value&& literal) {
      literals.push_back(std::move(literal));
      if (literals.size() - 1 <= max_of<u8>) {
        add(op_code::lit_8);
        add(u8(literals.size() - 1));
      } else {
        add(op_code::lit_16);
        add(u16(literals.size() - 1));
      }
    }

    inline u16 add_lit_get(value&& literal) {
      u16 ret = literals.size();
      literals.push_back(std::move(literal));
      return ret;
    }

    void disasm(std::ostream& out, bool add_name = true);

    inline u8& rd_u8(size_t idx) {
      return data[idx];
    }

    inline u16 rd_u16(size_t idx) {
      return (uint16_t(data[idx + 1]) << 8) + uint16_t(data[idx + 0]);
    }

    inline op_code rd_op(size_t idx) {
      return op_code(data[idx]);
    }

    inline value& lit_16(size_t idx) {
      return literals[rd_u16(idx)];
    }

    inline value& lit_8(size_t idx) {
      return literals[rd_u8(idx)];
    }

    size_t disasm_instr(std::ostream& out, size_t idx);
  };

  struct call_frame {
    value::fn& fn;
    size_t ip;
    size_t offset;

    call_frame(value::fn& fn, size_t ip, size_t offset);
  };

  struct vm {
    std::vector<call_frame> frames;
    std::vector<value> stack;
    std::unordered_map<value::str, value> globals;
    upvalue* open_upvals = nullptr;

    inline explicit vm(value::fn& fn) : frames{call_frame{fn, 0, 0}},
                                        stack() {
      stack.reserve(max_of<u16>);
      frames.reserve(max_of<u8>);
      stack[0] = value{frames.back().fn};
    }

    void
    def_native(std::string const& name, value::native_fn::second_type arity,
               value::native_fn::first_type fn);

    std::expected<void, std::string> run(std::ostream& out);

    std::expected<void, std::string>
    call(value& callee, u8 arity, value*& stack_top,
         std::function<void(bool)> const& update_stack_frame);

    void make_closure(value::fn& fn, u16 num_upvals);

    upvalue* capture_upval(value* local);

    void close_upvals(value* last);
  };
}