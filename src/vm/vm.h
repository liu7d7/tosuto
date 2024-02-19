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

    inline explicit chunk(value::str&& name) : name(name) {}

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
      return *(u16*)(&data[idx]);
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

  struct fn_desc {
    chunk chunk;
    u8 arity = 0xff;
    std::optional<u8> varargs_start;
  };

  struct call_frame {
    value::function& fn;
    size_t ip;
    size_t offset;

    call_frame(value::function& fn, size_t ip, size_t offset);
  };

  struct vm {
    std::vector<call_frame> frames;
    std::vector<value> stack;
    std::unordered_map<value::str, value> globals;
    upvalue* open_upvals = nullptr;

    inline explicit vm(value::function& fn) : frames{call_frame{fn, 0, 0}},
                                              stack() {
      stack.reserve(max_of<u16>);
      frames.reserve(max_of<u8>);
      stack[0] = value{frames.back().fn};
    }

    void
    def_native(std::string const& name, value::native_fn::second_type arity,
               value::native_fn::first_type fn);

    std::expected<void, std::string> run(std::ostream& out);

    template<typename Fn>
    std::expected<void, std::string>
    call(value& callee, u8 arity, value*& stack_top,
         Fn const& update_stack_frame) {
      switch (callee.val.index()) {
        case 7: {
          auto fn = callee.get<value::native_fn>();
          if (arity != fn.second) {
            return std::unexpected{
              "Non-matching # of arguments! (exp: "
              + std::to_string(fn.second)
              + ", got: " + std::to_string(arity) + ")"};
          }

          auto res = ami_unwrap_move_fast(
            fn.first(std::span{stack_top + 1 - arity, stack_top + 1}));
          stack_top -= arity;
          stack_top--;
          *(++stack_top) = res;
          return {};
        }
        case 6: {
          auto& fn = callee.get<value::function>();
          // TODO: implement varargs
          if (fn.desc->arity != arity) {
            return std::unexpected{
              "Non-matching # of arguments! (exp: "
              + std::to_string(fn.desc->arity)
              + ", got: " + std::to_string(arity) + ")"};
          }

          frames.emplace_back(fn, 0, stack_top - stack.data() - arity);
          update_stack_frame(false);

          return {};
        }
        default:;
      }

      return std::unexpected{"Can't call " + callee.to_string()};
    }

    void make_closure(value::function& fn, u16 num_upvals);

    upvalue* capture_upval(value* local);

    void close_upvals(value* last);
  };
}