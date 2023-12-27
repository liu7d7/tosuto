#pragma once

#include <cstddef>
#include <string>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <utility>
#include <expected>
#include <source_location>
#include <variant>
#include <functional>
#include <optional>
#include <cmath>

namespace ami
{
  struct pos
  {
    size_t idx, col, row;

    [[nodiscard]] std::string to_string() const;
  };

  enum class tok_type
  {
    id,
    colon,
    l_curly,
    r_curly,
    assign,
    number,
    l_object,
    r_object,
    l_square,
    r_square,
    comma,
    l_paren,
    r_paren,
    dot,
    string,
    inc,
    dec,
    add,
    sub,
    mul,
    div,
    mod,
    add_assign,
    sub_assign,
    mul_assign,
    div_assign,
    mod_assign,
    not_equal,
    equal,
    sym_or,
    sym_and,
    less_than,
    greater_than,
    less_than_equal,
    greater_than_equal,
    key_if,
    key_else,
    key_elif,
    key_ret,
    key_next,
    key_break,
    key_fun,
    eof,
    r_arrow,
    l_arrow,
    walrus,
    exclaim,
    semicolon,
    key_for,
    range
  };

  static std::string to_string(tok_type type)
  {
    static const std::string tok_type_to_string[]
      {
        "id",
        "colon",
        "l_curly",
        "r_curly",
        "assign",
        "number",
        "l_object",
        "r_object",
        "l_square",
        "r_square",
        "comma",
        "l_paren",
        "r_paren",
        "dot",
        "string",
        "inc",
        "dec",
        "add",
        "sub",
        "mul",
        "div",
        "mod",
        "add_assign",
        "sub_assign",
        "mul_assign",
        "div_assign",
        "mod_assign",
        "not_equal",
        "equal",
        "or",
        "and",
        "less_than",
        "greater_than",
        "less_than_equal",
        "greater_than_equal",
        "if",
        "else",
        "elif",
        "ret",
        "next",
        "break",
        "fun",
        "eof",
        "r_arrow",
        "l_arrow",
        "walrus",
        "exclaim",
        "semicolon",
        "for",
        "range"
      };

    return tok_type_to_string[std::to_underlying(type)];
  }

  struct token
  {
    tok_type type;
    std::string lexeme;
    pos begin, end;

    [[nodiscard]] std::string to_string() const;
  };

  struct lexer
  {
    std::string text;
    pos cpos = {.idx = 0, .col = 1, .row = 1};
    char ch;

    explicit lexer(std::string const& path);

    void advance();

    std::vector<token> lex();

    static bool is_id_start(char ch);

    static bool is_id_continue(char ch);

    static std::unordered_map<std::string, tok_type> keywords;
    static std::unordered_map<char, tok_type> symbols;
  };

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
    std::vector<std::string> args;
    node* body;

    fn_def_node(
      std::string name,
      std::vector<std::string> args,
      node* body, pos begin, pos end);

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

    call_node(node* callee, std::vector<node*> args, pos begin, pos end);

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
    double start;
    double finish;

    range_node(double start, double finish, pos begin, pos end);

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
      node(node_type::for_loop, begin, end) {}

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

  template<typename V>
  struct yeah
  {
    static V held;
  };

  struct parser
  {
    std::vector<token> toks;
    size_t idx;
    token tok, next;

    explicit parser(std::vector<token> toks);

    void advance();
    std::expected<token, std::string> expect(tok_type type, bool advance = true, std::source_location loc = std::source_location::current());
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
    std::expected<node*, std::string> pre_unary();
    std::expected<node*, std::string> post_unary();
    std::expected<node*, std::string> call();
    std::expected<node*, std::string> field_get();
    std::expected<node*, std::string> atom();

    std::expected<node*, std::string> for_loop();
  };

  std::string repeat(const std::string& input, size_t num);

  struct symbol_table;

  struct nil_type {};

  struct value
  {
    using interpret_result = std::expected<value, std::string>;
    using builtin_fn = std::function<std::expected<value, std::string>(std::vector<value> const&, symbol_table const&)>;

    std::variant<
      double,
      std::string,
      bool,
      std::unordered_map<std::string, value>,
      fn_def_node*,
      builtin_fn,
      nil_type> val;

    explicit value(decltype(val) val);

    value();

    static std::unexpected<std::string>
    fail(std::string const& message, std::source_location loc = std::source_location::current());

    std::string type_name() const;

    interpret_result add(value const& other);

    interpret_result sub(value const& other);

    interpret_result mul(value const& other);

    interpret_result call(std::vector<value> const& args, symbol_table& sym);

    interpret_result div(value const& other);

    interpret_result mod(value const& other);

    interpret_result get(std::string const& field);

    interpret_result set(std::string const& field);

    bool truthy();

    interpret_result invert();

    interpret_result eq(value const& other);

    interpret_result neq(value const& other);
  };

  using interpret_result = value::interpret_result;

  struct symbol_table
  {
    symbol_table* parent = nullptr;
    std::unordered_map<std::string, value> vals;

    explicit symbol_table(symbol_table* parent,
                          std::unordered_map<std::string, value> vals);

    interpret_result const& get(std::string const& name) const;

    interpret_result const& set(std::string const& name, value val);
  };

  struct interpreter
  {
    bool has_next = false, has_break = false, has_return = false;

    void reset_state()
    {
      has_next = false, has_break = false, has_return = false;
    }

    static std::unexpected<std::string>
    fail(node* nod, std::source_location loc = std::source_location::current());

    interpret_result block(node* nod, symbol_table& sym);
    interpret_result fn_def(node* nod, symbol_table& sym);
    interpret_result interpret(node* nod, symbol_table& sym);

    static std::unordered_map<
      node_type,
      interpret_result(interpreter::*)(node* nod, symbol_table& sym)> interps;

    interpret_result call(node* nod, symbol_table& sym);
  };
}