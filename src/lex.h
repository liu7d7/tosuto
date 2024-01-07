#pragma once

#include <string>
#include <utility>
#include "ami.h"

namespace ami {
  enum class tok_type {
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
    neq,
    eq,
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
    range,
    backslash,
    key_with,
    at,
    key_false,
    key_true,
    key_nil,
  };

  static std::string to_string(tok_type type) {
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
        "str",
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
        "neq",
        "eq",
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
        "range",
        "backslash",
        "with",
        "at",
        "false",
        "true",
        "nil",
      };

    return tok_type_to_string[std::to_underlying(type)];
  }

  struct token {
    tok_type type;
    std::string lexeme;
    pos begin, end;

    [[nodiscard]] std::string to_string() const;
  };

  struct lexer {
    std::u16string text;
    pos cpos = {.idx = 0, .col = 1, .row = 1};
    char16_t ch;

    explicit lexer(std::string const& path);

    void advance();

    std::vector<token> lex();

    static bool is_id_start(char16_t ch);

    static bool is_id_continue(char16_t ch);
  };
}
