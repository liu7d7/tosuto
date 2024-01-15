#include <sstream>
#include <iostream>
#include "lex.h"

namespace tosuto {
  static std::unordered_map<std::u32string, tok_type> keywords =
    {
      {U"if",    tok_type::key_if},
      {U"elif",  tok_type::key_elif},
      {U"else",  tok_type::key_else},
      {U"fun",   tok_type::key_fun},
      {U"ret",   tok_type::key_ret},
      {U"next",  tok_type::key_next},
      {U"break", tok_type::key_break},
      {U"for",   tok_type::key_for},
      {U"with",  tok_type::key_with},
      {U"false", tok_type::key_false},
      {U"true",  tok_type::key_true},
      {U"nil",   tok_type::key_nil},
      {U"of",   tok_type::key_of},
    };

  static std::unordered_map<char32_t, tok_type> symbols =
    {
      {U']',  tok_type::r_square},
      {U'{',  tok_type::l_curly},
      {U'}',  tok_type::r_curly},
      {U'(',  tok_type::l_paren},
      {U')',  tok_type::r_paren},
      {U'&',  tok_type::sym_and},
      {U'|',  tok_type::sym_or},
      {U',',  tok_type::comma},
      {U'.',  tok_type::dot},
      {U'!',  tok_type::exclaim},
      {U';',  tok_type::semicolon},
      {U'\\', tok_type::backslash},
      {U'@',  tok_type::at},
      {U'?',  tok_type::question},
    };

  lexer::lexer(std::string const& path) :
    text(
      ([&path] {
        std::stringstream out;
        out << std::ifstream(path).rdbuf();
        return to_utf32(out.str());
      })()) {
    ch = text[0];
  }

  void lexer::advance() {
    switch (ch) {
      case '\n':cpos.idx++, cpos.row++, cpos.col = 1;
        break;
      case '\r':break;
      default:cpos.idx++, cpos.col++;
    }

    ch = cpos.idx >= text.length() ? '\0' : text[cpos.idx];
  }

  std::vector<token> lexer::lex() {
    std::vector<token> toks;

    while (cpos.idx < text.length()) {
      pos begin = cpos;
      if (is_id_start(ch)) {
        std::u32string buf;
        while (is_id_continue(ch)) {
          buf += ch;
          advance();
        }

        if (keywords.contains(buf)) {
          toks.emplace_back(keywords[buf], to_utf8(buf), begin, cpos);
        } else {
          toks.emplace_back(tok_type::id, to_utf8(buf), begin, cpos);
        }
      } else if (ch == '=') {
        advance();
        if (ch == '=') {
          advance();
          toks.emplace_back(tok_type::eq, "==", begin, cpos);
        } else {
          toks.emplace_back(tok_type::assign, "=", begin, cpos);
        }
      } else if (ch == '[') {
        advance();
        if (ch == '|') {
          advance();
          toks.emplace_back(tok_type::l_object, "[|", begin, cpos);
        } else {
          toks.emplace_back(tok_type::l_square, "[", begin, cpos);
        }
      } else if (ch == '|') {
        advance();

        if (ch == ']') {
          advance();
          toks.emplace_back(tok_type::r_object, "[|", begin, cpos);
        } else {
          toks.emplace_back(tok_type::sym_or, "|", begin, cpos);
        }
      } else if (ch == '+') {
        advance();

        if (ch == '=') {
          advance();
          toks.emplace_back(tok_type::add_assign, "+=", begin, cpos);
        } else if (ch == '+') {
          advance();
          toks.emplace_back(tok_type::inc, "++", begin, cpos);
        } else {
          toks.emplace_back(tok_type::add, "+", begin, cpos);
        }
      } else if (ch == '-') {
        advance();

        if (ch == '=') {
          advance();
          toks.emplace_back(tok_type::sub_assign, "-=", begin, cpos);
        } else if (ch == '>') {
          advance();
          toks.emplace_back(tok_type::r_arrow, "->", begin, cpos);
        } else if (ch == '-') {
          advance();
          toks.emplace_back(tok_type::dec, "--", begin, cpos);
        } else {
          toks.emplace_back(tok_type::sub, "-", begin, cpos);
        }
      } else if (ch == '<') {
        advance();

        if (ch == '>') {
          advance();
          toks.emplace_back(tok_type::neq, "<>", begin, cpos);
        } else if (ch == '-') {
          advance();
          toks.emplace_back(tok_type::l_arrow, "<-", begin, cpos);
        } else if (ch == '=') {
          advance();
          toks.emplace_back(tok_type::less_than_equal, "<=", begin, cpos);
        } else {
          toks.emplace_back(tok_type::less_than, "<", begin, cpos);
        }
      } else if (ch == '>') {
        advance();

        if (ch == '=') {
          advance();
          toks.emplace_back(tok_type::greater_than_equal, ">=", begin, cpos);
        } else {
          toks.emplace_back(tok_type::greater_than, ">", begin, cpos);
        }
      } else if (ch == '/') {
        advance();
        if (ch == '/') {
          advance();
          while (ch != '\n' && ch != '\0') {
            advance();
          }
        } else if (ch == '=') {
          advance();
          toks.emplace_back(tok_type::div_assign, "/=", begin, cpos);
        } else {
          toks.emplace_back(tok_type::div, "/", begin, cpos);
        }
      } else if (ch == '*') {
        advance();
        if (ch == '=') {
          advance();
          toks.emplace_back(tok_type::mul_assign, "*=", begin, cpos);
        } else {
          toks.emplace_back(tok_type::mul, "*", begin, cpos);
        }
      } else if (ch == '%') {
        advance();
        if (ch == '=') {
          advance();
          toks.emplace_back(tok_type::mod_assign, "%=", begin, cpos);
        } else {
          toks.emplace_back(tok_type::mod, "%", begin, cpos);
        }
      } else if (symbols.contains(ch)) {
        char32_t cur = ch;
        advance();
        toks.emplace_back(symbols[cur], to_utf8(std::u32string(1, cur)), begin,
                          cpos);
      } else if (isdigit(ch)) {
        std::u32string buf;
        bool range = false;
        while (isdigit(ch) || ch == '.') {
          buf += ch;
          advance();
          if (buf.ends_with(U"..")) {
            buf = buf.substr(0, buf.length() - 2);
            range = true;
            break;
          }
        }

        toks.emplace_back(tok_type::number, to_utf8(buf), begin, cpos);

        if (range) {
          toks.emplace_back(tok_type::range, "..", begin, cpos);
        }
      } else if (ch == '"') {
        advance();
        std::u32string buf;
        while (ch != '"') {
          if (ch == '\\') {
            advance();
            char32_t esc;
            switch (ch) {
              case 'n':esc = '\n';
                break;
              default:
                throw std::runtime_error(
                  std::string("Unknown escape character ") +
                  std::to_string(ch));
            }

            buf += esc;
            advance();
          } else {
            buf += ch;
            advance();
          }
        }

        advance();
        toks.emplace_back(tok_type::string, to_utf8(buf), begin, cpos);
      } else if (ch == ':') {
        advance();
        if (ch == '=') {
          advance();
          toks.emplace_back(tok_type::walrus, ":=", begin, cpos);
        } else {
          toks.emplace_back(tok_type::colon, ":", begin, cpos);
        }
      } else if (ch == '`') {
        advance();
        std::u32string buf;
        while (ch != '`') {
          buf += ch;
          advance();
        }

        advance();
        toks.emplace_back(tok_type::id, to_utf8(buf), begin, cpos);
      } else if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') {
        advance();
      } else {
        throw std::runtime_error(
          std::string("Failed to read next token at ")
          + cpos.to_string()
          + " with character "
          + std::to_string(int(ch)));
      }
    }

    toks.emplace_back(tok_type::eof, "", cpos, cpos);

    return toks;
  }

  bool lexer::is_id_start(char32_t ch) {
    return isalpha(ch)
           || ch == '_'
           || ch == '$'
           || (ch >= 0xff61 && ch <= 0xff9F) // half-width katakana
           || (ch >= 0x0300 && ch <= 0x036f) // diacritics (combining characters)
           || (ch >= 128 && ch <= 255); // diacritics (extended ascii)
  }

  bool lexer::is_id_continue(char32_t ch) {
    return is_id_start(ch) || isdigit(ch);
  }

  std::string token::to_string() const {
    std::string buf = "token{type=";
    buf += tosuto::to_string(type);
    buf += ", lexeme=";
    buf += lexeme;
    buf += ", begin=";
    buf += begin.to_string();
    buf += ", end=";
    buf += end.to_string();
    buf += "}";
    return buf;
  }
}