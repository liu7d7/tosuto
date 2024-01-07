#include <sstream>
#include "lex.h"

namespace ami {
  static std::unordered_map<std::u16string, tok_type> keywords =
    {
      {u"if",    tok_type::key_if},
      {u"elif",  tok_type::key_elif},
      {u"else",  tok_type::key_else},
      {u"fun",   tok_type::key_fun},
      {u"ret",   tok_type::key_ret},
      {u"next",  tok_type::key_next},
      {u"break", tok_type::key_break},
      {u"for",   tok_type::key_for},
      {u"with",  tok_type::key_with},
      {u"false", tok_type::key_false},
      {u"true",  tok_type::key_true},
      {u"nil",   tok_type::key_nil},
    };

  static std::unordered_map<char16_t, tok_type> symbols =
    {
      {']',  tok_type::r_square},
      {'{',  tok_type::l_curly},
      {'}',  tok_type::r_curly},
      {'(',  tok_type::l_paren},
      {')',  tok_type::r_paren},
      {'&',  tok_type::sym_and},
      {'|',  tok_type::sym_or},
      {',',  tok_type::comma},
      {'.',  tok_type::dot},
      {'!',  tok_type::exclaim},
      {';',  tok_type::semicolon},
      {'\\', tok_type::backslash},
      {'@',  tok_type::at},
    };

  lexer::lexer(std::string const& path) :
    text(
      ([&path] {
        std::stringstream out;
        out << std::ifstream(path).rdbuf();
        return to_utf16(out.str());
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
        std::u16string buf;
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
          while (ch != '\n') {
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
        char16_t cur = ch;
        advance();
        toks.emplace_back(symbols[cur], to_utf8(std::u16string(1, cur)), begin,
                          cpos);
      } else if (isdigit(ch)) {
        std::u16string buf;
        bool range = false;
        while (isdigit(ch) || ch == '.') {
          buf += ch;
          advance();
          if (buf.ends_with(u"..")) {
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
        std::u16string buf;
        while (ch != '"') {
          if (ch == '\\') {
            advance();
            char16_t esc;
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
        std::u16string buf;
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

  bool lexer::is_id_start(char16_t ch) {
    return isalpha(ch)
           || ch == '_'
           || ch == '$'
           || (ch >= 0xff61 && ch <= 0xff9F) // half-width katakana
           ||
           (ch >= 0x0300 && ch <= 0x036f) // diacritics (combining characters)
           || (ch >= 128 && ch <= 255); // diacritics (extended ascii)
  }

  bool lexer::is_id_continue(char16_t ch) {
    return is_id_start(ch) || isdigit(ch);
  }

  std::string token::to_string() const {
    std::string buf = "token{type=";
    buf += ami::to_string(type);
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