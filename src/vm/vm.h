#pragma once

#include <stack>
#include "../tosuto.h"
#include "value.h"

namespace tosuto::vm {
  enum class op_code : u8 {
    ret,
    lit_16,
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
    key_nil, key_false, key_true,
    get_global, set_global, def_global,
    get_local, set_local,
    jmpf,
    jmp,
    jmpf_pop,
    jmpb_pop,
    call,
    def_prop, get_prop, set_prop,
    get_idx, set_idx,
    array,
    szd_arr,
    key_with,
    new_obj,
    lit_8,
    ld_0, ld_1
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

    inline explicit vm(value::fn& fn) : frames{call_frame{fn, 0, 0}},
                                        stack() {
      stack.reserve(max_of<u16>);
      frames.reserve(max_of<u8>);
      void(push(value{frames.back().fn})); // if this fails we're screwed anyway
    }

#ifdef NDEBUG
    inline dummy_expected<void> push(value val) {
      stack.push_back(std::move(val));
      return {};
    }

    inline dummy_expected<value> pop() {
      value top = stack.back();
      stack.pop_back();
      return dummy_expected{std::move(top)};
    }
#else
    inline std::expected<void, std::string> push(value val) {
      if (stack.size() > max_of<u16>) {
        return std::unexpected{
          "Chance of invalidation of references due to vector resize"};
      }
      stack.push_back(std::move(val));
      return {};
    }

    inline std::expected<value, std::string> pop() {
      value top = stack.back();
      stack.pop_back();
      return top;
    }
#endif

    void
    def_native(std::string const& name, value::native_fn::second_type arity,
               value::native_fn::first_type fn);

    std::expected<void, std::string> run(std::ostream& out);

    std::expected<void, std::string>
    call(value& callee, u8 arity, value*& stack_top, std::function<void(bool)> const& update_stack_frame);
  };
}