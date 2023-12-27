#include <utility>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include "ami.h"

template<typename V>
V ami::yeah<V>::held;

#define ami_unwrap(exp) *(yeah<decltype(exp)>::held = exp); if (!yeah<decltype(exp)>::held.has_value()) return std::unexpected(yeah<decltype(exp)>::held.error())
#define ami_discard(exp) if (!(yeah<decltype(exp)>::held = exp).has_value()) return std::unexpected(yeah<decltype(exp)>::held.error())

namespace ami
{
  lexer::lexer(const std::string& path) :
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
          toks.emplace_back(tok_type::equal, "==", begin, cpos);
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
          toks.emplace_back(tok_type::not_equal, "<>", begin, cpos);
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
      {"for", tok_type::key_for},
    };

  std::unordered_map<char, tok_type> lexer::symbols =
    {
      {']', tok_type::r_square},
      {'{', tok_type::l_curly},
      {'}', tok_type::r_curly},
      {'(', tok_type::l_paren},
      {')', tok_type::r_paren},
      {'&', tok_type::sym_and},
      {'|', tok_type::sym_or},
      {',', tok_type::comma},
      {'.', tok_type::dot},
      {'!', tok_type::exclaim},
      {';', tok_type::semicolon},
    };

  std::string token::to_string() const
  {
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

  std::string pos::to_string() const
  {
    std::string buf = "pos{idx=";
    buf += std::to_string(idx);
    buf += ", col=";
    buf += std::to_string(col);
    buf += ", row=";
    buf += std::to_string(row);
    buf += "}";
    return buf;
  }

  parser::parser(std::vector<token> toks) :
    toks(std::move(toks)),
    idx(0)
  {
    tok = this->toks[0];
    next = this->toks[1];
  }

  void parser::advance()
  {
    idx++;
    tok = toks[idx];
    next = toks[std::min(toks.size() - 1, idx + 1)];
  }

  std::expected<node*, std::string> parser::block()
  {
    pos begin = tok.begin;
    ami_discard(expect(tok_type::l_curly));

    std::vector<node*> exprs;
    while (tok.type != tok_type::r_curly)
    {
      auto exp = statement();
      if (!exp.has_value()) return exp;
      exprs.push_back(exp.value());
    }

    advance();
    return new block_node(exprs, begin, tok.begin);
  }

  std::expected<node*, std::string> parser::global()
  {
    pos begin = tok.begin;
    std::vector<node*> exprs;
    while (tok.type != tok_type::eof)
    {
      auto exp = statement();
      if (!exp.has_value()) return exp;
      exprs.push_back(exp.value());
    }

    return new block_node(exprs, begin, tok.begin);
  }

  std::expected<node*, std::string> parser::for_loop()
  {
    pos begin = tok.begin;
    ami_discard(expect(tok_type::key_for));
    auto id = ami_unwrap(expect(tok_type::id));
    ami_discard(expect(tok_type::colon));
    auto iterable = ami_unwrap(expr());
    auto body = ami_unwrap(block());

    return new for_node(id.lexeme, iterable, body, begin, body->end);
  }

  std::expected<node*, std::string> parser::statement()
  {
    if (tok.type == tok_type::id &&
        (next.type == tok_type::colon || next.type == tok_type::l_curly))
    {
      return function();
    }

    switch (tok.type)
    {
      case tok_type::key_for:
      {
        return for_loop();
      }
      case tok_type::key_ret:
      {
        token cur = tok;
        advance();
        node* ret_val = nullptr;
        auto exp = expr();
        if (exp.has_value()) ret_val = exp.value();

        return new ret_node(ret_val, cur.begin,
                            ret_val ? ret_val->end : cur.end);
      }
      case tok_type::key_next:
      {
        token cur = tok;
        advance();
        return new next_node(cur.begin, cur.end);
      }
      case tok_type::key_break:
      {
        token cur = tok;
        advance();
        return new break_node(cur.begin, cur.end);
      }
      default:
        return expr();
    }
  }

  std::expected<node*, std::string> parser::expr()
  {
    return define();
  }

  std::expected<node*, std::string> parser::define()
  {
    if (tok.type != tok_type::id)
    {
      return assign();
    }

    std::string id = tok.lexeme;
    node* lhs = ami_unwrap(assign());
    while (tok.type == tok_type::walrus)
    {
      advance();
      node* rhs = ami_unwrap(assign());
      lhs = new var_def_node(id, rhs, lhs->begin, rhs->end);
    }

    return lhs;
  }

  static std::unordered_set<tok_type> assign_ops
    {
      tok_type::add_assign,
      tok_type::mul_assign,
      tok_type::div_assign,
      tok_type::mod_assign,
      tok_type::sub_assign,
      tok_type::assign
    };

  std::expected<node*, std::string> parser::assign()
  {
    node* lhs = ami_unwrap(sym_or());
    while (assign_ops.contains(tok.type))
    {
      tok_type type = tok.type;
      advance();
      node* rhs = ami_unwrap(sym_or());
      lhs = new bin_op_node(lhs, rhs, type, lhs->begin, rhs->end);
    }

    return lhs;
  }

  std::expected<node*, std::string> parser::sym_or()
  {
    node* lhs = ami_unwrap(sym_and());
    while (tok.type == tok_type::sym_or)
    {
      advance();
      node* rhs = ami_unwrap(sym_and());
      lhs = new bin_op_node(lhs, rhs, tok_type::sym_or, lhs->begin, rhs->end);
    }

    return lhs;
  }

  std::expected<node*, std::string> parser::sym_and()
  {
    node* lhs = ami_unwrap(comp());
    while (tok.type == tok_type::sym_and)
    {
      advance();
      node* rhs = ami_unwrap(comp());
      lhs = new bin_op_node(lhs, rhs, tok_type::sym_and, lhs->begin, rhs->end);
    }

    return lhs;
  }

  static std::unordered_set<tok_type> comp_ops
    {
      tok_type::equal,
      tok_type::not_equal,
      tok_type::less_than,
      tok_type::less_than_equal,
      tok_type::greater_than,
      tok_type::greater_than_equal
    };

  std::expected<node*, std::string> parser::comp()
  {
    node* lhs = ami_unwrap(add());
    while (comp_ops.contains(tok.type))
    {
      tok_type type = tok.type;
      advance();
      node* rhs = ami_unwrap(add());
      lhs = new bin_op_node(lhs, rhs, type, lhs->begin, rhs->end);
    }

    return lhs;
  }

  static std::unordered_set<tok_type> add_ops
    {
      tok_type::add,
      tok_type::sub
    };

  std::expected<node*, std::string> parser::add()
  {
    node* lhs = ami_unwrap(mul());
    while (add_ops.contains(tok.type))
    {
      tok_type type = tok.type;
      advance();
      node* rhs = ami_unwrap(mul());
      lhs = new bin_op_node(lhs, rhs, type, lhs->begin, rhs->end);
    }

    return lhs;
  }

  static std::unordered_set<tok_type> mul_ops
    {
      tok_type::mul,
      tok_type::div
    };

  std::expected<node*, std::string> parser::mul()
  {
    node* lhs = ami_unwrap(pre_unary());
    while (mul_ops.contains(tok.type))
    {
      tok_type type = tok.type;
      advance();
      node* rhs = ami_unwrap(pre_unary());
      lhs = new bin_op_node(lhs, rhs, type, lhs->begin, rhs->end);
    }

    return lhs;
  }

  static std::unordered_set<tok_type> pre_unary_ops
    {
      tok_type::exclaim,
      tok_type::add,
      tok_type::sub
    };

  std::expected<node*, std::string> parser::pre_unary()
  {
    pos begin = tok.begin;
    if (pre_unary_ops.contains(tok.type))
    {
      tok_type type = tok.type;
      advance();
      node* body = ami_unwrap(post_unary());
      return new un_op_node(body, type, begin, body->end);
    }

    return post_unary();
  }

  static std::unordered_set<tok_type> post_unary_ops
    {
      tok_type::inc,
      tok_type::dec
    };

  std::expected<node*, std::string> parser::post_unary()
  {
    pos begin = tok.begin;
    node* body = ami_unwrap(call());
    if (post_unary_ops.contains(tok.type))
    {
      tok_type type = tok.type;
      advance();
      return new un_op_node(body, type, begin, body->end);
    }

    return body;
  }

  std::expected<node*, std::string> parser::call()
  {
    pos begin = tok.begin;
    node* body = ami_unwrap(field_get());
    while (tok.type == tok_type::l_paren)
    {
      advance();
      std::vector<node*> args;
      while (tok.type != tok_type::r_paren)
      {
        node* exp = ami_unwrap(expr());
        args.push_back(exp);
        if (tok.type == tok_type::comma)
        {
          advance();
        }
      }

      advance();
      return new call_node(
        body, args, begin, args.empty() ? body->end : args.back()->end);
    }

    return body;
  }

  std::expected<node*, std::string> parser::field_get()
  {
    pos begin = tok.begin;
    node* body = ami_unwrap(atom());
    while (tok.type == tok_type::dot)
    {
      advance();
      token id = ami_unwrap(expect(tok_type::id));

      body = new field_get_node(body, id.lexeme, begin, id.end);
    }

    return body;
  }

  std::expected<node*, std::string> parser::atom()
  {
    pos begin = tok.begin;
    switch (tok.type)
    {
      case tok_type::l_paren:
      {
        advance();
        node* exp = ami_unwrap(expr());
        ami_discard(expect(tok_type::r_paren));
        return exp;
      }
      case tok_type::number:
      {
        double value = std::stod(tok.lexeme);
        advance();
        if (tok.type == tok_type::range)
        {
          advance();
          token num = ami_unwrap(expect(tok_type::number));
          return new range_node(value, std::stod(num.lexeme), begin, tok.begin);
        }

        return new number_node(value, tok.begin, tok.end);;
      }
      case tok_type::string:
      {
        node* nod = new string_node(tok.lexeme, tok.begin, tok.end);
        advance();
        return nod;
      }
      case tok_type::key_if:
      {
        std::vector<std::pair<node*, node*>> cases;
        advance();
        auto exp = ami_unwrap(expr());
        auto body = ami_unwrap(block());
        cases.emplace_back(exp, body);
        while (tok.type == tok_type::key_elif)
        {
          advance();
          exp = ami_unwrap(expr());
          body = ami_unwrap(block());
          cases.emplace_back(exp, body);
        }

        node* other = nullptr;
        if (tok.type == tok_type::key_else)
        {
          advance();
          other = ami_unwrap(block());
        }

        return new if_node(cases, other, begin, tok.begin);
      }
      case tok_type::id:
      {
        node* nod = new field_get_node(nullptr, tok.lexeme, begin, tok.begin);
        advance();
        return nod;
      }
      case tok_type::l_object:
      {
        advance();
        std::vector<std::pair<std::string, node*>> fields;
        while (tok.type == tok_type::id)
        {
          auto id = tok.lexeme;
          advance();

          ami_discard(expect(tok_type::assign));
          node* body = ami_unwrap(expr());
          fields.emplace_back(id, body);

          consume(tok_type::comma);
        }

        ami_discard(expect(tok_type::r_object));
        return new object_node(fields, begin, tok.begin);
      }
      default:
        return std::unexpected("Expected atom, got " + tok.to_string() + " in " + __PRETTY_FUNCTION__);
    }
  }

  std::expected<token, std::string> parser::expect(tok_type type, bool do_advance, std::source_location loc)
  {
    if (tok.type != type)
      return std::unexpected(
        "Expected " + to_string(type) + ", got " + tok.to_string() + " at " + loc.function_name());

    token cur = tok;
    if (do_advance) advance();
    return cur;
  }

  void parser::consume(tok_type type)
  {
    if (tok.type == type) advance();
  }

  std::expected<node*, std::string> parser::function()
  {
    pos begin = tok.begin;
    token id = ami_unwrap(expect(tok_type::id));

    std::vector<std::string> args;
    if (tok.type == tok_type::colon)
    {
      advance();
      while (tok.type == tok_type::id)
      {
        args.push_back(tok.lexeme);
        advance();
      }
    }

    if (tok.type == tok_type::r_arrow)
    {
      advance();
      node* body = ami_unwrap(expr());
      return new fn_def_node(id.lexeme, args, body, begin, body->end);
    }

    ami_discard(expect(tok_type::l_curly, false));
    node* body = ami_unwrap(block());
    return new fn_def_node(id.lexeme, args, body, begin, body->end);
  }

  node::node(node_type type, pos begin, pos end) :
    type(type),
    begin(begin),
    end(end)
  {}

  std::string node::pretty(int indent) const
  {
    throw std::runtime_error("TODO: implement");
  }

  fn_def_node::fn_def_node(std::string name, std::vector<std::string> args,
                           node* body, pos begin, pos end) :
    name(std::move(name)),
    args(std::move(args)),
    body(body),
    node(node_type::fn_def, begin, end)
  {}

  std::string fn_def_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::string arg_str;
    for (auto const& it: args)
    {
      arg_str += it;
      arg_str += ", ";
    }

    if (!arg_str.empty())
    {
      arg_str = arg_str.substr(0, arg_str.length() - 2);
    }

    return (
      std::stringstream()
        << "fn_def_node: {\n"
        << ind << "name: " << name << ",\n"
        << ind << "args: [" << arg_str << ']' << ",\n"
        << ind << "body: " << body->pretty(indent + 1) << '\n'
        << std::string(indent * 2, ' ') << '}').str();
  }

  block_node::block_node(std::vector<node*> exprs, pos begin, pos end) :
    exprs(std::move(exprs)),
    node(node_type::block, begin, end)
  {}

  std::string block_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::stringstream ss;
    ss << "block: [\n";
    for (auto it: exprs)
    {
      ss << ind << it->pretty(indent + 1) << ",\n";
    }

    ss << std::string(indent * 2, ' ') << ']';
    return ss.str();
  }

  call_node::call_node(node* callee, std::vector<node*> args, pos begin,
                       pos end) :
    callee(callee),
    args(std::move(args)),
    node(node_type::call, begin, end)
  {}

  std::string call_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::string arg_str;
    for (auto const& it: args)
    {
      arg_str += ind;
      arg_str += "  ";
      arg_str += it->pretty(indent + 2);
      arg_str += ",\n";
    }

    if (!arg_str.empty())
    {
      arg_str = arg_str.substr(0, arg_str.length() - 2);
    }

    return (
      std::stringstream()
        << "call_node: {\n"
        << ind << "callee: " << callee->pretty(indent + 1) << ",\n"
        << ind << "args: [\n" << arg_str << '\n' << ind << ']' << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
  }

  un_op_node::un_op_node(node* target, tok_type op, pos begin, pos end) :
    target(target),
    op(op),
    node(node_type::un_op, begin, end)
  {}

  std::string un_op_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "un_op_node: {\n"
        << ind << "op: " << to_string(op) << ",\n"
        << ind << "target: " << target->pretty(indent + 1) << '\n'
        << std::string(indent * 2, ' ') << '}').str();
  }

  bin_op_node::bin_op_node(node* lhs, node* rhs, tok_type op, pos begin,
                           pos end) :
    lhs(lhs),
    rhs(rhs),
    op(op),
    node(node_type::bin_op, begin, end)
  {}

  std::string bin_op_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "bin_op_node: {\n"
        << ind << "op: " << to_string(op) << ",\n"
        << ind << "lhs: " << lhs->pretty(indent + 1) << '\n'
        << ind << "rhs: " << rhs->pretty(indent + 1) << '\n'
        << std::string(indent * 2, ' ') << '}').str();
  }

  number_node::number_node(double value, pos begin, pos end) :
    value(value),
    node(node_type::number, begin, end)
  {}

  std::string number_node::pretty(int indent) const
  {
    return "number_node: " + std::to_string(value);
  }

  string_node::string_node(std::string value, pos begin, pos end) :
    value(std::move(value)),
    node(node_type::string, begin, end)
  {}

  std::string string_node::pretty(int indent) const
  {
    return "string_node: \"" + value + "\"";
  }

  object_node::object_node(decltype(fields) fields, pos begin, pos end) :
    fields(std::move(fields)),
    node(node_type::object, begin, end)
  {}

  std::string object_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');

    auto ss = std::stringstream() << "bin_op_node: {\n";
    for (auto const& it : fields)
    {
      ss << ind << it.first << ": " << it.second->pretty(indent + 1) << '\n';
    }

    ss << std::string(indent * 2, ' ') << '}';

    return ss.str();
  }

  field_get_node::field_get_node(node* target, std::string field, pos begin,
                                 pos end) :
    target(target),
    field(std::move(field)),
    node(node_type::field_get, begin, end)
  {}

  std::string field_get_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "field_get_node: {\n"
        << ind << "target: " << (target ? target->pretty(indent + 1) : "nil") << ",\n"
        << ind << "field: " << field << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
  }

  var_def_node::var_def_node(std::string name, node* value, pos begin, pos end)
    :
    name(std::move(name)),
    value(value),
    node(node_type::var_def, begin, end)
  {}

  std::string var_def_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "var_def_node: {\n"
        << ind << "name: " << name << ",\n"
        << ind << "value: " << value->pretty(indent + 1) << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
  }

  if_node::if_node(decltype(cases) cases, node* else_case, pos begin, pos end) :
    cases(std::move(cases)),
    else_case(else_case),
    node(node_type::if_stmt, begin, end)
  {}

  std::string if_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');

    std::string cases_str;
    for (auto const& it : cases)
    {
      cases_str += ind;
      cases_str += "  ";
      cases_str += "case{\n";
      cases_str += ind;
      cases_str += "    cond: ";
      cases_str += it.first->pretty(indent + 3);
      cases_str += '\n';
      cases_str += ind;
      cases_str += "    body: ";
      cases_str += it.second->pretty(indent + 3);
      cases_str += '\n';
      cases_str += ind;
      cases_str += "  }";
    }

    auto ss = std::stringstream()
      << "if_stmt_node: {\n"
      << ind << "cases: [\n" << cases_str << '\n' << ind << ']' << (else_case ? ",\n" : "\n");

    if (else_case)
    {
      ss << ind << "other: " << else_case->pretty(indent + 1) << '\n';
    }

    ss << std::string(indent * 2, ' ') << '}';

    return ss.str();
  }

  ret_node::ret_node(node* ret_val, pos begin, pos end) :
    ret_val(ret_val),
    node(node_type::ret, begin, end)
  {}

  std::string ret_node::pretty(int indent) const
  {
    return ret_val ? "ret: " + ret_val->pretty(indent + 1) : "ret";
  }

  next_node::next_node(pos begin, pos end) : node(node_type::next, begin, end)
  {}

  std::string next_node::pretty(int indent) const
  {
    return "next";
  }

  break_node::break_node(pos begin, pos end) : node(node_type::brk, begin, end)
  {}

  std::string break_node::pretty(int indent) const
  {
    return "break";
  }

  range_node::range_node(double start, double finish, pos begin, pos end) :
    start(start),
    finish(finish),
    node(node_type::range, begin, end) {}

  std::string range_node::pretty(int indent) const
  {
    return "range_node: " + std::to_string(start) + ".." + std::to_string(finish);
  }

  std::string for_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "for_node: {\n"
        << ind << "id: " << id << ",\n"
        << ind << "iterable: " << iterable->pretty(indent + 1) << ",\n"
        << ind << "body: " << body->pretty(indent + 1) << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
  }

  std::unordered_map<
    node_type,
    std::expected<value, std::string>(interpreter::*)(node* nod, symbol_table& sym)> interpreter::interps
    {
      {node_type::block, &interpreter::block},
    };

  std::expected<value, std::string>
  interpreter::block(node* nod, symbol_table& sym)
  {
    if (auto it = dynamic_cast<block_node*>(nod))
    {
      value ret{nil_type{}};
      symbol_table new_sym{&sym, std::unordered_map<std::string, value>()};

      for (auto exp : it->exprs)
      {
        ret = ami_unwrap(interpret(exp, new_sym));
        switch (exp->type)
        {
          case node_type::next:
          {
            has_next = true;
            return ret;
          }
          case node_type::brk:
          {
            has_break = true;
            return ret;
          }
          case node_type::ret:
          {
            has_return = true;
            return ret;
          }
          default:;
        }
      }
    }

    return fail(nod);
  }

  std::expected<value, std::string>
  interpreter::fn_def(node* nod, symbol_table& sym)
  {
    if (auto it = dynamic_cast<fn_def_node*>(nod))
    {
      sym.set(it->name, value{it});
      return value{nil_type{}};
    }

    return fail(nod);
  }

  std::expected<value, std::string>
  interpreter::call(node* nod, symbol_table& sym)
  {
    if (auto it = dynamic_cast<call_node*>(nod))
    {
      auto fn = interpret(it->callee, sym);
      auto args = std::vector<value>();
      args.reserve(it->args.size());
      for (auto no : it->args)
      {
        value val = ami_unwrap(interpret(no, sym));
        args.push_back(std::move(val));
      }

      return fn->call(args, sym);
    }

    return fail(nod);
  }

  std::expected<value, std::string>
  interpreter::interpret(node* nod, symbol_table& sym)
  {
    return (this->*interps[nod->type])(nod, sym);
  }

  std::unexpected<std::string>
  interpreter::fail(node* nod, std::source_location loc)
  {
    return std::unexpected("Failed to interpret " + nod->pretty(0) + " in " + loc.function_name());
  }

  symbol_table::symbol_table(symbol_table* parent,
                             std::unordered_map<std::string, value> vals)
    : parent(parent), vals(std::move(vals))
  {}

  std::expected<value, std::string> const&
  symbol_table::get(const std::string& name) const
  {
    if (vals.contains(name)) return vals.at(name);
    if (!parent) return std::unexpected("Failed to find value");
    return parent->get(name);
  }

  std::expected<value, std::string> const&
  symbol_table::set(const std::string& name, value val)
  {
    if (vals.contains(name)) return vals[name] = std::move(val);
    if (!parent) return std::unexpected("Failed to find value");
    return parent->set(name, std::move(val));
  }

  value::value() : val(nil_type{}) {}

  value::value(decltype(val) val) : val(std::move(val))
  {

  }

  std::unexpected<std::string>
  value::fail(const std::string& message, std::source_location loc)
  {
    return std::unexpected{message + " at " + loc.function_name()};
  }

  std::string value::type_name() const
  {
    switch (val.index())
    {
      case 0: return "number";
      case 1: return "string";
      case 2: return "bool";
      case 3: return "object";
      case 4:
      case 5: return "function";
      case 6: return "nil";
    }
  }

  interpret_result value::add(const value& other)
  {
    switch (val.index())
    {
      case 0:
      {
        if (!std::holds_alternative<double>(other.val))
        {
          return fail("Cannot add num to anything other than num");
        }

        return value{get<double>(val) + get<double>(other.val)};
      }
      case 1:
      {
        if (!std::holds_alternative<std::string>(other.val))
        {
          return fail("Cannot add str to anything other than str");
        }

        return value{get<std::string>(val) + get<std::string>(other.val)};
      }
      default: return
          fail("Cannot do " + type_name() + " + " + other.type_name());
    }
  }

  interpret_result value::sub(const value& other)
  {
    switch (val.index())
    {
      case 0:
      {
        if (!std::holds_alternative<double>(other.val))
        {
          return fail("Cannot sub num from anything other than num");
        }

        return value{get<double>(val) - get<double>(other.val)};
      }
      default: return
          fail("Cannot do " + type_name() + " - " + other.type_name());
    }
  }

  interpret_result value::mul(const value& other)
  {
    switch (val.index())
    {
      case 0:
      {
        if (std::holds_alternative<double>(other.val))
        {
          return value{get<double>(val) * get<double>(other.val)};
        }

        if (std::holds_alternative<std::string>(other.val))
        {
          return value{repeat(get<std::string>(other.val), size_t(floor(get<double>(val))))};
        }

        return fail("Cannot mul num by anything other than num or str");
      }
      case 1:
      {
        if (!std::holds_alternative<double>(other.val))
        {
          return std::unexpected{"Cannot mul str to anything other than str"};
        }

        return value{repeat(get<std::string>(val), size_t(floor(get<double>(other.val))))};
      }
      default: return
          fail("Cannot do " + type_name() + " * " + other.type_name());
    }
  }

  interpret_result
  value::call(const std::vector<value>& args, symbol_table& sym)
  {
    switch (val.index())
    {
      case 4:
      {
        fn_def_node* fn_def = get<fn_def_node*>(val);
        interpreter interp{};
        std::unordered_map<std::string, value> arg_vals;
        if (args.size() != fn_def->args.size())
        {
          return fail("Wrong arity when calling function!");
        }

        for (size_t i = 0; i < args.size(); i++)
        {
          arg_vals[fn_def->args[i]] = args[i];
        }

        symbol_table new_sym = symbol_table{&sym, arg_vals};
        return interp.interpret(fn_def->body, new_sym);
      }
      case 5:
      {
        auto fn = get<builtin_fn>(val);
        return fn(args, sym);
      }
    }
  }

  interpret_result value::div(const value& other)
  {
    switch (val.index())
    {
      case 0:
      {
        if (!std::holds_alternative<double>(other.val))
        {
          return fail("Cannot div num by anything other than num");
        }

        return value{get<double>(val) / get<double>(other.val)};
      }
      default: return
          fail("Cannot do " + type_name() + " / " + other.type_name());
    }
  }

  interpret_result value::mod(const value& other)
  {
    switch (val.index())
    {
      case 0:
      {
        if (!std::holds_alternative<double>(other.val))
        {
          return fail("Cannot mod num by anything other than num");
        }

        return value{fmod(get<double>(val), get<double>(other.val))};
      }
      default: return
          fail("Cannot do " + type_name() + " % " + other.type_name());
    }
  }

  bool value::truthy()
  {
    throw std::runtime_error("TODO: not implemented yet");
  }

  interpret_result value::invert()
  {
    throw std::runtime_error("TODO: not implemented yet");
  }

  interpret_result value::eq(const value& other)
  {
    throw std::runtime_error("TODO: not implemented yet");
  }

  interpret_result value::neq(const value& other)
  {
    throw std::runtime_error("TODO: not implemented yet");
  }

  std::string repeat(const std::string& input, size_t num)
  {
    std::string ret;
    ret.reserve(input.size() * num);
    while (num--)
      ret += input;
    return ret;
  }
}