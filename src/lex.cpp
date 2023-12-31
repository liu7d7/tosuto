#include <sstream>
#include "lex.h"

namespace ami
{
  lexer::lexer(std::string const& path) :
    text(
      ([&path]
      {
        std::stringstream out;
        out << std::ifstream(path).rdbuf();
        return out.str();
      })())
  {
    ch = text[0];
  }

  void lexer::advance()
  {
    switch (ch)
    {
      case '\n':
        cpos.idx++, cpos.row++, cpos.col = 1;
        break;
      case '\r':
        break;
      default:
        cpos.idx++, cpos.col++;
    }

    ch = cpos.idx >= text.length() ? '\0' : text[cpos.idx];
  }

  std::vector<token> lexer::lex()
  {
    std::vector<token> toks;

    while (cpos.idx < text.length())
    {
      pos begin = cpos;
      if (is_id_start(ch))
      {
        std::string buf;
        while (is_id_continue(ch))
        {
          buf += ch;
          advance();
        }

        if (keywords.contains(buf))
        {
          toks.emplace_back(keywords[buf], buf, begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::id, buf, begin, cpos);
        }
      }
      else if (ch == '=')
      {
        advance();
        if (ch == '=')
        {
          advance();
          toks.emplace_back(tok_type::eq, "==", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::assign, "=", begin, cpos);
        }
      }
      else if (ch == '[')
      {
        advance();
        if (ch == '|')
        {
          advance();
          toks.emplace_back(tok_type::l_object, "[|", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::l_square, "[", begin, cpos);
        }
      }
      else if (ch == '|')
      {
        advance();

        if (ch == ']')
        {
          advance();
          toks.emplace_back(tok_type::r_object, "[|", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::sym_or, "|", begin, cpos);
        }
      }
      else if (ch == '+')
      {
        advance();

        if (ch == '=')
        {
          advance();
          toks.emplace_back(tok_type::add_assign, "+=", begin, cpos);
        }
        else if (ch == '+')
        {
          advance();
          toks.emplace_back(tok_type::inc, "++", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::add, "+", begin, cpos);
        }
      }
      else if (ch == '-')
      {
        advance();

        if (ch == '=')
        {
          advance();
          toks.emplace_back(tok_type::sub_assign, "-=", begin, cpos);
        }
        else if (ch == '>')
        {
          advance();
          toks.emplace_back(tok_type::r_arrow, "->", begin, cpos);
        }
        else if (ch == '-')
        {
          advance();
          toks.emplace_back(tok_type::dec, "--", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::sub, "-", begin, cpos);
        }
      }
      else if (ch == '<')
      {
        advance();

        if (ch == '>')
        {
          advance();
          toks.emplace_back(tok_type::neq, "<>", begin, cpos);
        }
        else if (ch == '-')
        {
          advance();
          toks.emplace_back(tok_type::l_arrow, "<-", begin, cpos);
        }
        else if (ch == '=')
        {
          advance();
          toks.emplace_back(tok_type::less_than_equal, "<=", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::less_than, "<", begin, cpos);
        }
      }
      else if (ch == '>')
      {
        advance();

        if (ch == '=')
        {
          advance();
          toks.emplace_back(tok_type::greater_than_equal, ">=", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::greater_than, ">", begin, cpos);
        }
      }
      else if (ch == '/')
      {
        advance();
        if (ch == '/')
        {
          advance();
          while (ch != '\n')
          {
            advance();
          }
        }
        else if (ch == '=')
        {
          advance();
          toks.emplace_back(tok_type::div_assign, "/=", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::div, "/", begin, cpos);
        }
      }
      else if (ch == '*')
      {
        advance();
        if (ch == '=')
        {
          advance();
          toks.emplace_back(tok_type::mul_assign, "*=", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::mul, "*", begin, cpos);
        }
      }
      else if (ch == '%')
      {
        if (ch == '=')
        {
          advance();
          toks.emplace_back(tok_type::mod_assign, "%=", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::mod, "%", begin, cpos);
        }
      }
      else if (symbols.contains(ch))
      {
        char cur = ch;
        advance();
        toks.emplace_back(symbols[cur], std::string(1, cur), begin, cpos);
      }
      else if (isdigit(ch))
      {
        std::string buf;
        bool range = false;
        while (isdigit(ch) || ch == '.')
        {
          buf += ch;
          advance();
          if (buf.ends_with(".."))
          {
            buf = buf.substr(0, buf.length() - 2);
            range = true;
            break;
          }
        }

        toks.emplace_back(tok_type::number, buf, begin, cpos);

        if (range)
        {
          toks.emplace_back(tok_type::range, buf, begin, cpos);
        }
      }
      else if (ch == '"')
      {
        advance();
        std::string buf;
        while (ch != '"')
        {
          if (ch == '\\')
          {
            advance();
            char esc;
            switch (ch)
            {
              case 'n':
                esc = '\n';
                break;
              default:
                throw std::runtime_error(
                  std::string("Unknown escape char ") + ch);
            }

            buf += esc;
            advance();
          }
          else
          {
            buf += ch;
            advance();
          }
        }

        advance();
        toks.emplace_back(tok_type::string, buf, begin, cpos);
      }
      else if (ch == ':')
      {
        advance();
        if (ch == '=')
        {
          advance();
          toks.emplace_back(tok_type::walrus, ":=", begin, cpos);
        }
        else
        {
          toks.emplace_back(tok_type::colon, ":", begin, cpos);
        }
      }
      else if (ch == '`')
      {
        advance();
        std::string buf;
        while (ch != '`')
        {
          buf += ch;
          advance();
        }

        advance();
        toks.emplace_back(tok_type::id, buf, begin, cpos);
      }
      else if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t')
      {
        advance();
      }
      else
      {
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

  bool lexer::is_id_start(char ch)
  {
    return isalpha(ch) || ch == '_' || ch == '$';
  }

  bool lexer::is_id_continue(char ch)
  {
    return is_id_start(ch) || isdigit(ch);
  }

  std::unordered_map<std::string, tok_type> lexer::keywords =
    {
      {"if",    tok_type::key_if},
      {"elif",  tok_type::key_elif},
      {"else",  tok_type::key_else},
      {"fun",   tok_type::key_fun},
      {"ret",   tok_type::key_ret},
      {"next",  tok_type::key_next},
      {"break", tok_type::key_break},
      {"for",   tok_type::key_for},
      {"with",  tok_type::key_with},
    };

  std::unordered_map<char, tok_type> lexer::symbols =
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
    };

  std::string token::to_string() const
  {
    std::string buf = "token{type=";
    buf += ami::to_string(type);
    buf += ", lexeme=";
    buf += lexeme;
    buf += ", iterator=";
    buf += begin.to_string();
    buf += ", end=";
    buf += end.to_string();
    buf += "}";
    return buf;
  }
}