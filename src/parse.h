#pragma once

#include <string>
#include <sstream>
#include <memory>
#include "tosuto.h"
#include "lex.h"

namespace tosuto {
  enum class node_type : u8 {
    fn_def,
    block,
    call,
    un_op,
    bin_op,
    number,
    string,
    object,
    field_get,
    if_stmt,
    ret,
    next,
    brk,
    var_def,
    range,
    for_loop,
    anon_fn_def,
    deco,
    decorated,
    kw_literal,
    member_call,
    array,
    sized_array
  };

  static std::string to_string(node_type type) {
    static const std::string node_type_to_string[]{
      "fn_def",
      "block",
      "call",
      "un_op",
      "bin_op",
      "number",
      "str",
      "object",
      "field_get",
      "if_stmt",
      "ret",
      "next",
      "break",
      "var_def",
      "range",
      "for_loop",
      "anon_fn_def",
      "deco",
      "decorated",
      "kw_literal",
      "member_call",
      "array",
      "sized_array",
    };

    return node_type_to_string[std::to_underlying(type)];
  }

  struct node {
    pos begin, end;
    node_type type;

    node(node_type type, pos begin, pos end);

    virtual ~node() = default;

    [[nodiscard]] virtual std::string pretty(int indent) const = 0;
  };

  struct fn_def_node : public node {
    std::string name;
    std::vector<std::pair<std::string, bool>> args;
    std::shared_ptr<node> body;
    bool is_variadic;

    fn_def_node(
      std::string name,
      std::vector<std::pair<std::string, bool>> args,
      std::shared_ptr<node> body, bool is_variadic, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct block_node : public node {
    std::vector<std::shared_ptr<node>> exprs;

    block_node(std::vector<std::shared_ptr<node>> exprs, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct array_node : public node {
    std::vector<std::shared_ptr<node>> exprs;

    array_node(std::vector<std::shared_ptr<node>> exprs, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct sized_array_node : public node {
    std::shared_ptr<node> size;
    std::shared_ptr<node> val;

    sized_array_node(std::shared_ptr<node> size, std::shared_ptr<node> val,
                     pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct call_node : public node {
    std::shared_ptr<node> callee;
    std::vector<std::shared_ptr<node>> args;

    call_node(std::shared_ptr<node> callee,
              std::vector<std::shared_ptr<node>> args,
              pos begin,
              pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct member_call_node : public node {
    std::shared_ptr<node> callee;
    std::string field;
    std::vector<std::shared_ptr<node>> args;

    member_call_node(std::shared_ptr<node> callee,
                     std::string field,
                     std::vector<std::shared_ptr<node>> args,
                     pos begin,
                     pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct un_op_node : public node {
    std::shared_ptr<node> target;
    tok_type op;

    un_op_node(std::shared_ptr<node> target, tok_type op, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct bin_op_node : public node {
    std::shared_ptr<node> lhs;
    std::shared_ptr<node> rhs;
    tok_type op;

    bin_op_node(std::shared_ptr<node> lhs, std::shared_ptr<node> rhs,
                tok_type op, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct number_node : public node {
    double value;

    number_node(double value, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct range_node : public node {
    std::shared_ptr<node> start;
    std::shared_ptr<node> finish;

    range_node(std::shared_ptr<node> start, std::shared_ptr<node> finish,
               pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct string_node : public node {
    std::string value;

    string_node(std::string value, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct object_node : public node {
    std::vector<std::pair<std::string, std::shared_ptr<node>>> fields;

    object_node(decltype(fields) fields, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct kw_literal_node : public node {
    tok_type lit;

    kw_literal_node(tok_type lit, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct field_get_node : public node {
    std::shared_ptr<node> target;
    std::string field;

    field_get_node(std::shared_ptr<node> target, std::string field, pos begin,
                   pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct var_def_node : public node {
    std::string name;
    std::shared_ptr<node> value;

    var_def_node(std::string name, std::shared_ptr<node> value, pos begin,
                 pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct if_node : public node {
    std::vector<std::pair<std::shared_ptr<node>, std::shared_ptr<node>>> cases;
    std::shared_ptr<node> else_case;

    if_node(decltype(cases) cases, std::shared_ptr<node> else_case, pos begin,
            pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct for_node : public node {
    std::string id;
    std::shared_ptr<node> iterable;
    std::shared_ptr<node> body;

    for_node(std::string id, std::shared_ptr<node> iterable,
             std::shared_ptr<node> body, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct ret_node : public node {
    std::shared_ptr<node> ret_val;

    ret_node(std::shared_ptr<node> ret_val, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct next_node : public node {
    next_node(pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct break_node : public node {
    break_node(pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct deco_node : public node {
    std::shared_ptr<node> deco;
    std::vector<std::shared_ptr<node>> fields;

    deco_node(std::shared_ptr<node> deco,
              std::vector<std::shared_ptr<node>> nodes,
              pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct decorated_node : public node {
    std::vector<std::shared_ptr<node>> decos;
    std::shared_ptr<node> target;

    decorated_node(std::vector<std::shared_ptr<node>> decos,
                   std::shared_ptr<node> target,
                   pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct parser {
    std::vector<token> toks;
    size_t idx;
    token tok, next;

    struct state {
      size_t idx;
      token tok, next;
    };

    state save_state();

    void set_state(state const& s);

    explicit parser(std::vector<token> toks);

    void advance();

    std::expected<token, std::string> expect(tok_type type, bool advance = true,
                                             std::source_location loc = std::source_location::current());

    void consume(tok_type type);

    std::expected<std::shared_ptr<node>, std::string> block();

    std::expected<std::shared_ptr<node>, std::string> global();

    std::expected<std::shared_ptr<node>, std::string> function();

    std::expected<std::shared_ptr<node>, std::string> statement();

    std::expected<std::shared_ptr<node>, std::string> expr();

    std::expected<std::shared_ptr<node>, std::string> define();

    std::expected<std::shared_ptr<node>, std::string> assign();

    std::expected<std::shared_ptr<node>, std::string> sym_or();

    std::expected<std::shared_ptr<node>, std::string> sym_and();

    std::expected<std::shared_ptr<node>, std::string> comp();

    std::expected<std::shared_ptr<node>, std::string> add();

    std::expected<std::shared_ptr<node>, std::string> mul();

    std::expected<std::shared_ptr<node>, std::string> range();

    std::expected<std::shared_ptr<node>, std::string> pre_unary();

    std::expected<std::shared_ptr<node>, std::string> post_unary();

    std::expected<std::shared_ptr<node>, std::string> call(bool allow_parens = true);

    std::expected<std::shared_ptr<node>, std::string> atom();

    std::expected<std::shared_ptr<node>, std::string> for_loop();

    std::expected<std::shared_ptr<node>, std::string> with();

    std::expected<std::vector<std::shared_ptr<node>>, std::string> decos();
  };
}
