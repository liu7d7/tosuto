#pragma once

#include "vm.h"
#include "../parse.h"

namespace ami::vm {
  struct compiler {
    struct upvalue {
      u16 index;
      bool is_local;

      inline upvalue(u16 index, bool is_local) : index(index), is_local(is_local) {}
    };

    struct local {
      value::str name{""};
      u8 depth{};
      bool is_captured;

      inline local(value::str&& name, u8 depth) :
        name(name), depth(depth), is_captured(false) {}

      constexpr static u8 invalid_depth = std::numeric_limits<u8>::max();
    };

    compiler* enclosing;
    value::function fun;
    value::function::type fn_type;
    constexpr static u16 max_locals = std::numeric_limits<u16>::max();
    std::vector<local> locals;
    std::vector<upvalue> upvals;
    u8 depth{};

    explicit compiler(value::function::type type);

    [[nodiscard]] inline chunk& cur_ch() /* mutating */ {
      return fun.desc->chunk;
    }

    std::expected<void, std::string> number(node* n);

    std::expected<void, std::string> bin_op(node* n);

    std::expected<void, std::string> un_op(node* n);

    std::expected<void, std::string> block(node* n);

    std::expected<void, std::string> compile(node* n);

    std::expected<void, std::string> kw_literal(node* n);

    std::expected<void, std::string> string(node* n);

    std::expected<void, std::string> var_def(node* n);

    std::expected<void, std::string> field_get(node* n);

    std::expected<void, std::string> add_local(const std::string& name);

    void begin_block();

    void end_block();

    std::optional<u16> resolve_local(std::string const& name);
    std::optional<u16> resolve_upval(std::string const& name);

    std::expected<void, std::string> if_stmt(node* n);

    size_t emit_jump(op_code type);

    std::expected<void, std::string> patch_jump(size_t offset);

    std::expected<value::function, std::string> global(node* n);

    std::expected<void, std::string> fn_def(node* n);

    std::expected<void, std::string> function(value::function::type type, node* n);

    std::expected<void, std::string> pop_for_exp_stmt(node* exp);

    std::expected<void, std::string> call(node* n);

    std::expected<void, std::string> basic_block(node* n, bool pop_last);

    std::expected<void, std::string> exp_or_block_no_pop(node* n);

    std::expected<void, std::string> ret(node* n);

    std::expected<void, std::string> object(node* n);

    std::expected<void, std::string> anon_fn_def(node* n);

    std::expected<void, std::string> member_call(node* n);

    std::expected<void, std::string> array(node* n);

    std::expected<void, std::string> for_loop(node* n);

    std::expected<void, std::string> sized_array(node* n);

    std::pair<op_code, u16> get_instr(std::string const& name);
    std::pair<op_code, u16> set_instr(std::string const& name);

    u16 add_upval(u16 idx, bool is_local);

    std::expected<void, std::string> decorated(node* n);

    std::expected<void, std::string>
    decorate_fn(decorated_node* decor, std::shared_ptr<node> const& fn);
  };
}