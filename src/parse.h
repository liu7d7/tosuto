#pragma once

#include <string>
#include "ami.h"
#include "lex.h"

namespace ami
{
  enum class node_type
  {
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
  };

  static std::string to_string(node_type type)
  {
    static const std::string node_type_to_string[]
      {
        "fn_def",
        "block",
        "call",
        "un_op",
        "bin_op",
        "number",
        "string",
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
      };

    return node_type_to_string[std::to_underlying(type)];
  }

  struct node
  {
    pos begin, end;
    node_type type;

    node(node_type type, pos begin, pos end);

    [[nodiscard]] virtual std::string pretty(int indent) const;
  };

  struct fn_def_node : public node
  {
    std::string name;
    std::vector<std::pair<std::string, bool>> args;
    node* body;

    fn_def_node(
      std::string name,
      std::vector<std::pair<std::string, bool>> args,
      node* body, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct anon_fn_def_node : public node
  {
    std::vector<std::pair<std::string, bool>> args;
    node* body;

    anon_fn_def_node(
      std::vector<std::pair<std::string, bool>> args,
      node* body, pos begin, pos end);;

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct block_node : public node
  {
    std::vector<node*> exprs;

    block_node(std::vector<node*> exprs, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct call_node : public node
  {
    node* callee;
    std::vector<node*> args;
    bool is_member;

    call_node(node* callee, std::vector<node*> args, bool is_member, pos begin,
              pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct un_op_node : public node
  {
    node* target;
    tok_type op;

    un_op_node(node* target, tok_type op, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct bin_op_node : public node
  {
    node* lhs;
    node* rhs;
    tok_type op;

    bin_op_node(node* lhs, node* rhs, tok_type op, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct number_node : public node
  {
    double value;

    number_node(double value, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct range_node : public node
  {
    node* start;
    node* finish;

    range_node(node* start, node* finish, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct string_node : public node
  {
    std::string value;

    string_node(std::string value, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct object_node : public node
  {
    std::vector<std::pair<std::string, node*>> fields;

    object_node(decltype(fields) fields, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct field_get_node : public node
  {
    node* target;
    std::string field;

    field_get_node(node* target, std::string field, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct var_def_node : public node
  {
    std::string name;
    node* value;

    var_def_node(std::string name, node* value, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct if_node : public node
  {
    std::vector<std::pair<node*, node*>> cases;
    node* else_case;

    if_node(decltype(cases) cases, node* else_case, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct for_node : public node
  {
    std::string id;
    node* iterable;
    node* body;

    for_node(std::string id, node* iterable, node* body, pos begin, pos end) :
      id(std::move(id)),
      iterable(iterable),
      body(body),
      node(node_type::for_loop, begin, end)
    {}

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct ret_node : public node
  {
    node* ret_val;

    ret_node(node* ret_val, pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct next_node : public node
  {
    next_node(pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct break_node : public node
  {
    break_node(pos begin, pos end);

    [[nodiscard]] std::string pretty(int indent) const override;
  };

  struct parser
  {
    std::vector<token> toks;
    size_t idx;
    token tok, next;

    struct state
    {
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

    std::expected<node*, std::string> block();

    std::expected<node*, std::string> global();

    std::expected<node*, std::string> function();

    std::expected<node*, std::string> statement();

    std::expected<node*, std::string> expr();

    std::expected<node*, std::string> define();

    std::expected<node*, std::string> assign();

    std::expected<node*, std::string> sym_or();

    std::expected<node*, std::string> sym_and();

    std::expected<node*, std::string> comp();

    std::expected<node*, std::string> add();

    std::expected<node*, std::string> mul();

    std::expected<node*, std::string> range();

    std::expected<node*, std::string> pre_unary();

    std::expected<node*, std::string> post_unary();

    std::expected<node*, std::string> call();

    std::expected<node*, std::string> field_get();

    std::expected<node*, std::string> atom();

    std::expected<node*, std::string> for_loop();

    std::expected<node*, std::string> with();
  };
}
