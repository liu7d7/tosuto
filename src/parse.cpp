#include <unordered_set>
#include <sstream>
#include <utility>
#include "parse.h"

namespace ami
{
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
      tok_type::eq,
      tok_type::neq,
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
    node* lhs = ami_unwrap(range());
    while (mul_ops.contains(tok.type))
    {
      tok_type type = tok.type;
      advance();
      auto s = save_state();
      auto thing = range();
      if (!thing.has_value())
      {
        set_state(s);
        if (lhs->type == node_type::bin_op)
        {
          auto bin_op = dynamic_cast<bin_op_node*>(lhs);
          bin_op->rhs = new un_op_node(bin_op->rhs, tok_type::mul,
                                       bin_op->rhs->begin, tok.begin);
        }
        else
        {
          lhs = new un_op_node(lhs, tok_type::mul, lhs->begin, tok.begin);
        }
      }
      else
      {
        node* rhs = ami_unwrap(thing);
        lhs = new bin_op_node(lhs, rhs, type, lhs->begin, rhs->end);
      }
    }

    return lhs;
  }

  std::expected<node*, std::string> parser::range()
  {
    node* lhs = ami_unwrap(with());
    if (tok.type == tok_type::range)
    {
      advance();
      node* rhs = ami_unwrap(with());
      lhs = new range_node(lhs, rhs, lhs->begin, rhs->end);
    }

    return lhs;
  }

  std::expected<node*, std::string> parser::with()
  {
    node* lhs = ami_unwrap(pre_unary());
    if (tok.type == tok_type::key_with)
    {
      advance();
      node* rhs = ami_unwrap(pre_unary());
      if (rhs->type != node_type::object)
        return std::unexpected{"Expected object on rhs of with expr!"};

      lhs = new bin_op_node(lhs, rhs, tok_type::key_with, lhs->begin, rhs->end);
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
      tok_type::dec,
      tok_type::at,
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
        body, args, false, begin, args.empty() ? body->end : args.back()->end);
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
      if (tok.type == tok_type::l_paren)
      {
        // this is a member function call
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

        body = new call_node(body, args, true, begin, tok.begin);
        continue;
      }

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
      case tok_type::colon:
      {
        return function();
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

          if (tok.type == tok_type::colon)
          {
            node* body = ami_unwrap(function());
            fields.emplace_back(id, body);
          }
          else
          {
            ami_discard(expect(tok_type::assign));
            node* body = ami_unwrap(expr());
            fields.emplace_back(id, body);
          }

          consume(tok_type::comma);
        }

        ami_discard(expect(tok_type::r_object));
        return new object_node(fields, begin, tok.begin);
      }
      default:
        return std::unexpected(
          "Expected atom, got " + tok.to_string() + " in " +
          __PRETTY_FUNCTION__);
    }
  }

  std::expected<token, std::string>
  parser::expect(tok_type type, bool do_advance, std::source_location loc)
  {
    if (tok.type != type)
      return std::unexpected(
        "Expected " + to_string(type) + ", got " + tok.to_string() + " at " +
        loc.function_name());

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
    using namespace std::string_literals;
    pos begin = tok.begin;
    std::string id = tok.type == tok_type::id ? expect(tok_type::id)->lexeme : ""s;

    std::vector<std::pair<std::string, bool>> args;
    if (tok.type == tok_type::colon)
    {
      advance();
      while (tok.type == tok_type::id)
      {
        std::pair<std::string, bool> arg{tok.lexeme, false};
        advance();
        if (tok.type == tok_type::mul)
        {
          arg.second = true;
          advance();
        }
        args.push_back(arg);
      }
    }

    if (tok.type == tok_type::r_arrow)
    {
      advance();
      node* body = ami_unwrap(expr());
      return new fn_def_node(id, args, body, begin, body->end);
    }

    ami_discard(expect(tok_type::l_curly, false));
    node* body = ami_unwrap(block());
    return new fn_def_node(id, args, body, begin, body->end);
  }

  void parser::set_state(parser::state const& s)
  {
    idx = s.idx, tok = s.tok, next = s.tok;
  }

  parser::state parser::save_state()
  {
    return state{idx, tok, next};
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

  fn_def_node::fn_def_node(std::string name,
                           std::vector<std::pair<std::string, bool>> args,
                           node* body, pos begin, pos end) :
    name(std::move(name)),
    args(std::move(args)),
    body(body),
    node(node_type::fn_def, begin, end)
  {
    if (this->name.empty())
    {
      this->type = node_type::anon_fn_def;
    }
  }

  std::string fn_def_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::string arg_str;
    for (auto const& it: args)
    {
      arg_str += it.first;
      if (it.second) arg_str += "*";
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

  call_node::call_node(node* callee, std::vector<node*> args, bool is_member,
                       pos begin,
                       pos end) :
    callee(callee),
    args(std::move(args)),
    is_member(is_member),
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
        << ind << "is_member: " << std::to_string(is_member) << ",\n"
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
    for (auto const& it: fields)
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
        << ind << "target: " << (target ? target->pretty(indent + 1) : "nil")
        << ",\n"
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
    for (auto const& it: cases)
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
      << ind << "cases: [\n" << cases_str << '\n' << ind << ']'
      << (else_case ? ",\n" : "\n");

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

  range_node::range_node(node* start, node* finish, pos begin, pos end) :
    start(start),
    finish(finish),
    node(node_type::range, begin, end)
  {}

  std::string range_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "range_node: {\n"
        << ind << "start: " << start->pretty(indent + 1) << ",\n"
        << ind << "finish: " << finish->pretty(indent + 1) << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
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

  anon_fn_def_node::anon_fn_def_node(
    std::vector<std::pair<std::string, bool>> args, node* body, pos begin,
    pos end) :
    args(std::move(args)), body(body), node(node_type::anon_fn_def, begin, end)
  {}

  std::string anon_fn_def_node::pretty(int indent) const
  {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::string arg_str;
    for (auto const& it: args)
    {
      arg_str += it.first;
      if (it.second) arg_str += "*";
      arg_str += ", ";
    }

    if (!arg_str.empty())
    {
      arg_str = arg_str.substr(0, arg_str.length() - 2);
    }

    return (
      std::stringstream()
        << "fn_def_node: {\n"
        << ind << "args: [" << arg_str << ']' << ",\n"
        << ind << "body: " << body->pretty(indent + 1) << '\n'
        << std::string(indent * 2, ' ') << '}').str();
  }
}