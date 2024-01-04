#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include "../ami.h"

namespace ami::vm {
  using namespace std::string_literals;
  using namespace std::string_view_literals;

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
    eof,
    r_arrow,
    l_arrow,
    walrus,
    exclaim,
    key_for,
    range,
    key_with,
    at
  };

  static std::string to_string(tok_type in) {
    static std::string thing[] {
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
      "neq",
      "eq",
      "sym_or",
      "sym_and",
      "less_than",
      "greater_than",
      "less_than_equal",
      "greater_than_equal",
      "key_if",
      "key_else",
      "key_elif",
      "key_ret",
      "key_next",
      "key_break",
      "eof",
      "r_arrow",
      "l_arrow",
      "walrus",
      "exclaim",
      "key_for",
      "range",
      "key_with",
      "at"
    };

    return thing[std::to_underlying(in)];
  }

  struct token {
    std::string lexeme;
    tok_type type;
    int line;

    std::string to_string() const {
      return
      "token{lexeme="
      + lexeme
      + ", type="
      + ami::vm::to_string(type)
      + ", line="
      + std::to_string(line)
      + "}";
    }
  };

  struct scanner {
    std::u32string str;
    size_t idx;
    size_t start;
    int line;

    explicit scanner(std::string const& path) {
      str = to_utf32((std::stringstream() << std::ifstream(path).rdbuf()).str());
      idx = start = 0;
      line = 1;
    }

    inline bool is_at_end() {
      return str[idx] == U'\0';
    }

    inline token make_token(tok_type type) {
      return token{to_utf8(str.substr(start, idx - start)), type, line};
    }

    char32_t advance() {
      return str[idx++];
    }

    char32_t peek() {
      return str[idx];
    }

    char32_t peek_next() {
      if (is_at_end()) return '\0';
      return str[idx + 1];
    }

    bool match(char32_t expected) {
      if (is_at_end()) return false;
      if (str[idx] != expected) return false;
      idx++;
      return true;
    }

    void skip_whitespace() {
      for (;;) {
        switch (peek()) {
          case U' ':
          case '\r':
          case '\t':
            advance();
            break;
          case '\n':
            line++;
            advance();
            break;
          case '/':
            if (peek_next() == U'/') {
              while (peek() != '\n' && !is_at_end()) advance();
            } else {
              return;
            }
            break;
          default:
            return;
        }
      }
    }

    std::expected<token, std::string> string() {
      while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') line++;
        advance();
      }

      if (is_at_end()) return std::unexpected{"Unterminated string."};

      advance();
      return make_token(tok_type::string);
    }

    static bool is_digit(char32_t ch) {
      return ch >= U'0' && ch <= U'9';
    }

    token number() {
      while (is_digit(peek())) advance();

      if (peek() == U'.' && is_digit(peek_next())) {
        advance();
        while (is_digit(peek())) advance();
      }

      return make_token(tok_type::number);
    }

    static bool is_id_start(char16_t ch) {
      return isalpha(ch)
             || ch == '_'
             || ch == '$'
             || (ch >= 0xff61 && ch <= 0xff9F) // half-width katakana
             || (ch >= 0x0300 && ch <= 0x036f) // diacritics (combining characters)
             || (ch >= 128 && ch <= 255); // diacritics (extended ascii)
    }

    static bool is_id_continue(char16_t ch) {
      return is_id_start(ch) || isdigit(ch);
    }

    std::unordered_map<std::u32string_view , tok_type> keywords =
      {
        {U"if",    tok_type::key_if},
        {U"elif",  tok_type::key_elif},
        {U"else",  tok_type::key_else},
        {U"ret",   tok_type::key_ret},
        {U"next",  tok_type::key_next},
        {U"break", tok_type::key_break},
        {U"for",   tok_type::key_for},
        {U"with",  tok_type::key_with},
      };

    tok_type id_type() {
      std::u32string_view id = std::u32string_view(str).substr(start, idx);
      auto it = keywords.find(id);
      if (it != keywords.end()) return it->second;

      return tok_type::id;
    }

    token id() {
      while (is_id_continue(peek())) advance();
      return make_token(id_type());
    }

    std::expected<token, std::string> next() {
      skip_whitespace();
      start = idx;

      if (is_at_end()) make_token(tok_type::eof);

      auto c = advance();

      if (is_digit(c)) return number();
      if (is_id_start(c)) return id();

      switch (c) {
        case U'(': return make_token(tok_type::l_paren);
        case U')': return make_token(tok_type::r_paren);
        case U'{': return make_token(tok_type::l_curly);
        case U'}': return make_token(tok_type::r_curly);
        case U']': return make_token(tok_type::r_square);
        case U'&': return make_token(tok_type::sym_and);
        case U',': return make_token(tok_type::comma);
        case U'!': return make_token(tok_type::exclaim);
        case U'@': return make_token(tok_type::at);
        case U'|':
          return make_token(
            match(U']') ?
              tok_type::r_object
            : tok_type::sym_or);
        case U'[':
          return make_token(
            match(U'|') ?
              tok_type::l_object
            : tok_type::l_square);
        case U'<':
          return make_token(
            match(U'=') ?
              tok_type::less_than_equal
            : match(U'>') ?
              tok_type::neq
            : match(U'-') ?
              tok_type::l_arrow
            : tok_type::less_than);
        case U'>':
          return make_token(
            match(U'=') ?
              tok_type::greater_than_equal
            : tok_type::greater_than);
        case U'+':
          return make_token(
            match(U'=') ?
              tok_type::add_assign
            : match(U'+') ?
              tok_type::inc
            : tok_type::add);
        case U'-':
          return make_token(
            match(U'=') ?
              tok_type::sub_assign
            : match(U'-') ?
              tok_type::dec
            : match(U'>') ?
              tok_type::r_arrow
            : tok_type::sub);
        case U'*':
          return make_token(
            match(U'=') ?
              tok_type::mul_assign
            : tok_type::mul);
        case U'/':
          return make_token(
            match(U'=') ?
              tok_type::div_assign
            : tok_type::div);
        case U'%':
          return make_token(
            match(U'=') ?
              tok_type::mod_assign
            : tok_type::mod);
        case U'=':
          return make_token(
            match(U'=') ?
              tok_type::eq
            : tok_type::assign);
        case U':':
          return make_token(
            match(U'=') ?
              tok_type::walrus
            : tok_type::colon);
        case U'"': return string();
        case U'.':
          return make_token(
            match(U'.') ?
              tok_type::range
            : tok_type::dot);
      }

      return std::unexpected{
        "Unknown character "
        + std::to_string(peek())
        + " (" + to_utf8(std::u32string(1, peek()))
        + ")"};
    }
  };
}