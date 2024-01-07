#include <unordered_set>
#include <sstream>
#include <utility>
#include <memory>
#include "parse.h"

namespace tosuto {
  parser::parser(std::vector<token> toks) :
    toks(std::move(toks)),
    idx(0) {
    tok = this->toks[0];
    next = this->toks[1];
  }

  void parser::advance() {
    idx++;
    tok = toks[idx];
    next = toks[std::min(toks.size() - 1, idx + 1)];
  }

  std::expected<std::unique_ptr<node>, std::string> parser::block() {
    pos begin = tok.begin;
    tosuto_discard(expect(tok_type::l_curly));

    std::vector<std::unique_ptr<node>> exprs;
    while (tok.type != tok_type::r_curly) {
      auto exp = statement();
      if (!exp.has_value()) return exp;
      exprs.push_back(std::move(exp.value()));
    }

    advance();
    return std::make_unique<block_node>(std::move(exprs), begin, tok.begin);
  }

  std::expected<std::unique_ptr<node>, std::string> parser::global() {
    pos begin = tok.begin;
    std::vector<std::unique_ptr<node>> exprs;
    while (tok.type != tok_type::eof) {
      auto exp = statement();
      if (!exp.has_value()) return exp;
      exprs.push_back(std::move(exp.value()));
    }

    return std::make_unique<block_node>(std::move(exprs), begin, tok.begin);
  }

  std::expected<std::unique_ptr<node>, std::string> parser::for_loop() {
    pos begin = tok.begin;
    tosuto_discard(expect(tok_type::key_for));
    auto id = tosuto_unwrap(expect(tok_type::id));
    tosuto_discard(expect(tok_type::colon));
    auto iterable = tosuto_unwrap_move(expr());
    auto body = tosuto_unwrap_move(block());

    return std::make_unique<for_node>(id.lexeme, std::move(iterable), std::move(body), begin, body->end);
  }

  std::expected<std::vector<std::unique_ptr<node>>, std::string> parser::decos() {
    std::vector<std::unique_ptr<node>> decos;
    while (tok.type == tok_type::at) {
      pos begin = tok.begin;
      advance();
      token id = tosuto_unwrap(expect(tok_type::id));
      std::vector<std::pair<std::string, std::unique_ptr<node>>> fields;
      if (tok.type == tok_type::l_paren) {
        advance();
        while (tok.type == tok_type::id) {
          token field_id = tosuto_unwrap(expect(tok_type::id));
          tosuto_discard(expect(tok_type::assign));
          auto field_val = tosuto_unwrap_move(expr());
          consume(tok_type::comma);
          fields.emplace_back(field_id.lexeme, std::move(field_val));
        }

        tosuto_discard(expect(tok_type::r_paren));
      }

      decos.push_back(std::make_unique<deco_node>(id.lexeme, std::move(fields), begin, tok.begin));
    }

    return decos;
  }

  std::expected<std::unique_ptr<node>, std::string> parser::statement() {
    auto decor = tosuto_unwrap_move(decos());
    if (tok.type == tok_type::id &&
        (next.type == tok_type::colon || next.type == tok_type::l_curly)) {
      if (decor.empty()) {
        return function();
      } else {
        auto fn = tosuto_unwrap_move(function());
        return std::make_unique<decorated_node>(std::move(decor), std::move(fn), decor.front()->begin, tok.begin);
      }
    }

    switch (tok.type) {
      case tok_type::key_for: {
        return for_loop();
      }
      case tok_type::key_ret: {
        token cur = tok;
        advance();
        std::unique_ptr<node> ret_val = nullptr;
        auto exp = expr();
        if (exp.has_value()) ret_val = std::move(exp.value());

        return std::make_unique<ret_node>(std::move(ret_val), cur.begin,
                            ret_val ? ret_val->end : cur.end);
      }
      case tok_type::key_next: {
        token cur = tok;
        advance();
        return std::make_unique<next_node>(cur.begin, cur.end);
      }
      case tok_type::key_break: {
        token cur = tok;
        advance();
        return std::make_unique<break_node>(cur.begin, cur.end);
      }
      default: {
        if (decor.empty()) {
          return expr();
        } else {
          auto fn = tosuto_unwrap_move(expr());
          return std::make_unique<decorated_node>(std::move(decor), std::move(fn), decor.front()->begin, tok.begin);
        }
      }
    }
  }

  std::expected<std::unique_ptr<node>, std::string> parser::expr() {
    return define();
  }

  std::expected<std::unique_ptr<node>, std::string> parser::define() {
    if (tok.type != tok_type::id) {
      return assign();
    }

    std::string id = tok.lexeme;
    auto lhs = tosuto_unwrap_move(assign());
    while (tok.type == tok_type::walrus) {
      advance();
      auto rhs = tosuto_unwrap_move(assign());
      lhs = std::make_unique<var_def_node>(id, std::move(rhs), lhs->begin, rhs->end);
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

  std::expected<std::unique_ptr<node>, std::string> parser::assign() {
    auto lhs = tosuto_unwrap_move(sym_or());
    while (assign_ops.contains(tok.type)) {
      tok_type type = tok.type;
      advance();
      auto rhs = tosuto_unwrap_move(sym_or());
      lhs = std::make_unique<bin_op_node>(std::move(lhs), std::move(rhs), type, lhs->begin, rhs->end);
    }

    return lhs;
  }

  std::expected<std::unique_ptr<node>, std::string> parser::sym_or() {
    auto lhs = tosuto_unwrap_move(sym_and());
    while (tok.type == tok_type::sym_or) {
      advance();
      auto rhs = tosuto_unwrap_move(sym_and());
      lhs = std::make_unique<bin_op_node>(std::move(lhs), std::move(rhs), tok_type::sym_or, lhs->begin, rhs->end);
    }

    return lhs;
  }

  std::expected<std::unique_ptr<node>, std::string> parser::sym_and() {
    auto lhs = tosuto_unwrap_move(comp());
    while (tok.type == tok_type::sym_and) {
      advance();
      auto rhs = tosuto_unwrap_move(comp());
      lhs = std::make_unique<bin_op_node>(std::move(lhs), std::move(rhs), tok_type::sym_and, lhs->begin, rhs->end);
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

  std::expected<std::unique_ptr<node>, std::string> parser::comp() {
    auto lhs = tosuto_unwrap_move(add());
    while (comp_ops.contains(tok.type)) {
      tok_type type = tok.type;
      advance();
      auto rhs = tosuto_unwrap_move(add());
      lhs = std::make_unique<bin_op_node>(std::move(lhs), std::move(rhs), type, lhs->begin, rhs->end);
    }

    return lhs;
  }

  static std::unordered_set<tok_type> add_ops
    {
      tok_type::add,
      tok_type::sub
    };

  std::expected<std::unique_ptr<node>, std::string> parser::add() {
    auto lhs = tosuto_unwrap_move(mul());
    while (add_ops.contains(tok.type)) {
      tok_type type = tok.type;
      advance();
      auto rhs = tosuto_unwrap_move(mul());
      lhs = std::make_unique<bin_op_node>(std::move(lhs), std::move(rhs), type, lhs->begin, rhs->end);
    }

    return lhs;
  }

  static std::unordered_set<tok_type> mul_ops
    {
      tok_type::mul,
      tok_type::div,
      tok_type::mod
    };

  std::expected<std::unique_ptr<node>, std::string> parser::mul() {
    auto lhs = tosuto_unwrap_move(range());
    while (mul_ops.contains(tok.type)) {
      tok_type type = tok.type;
      advance();
      auto s = save_state();
      auto thing = range();
      if (!thing.has_value()) {
        set_state(s);
        if (lhs->type == node_type::bin_op) {
          auto bin_op = dynamic_uptr_cast<bin_op_node>(std::move(lhs));
          bin_op->rhs = std::make_unique<un_op_node>(std::move(bin_op->rhs), tok_type::mul,
                                       bin_op->rhs->begin, tok.begin);
          lhs = std::move(bin_op);
        } else {
          lhs = std::make_unique<un_op_node>(std::move(lhs), tok_type::mul, lhs->begin, tok.begin);
        }
      } else {
        auto rhs = tosuto_unwrap_move(thing);
        lhs = std::make_unique<bin_op_node>(std::move(lhs), std::move(rhs), type, lhs->begin, rhs->end);
      }
    }

    return lhs;
  }

  std::expected<std::unique_ptr<node>, std::string> parser::range() {
    std::unique_ptr<node> lhs = tosuto_unwrap_move(with());
    if (tok.type == tok_type::range) {
      advance();
      std::unique_ptr<node> rhs = tosuto_unwrap_move(with());
      lhs = std::make_unique<range_node>(std::move(lhs), std::move(rhs), lhs->begin, rhs->end);
    }

    return lhs;
  }

  std::expected<std::unique_ptr<node>, std::string> parser::with() {
    std::unique_ptr<node> lhs = tosuto_unwrap_move(pre_unary());
    if (tok.type == tok_type::key_with) {
      advance();
      std::unique_ptr<node> rhs = tosuto_unwrap_move(pre_unary());
      if (rhs->type != node_type::object)
        return std::unexpected{"Expected object on rhs of with expr!"};

      lhs = std::make_unique<bin_op_node>(std::move(lhs), std::move(rhs), tok_type::key_with, lhs->begin, rhs->end);
    }

    return lhs;
  }

  static std::unordered_set<tok_type> pre_unary_ops
    {
      tok_type::exclaim,
      tok_type::add,
      tok_type::sub
    };

  std::expected<std::unique_ptr<node>, std::string> parser::pre_unary() {
    pos begin = tok.begin;
    if (pre_unary_ops.contains(tok.type)) {
      tok_type type = tok.type;
      advance();
      auto body = tosuto_unwrap_move(post_unary());
      return std::make_unique<un_op_node>(std::move(body), type, begin, body->end);
    }

    return post_unary();
  }

  static std::unordered_set<tok_type> post_unary_ops
    {
      tok_type::inc,
      tok_type::dec,
    };

  std::expected<std::unique_ptr<node>, std::string> parser::post_unary() {
    pos begin = tok.begin;
    auto body = tosuto_unwrap_move(call());
    if (post_unary_ops.contains(tok.type)) {
      tok_type type = tok.type;
      advance();
      return std::make_unique<un_op_node>(std::move(body), type, begin, body->end);
    }

    return body;
  }

  std::expected<std::unique_ptr<node>, std::string> parser::call() {
    pos begin = tok.begin;
    std::unique_ptr<node> body = tosuto_unwrap_move(field_get());
    while (tok.type == tok_type::l_paren) {
      advance();
      std::vector<std::unique_ptr<node>> args;
      while (tok.type != tok_type::r_paren) {
        auto exp = tosuto_unwrap_move(expr());
        args.push_back(std::move(exp));
        if (tok.type == tok_type::comma) {
          advance();
        }
      }

      advance();
      return std::make_unique<call_node>(
        std::move(body), std::move(args), false, begin, args.empty() ? body->end : args.back()->end);
    }

    return body;
  }

  std::expected<std::unique_ptr<node>, std::string> parser::field_get() {
    pos begin = tok.begin;
    std::unique_ptr<node> body = tosuto_unwrap_move(atom());
    while (tok.type == tok_type::dot) {
      advance();
      if (tok.type == tok_type::l_paren) {
        advance();
        std::vector<std::unique_ptr<node>> args;
        while (tok.type != tok_type::r_paren) {
          auto exp = tosuto_unwrap_move(expr());
          args.push_back(std::move(exp));
          if (tok.type == tok_type::comma) {
            advance();
          }
        }

        advance();

        body = std::make_unique<call_node>(std::move(body), std::move(args), true, begin, tok.begin);
        continue;
      }

      token id = tosuto_unwrap(expect(tok_type::id));

      body = std::make_unique<field_get_node>(std::move(body), id.lexeme, begin, id.end);
    }

    return body;
  }

  std::expected<std::unique_ptr<node>, std::string> parser::atom() {
    pos begin = tok.begin;
    switch (tok.type) {
      case tok_type::l_paren: {
        advance();
        auto exp = tosuto_unwrap_move(expr());
        tosuto_discard(expect(tok_type::r_paren));
        return exp;
      }
      case tok_type::number: {
        double value = std::stod(tok.lexeme);
        advance();
        return std::make_unique<number_node>(value, tok.begin, tok.end);;
      }
      case tok_type::string: {
        auto nod = std::make_unique<string_node>(tok.lexeme, tok.begin, tok.end);
        advance();
        return nod;
      }
      case tok_type::key_if: {
        std::vector<std::pair<std::unique_ptr<node>, std::unique_ptr<node>>> cases;
        advance();
        auto exp = tosuto_unwrap_move(expr());
        auto body = tosuto_unwrap_move(block());
        cases.emplace_back(std::move(exp), std::move(body));
        while (tok.type == tok_type::key_elif) {
          advance();
          exp = tosuto_unwrap_move(expr());
          body = tosuto_unwrap_move(block());
          cases.emplace_back(std::move(exp), std::move(body));
        }

        std::unique_ptr<node> other = nullptr;
        if (tok.type == tok_type::key_else) {
          advance();
          other = tosuto_unwrap_move(block());
        }

        return std::make_unique<if_node>(std::move(cases), std::move(other), begin, tok.begin);
      }
      case tok_type::colon: {
        return function();
      }
      case tok_type::id: {
        auto nod = std::make_unique<field_get_node>(nullptr, tok.lexeme, begin, tok.begin);
        advance();
        return nod;
      }
      case tok_type::l_object: {
        advance();
        std::vector<std::pair<std::string, std::unique_ptr<node>>> fields;
        while (tok.type == tok_type::id) {
          auto id = tok.lexeme;
          advance();

          if (tok.type == tok_type::colon) {
            auto body = tosuto_unwrap_move(function());
            fields.emplace_back(id, std::move(body));
          } else {
            tosuto_discard(expect(tok_type::assign));
            auto body = tosuto_unwrap_move(expr());
            fields.emplace_back(id, std::move(body));
          }

          consume(tok_type::comma);
        }

        tosuto_discard(expect(tok_type::r_object));
        return std::make_unique<object_node>(std::move(fields), begin, tok.begin);
      }
      case tok_type::key_false: {
        advance();
        return std::make_unique<kw_literal_node>(tok_type::key_false, begin, tok.begin);
      }
      case tok_type::key_true: {
        advance();
        return std::make_unique<kw_literal_node>(tok_type::key_true, begin, tok.begin);
      }
      case tok_type::key_nil: {
        advance();
        return std::make_unique<kw_literal_node>(tok_type::key_nil, begin, tok.begin);
      }
      default:
        return std::unexpected(
          "Expected atom, got " + tok.to_string() + " in " +
          __PRETTY_FUNCTION__);
    }
  }

  std::expected<token, std::string>
  parser::expect(tok_type type, bool do_advance, std::source_location loc) {
    if (tok.type != type)
      return std::unexpected(
        "Expected " + to_string(type) + ", got " + tok.to_string() + " at " +
        loc.function_name());

    token cur = tok;
    if (do_advance) advance();
    return cur;
  }

  void parser::consume(tok_type type) {
    if (tok.type == type) advance();
  }

  std::expected<std::unique_ptr<node>, std::string> parser::function() {
    using namespace std::string_literals;
    pos begin = tok.begin;
    std::string id =
      tok.type == tok_type::id ? expect(tok_type::id)->lexeme : ""s;

    std::vector<std::pair<std::string, bool>> args;
    if (tok.type == tok_type::colon) {
      advance();
      while (tok.type == tok_type::id) {
        std::pair<std::string, bool> arg{tok.lexeme, false};
        advance();
        if (tok.type == tok_type::mul) {
          arg.second = true;
          advance();
        }
        args.push_back(arg);
      }
    }

    if (tok.type == tok_type::r_arrow) {
      advance();
      auto body = tosuto_unwrap_move(expr());
      return std::make_unique<fn_def_node>(id, args, std::move(body), begin, body->end);
    }

    tosuto_discard(expect(tok_type::l_curly, false));
    auto body = tosuto_unwrap_move(block());
    return std::make_unique<fn_def_node>(id, args, std::move(body), begin, body->end);
  }

  void parser::set_state(parser::state const& s) {
    idx = s.idx, tok = s.tok, next = s.tok;
  }

  parser::state parser::save_state() {
    return state{idx, tok, next};
  }

  node::node(node_type type, pos begin, pos end) :
    type(type),
    begin(begin),
    end(end) {}

  fn_def_node::fn_def_node(std::string name,
                           std::vector<std::pair<std::string, bool>> args,
                           std::unique_ptr<node> body, pos begin, pos end) :
    name(std::move(name)),
    args(std::move(args)),
    body(std::move(body)),
    node(node_type::fn_def, begin, end) {
    if (this->name.empty()) {
      this->type = node_type::anon_fn_def;
    }
  }

  std::string fn_def_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::string arg_str;
    for (auto const& it: args) {
      arg_str += it.first;
      if (it.second) arg_str += "*";
      arg_str += ", ";
    }

    if (!arg_str.empty()) {
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

  block_node::block_node(std::vector<std::unique_ptr<node>> exprs, pos begin, pos end) :
    exprs(std::move(exprs)),
    node(node_type::block, begin, end) {}

  std::string block_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::stringstream ss;
    ss << "block: [\n";
    for (auto const& it: exprs) {
      ss << ind << it->pretty(indent + 1) << ",\n";
    }

    ss << std::string(indent * 2, ' ') << ']';
    return ss.str();
  }

  call_node::call_node(std::unique_ptr<node> callee, std::vector<std::unique_ptr<node>> args, bool is_member,
                       pos begin,
                       pos end) :
    callee(std::move(callee)),
    args(std::move(args)),
    is_member(is_member),
    node(node_type::call, begin, end) {}

  std::string call_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::string arg_str;
    for (auto const& it: args) {
      arg_str += ind;
      arg_str += "  ";
      arg_str += it->pretty(indent + 2);
      arg_str += ",\n";
    }

    if (!arg_str.empty()) {
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

  un_op_node::un_op_node(std::unique_ptr<node> target, tok_type op, pos begin, pos end) :
    target(std::move(target)),
    op(op),
    node(node_type::un_op, begin, end) {}

  std::string un_op_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "un_op_node: {\n"
        << ind << "op: " << to_string(op) << ",\n"
        << ind << "target: " << target->pretty(indent + 1) << '\n'
        << std::string(indent * 2, ' ') << '}').str();
  }

  bin_op_node::bin_op_node(std::unique_ptr<node> lhs, std::unique_ptr<node> rhs, tok_type op, pos begin,
                           pos end) :
    lhs(std::move(lhs)),
    rhs(std::move(rhs)),
    op(op),
    node(node_type::bin_op, begin, end) {}

  std::string bin_op_node::pretty(int indent) const {
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
    node(node_type::number, begin, end) {}

  std::string number_node::pretty(int indent) const {
    return "number_node: " + std::to_string(value);
  }

  string_node::string_node(std::string value, pos begin, pos end) :
    value(std::move(value)),
    node(node_type::string, begin, end) {}

  std::string string_node::pretty(int indent) const {
    return "string_node: \"" + value + "\"";
  }

  object_node::object_node(decltype(fields) fields, pos begin, pos end) :
    fields(std::move(fields)),
    node(node_type::object, begin, end) {}

  std::string object_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');

    auto ss = std::stringstream() << "object_node: {\n";
    for (auto const& it: fields) {
      ss << ind << it.first << ": " << it.second->pretty(indent + 1) << '\n';
    }

    ss << std::string(indent * 2, ' ') << '}';

    return ss.str();
  }

  field_get_node::field_get_node(std::unique_ptr<node> target, std::string field, pos begin,
                                 pos end) :
    target(std::move(target)),
    field(std::move(field)),
    node(node_type::field_get, begin, end) {}

  std::string field_get_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "field_get_node: {\n"
        << ind << "target: " << (target ? target->pretty(indent + 1) : "nil")
        << ",\n"
        << ind << "field: " << field << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
  }

  var_def_node::var_def_node(std::string name, std::unique_ptr<node> value, pos begin, pos end)
    :
    name(std::move(name)),
    value(std::move(value)),
    node(node_type::var_def, begin, end) {}

  std::string var_def_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "var_def_node: {\n"
        << ind << "name: " << name << ",\n"
        << ind << "value: " << value->pretty(indent + 1) << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
  }

  if_node::if_node(decltype(cases) cases, std::unique_ptr<node> else_case, pos begin, pos end) :
    cases(std::move(cases)),
    else_case(std::move(else_case)),
    node(node_type::if_stmt, begin, end) {}

  std::string if_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');

    std::string cases_str;
    for (auto const& it: cases) {
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

    if (else_case) {
      ss << ind << "other: " << else_case->pretty(indent + 1) << '\n';
    }

    ss << std::string(indent * 2, ' ') << '}';

    return ss.str();
  }

  ret_node::ret_node(std::unique_ptr<node> ret_val, pos begin, pos end) :
    ret_val(std::move(ret_val)),
    node(node_type::ret, begin, end) {}

  std::string ret_node::pretty(int indent) const {
    return ret_val ? "ret: " + ret_val->pretty(indent + 1) : "ret";
  }

  next_node::next_node(pos begin, pos end) : node(node_type::next, begin,
                                                  end) {}

  std::string next_node::pretty(int indent) const {
    return "next";
  }

  break_node::break_node(pos begin, pos end) : node(node_type::brk, begin,
                                                    end) {}

  std::string break_node::pretty(int indent) const {
    return "break";
  }

  range_node::range_node(std::unique_ptr<node> start, std::unique_ptr<node> finish, pos begin, pos end) :
    start(std::move(start)),
    finish(std::move(finish)),
    node(node_type::range, begin, end) {}

  std::string range_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "range_node: {\n"
        << ind << "start: " << start->pretty(indent + 1) << ",\n"
        << ind << "finish: " << finish->pretty(indent + 1) << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
  }

  std::string for_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');

    return (
      std::stringstream()
        << "for_node: {\n"
        << ind << "id: " << id << ",\n"
        << ind << "iterable: " << iterable->pretty(indent + 1) << ",\n"
        << ind << "body: " << body->pretty(indent + 1) << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
  }

  for_node::for_node(std::string id, std::unique_ptr<node> iterable,
                     std::unique_ptr<node> body, pos begin, pos end) :
    id(std::move(id)),
    iterable(std::move(iterable)),
    body(std::move(body)),
    node(node_type::for_loop, begin, end) {}

  anon_fn_def_node::anon_fn_def_node(
    std::vector<std::pair<std::string, bool>> args, std::unique_ptr<node> body, pos begin,
    pos end) :
    args(std::move(args)), body(std::move(body)),
    node(node_type::anon_fn_def, begin, end) {}

  std::string anon_fn_def_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::string arg_str;
    for (auto const& it: args) {
      arg_str += it.first;
      if (it.second) arg_str += "*";
      arg_str += ", ";
    }

    if (!arg_str.empty()) {
      arg_str = arg_str.substr(0, arg_str.length() - 2);
    }

    return (
      std::stringstream()
        << "anon_fn_def_node: {\n"
        << ind << "args: [" << arg_str << ']' << ",\n"
        << ind << "body: " << body->pretty(indent + 1) << '\n'
        << std::string(indent * 2, ' ') << '}').str();
  }

  deco_node::deco_node(std::string id,
                       std::vector<std::pair<std::string, std::unique_ptr<node>>> nodes,
                       pos begin, pos end) : id(std::move(id)),
                                             fields(std::move(nodes)),
                                             node(node_type::deco, begin,
                                                  end) {}

  std::string deco_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::string arg_str;
    for (auto const& it: fields) {
      arg_str += ind;
      arg_str += "  ";
      arg_str += it.first;
      arg_str += "=";
      arg_str += it.second->pretty(indent + 2);
      arg_str += ",\n";
    }

    if (!arg_str.empty()) {
      arg_str = arg_str.substr(0, arg_str.length() - 2);
      arg_str += '\n';
    }

    return (
      std::stringstream()
        << "deco: {\n"
        << ind << "id: " << id << '\n'
        << ind << "fields: [" << arg_str << ']' << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
  }

  decorated_node::decorated_node(std::vector<std::unique_ptr<node>> decos, std::unique_ptr<node> target,
                                 pos begin, pos end) : decos(std::move(decos)),
                                                       target(std::move(target)),
                                                       node(node_type::decorated, begin,
                                                            end) {}

  std::string decorated_node::pretty(int indent) const {
    std::string ind = std::string((indent + 1) * 2, ' ');
    std::string arg_str;
    for (auto const& it: decos) {
      arg_str += ind;
      arg_str += "  ";
      arg_str += it->pretty(indent + 2);
      arg_str += ",\n";
    }

    if (!arg_str.empty()) {
      arg_str = arg_str.substr(0, arg_str.length() - 2);
      arg_str += '\n';
    }

    return (
      std::stringstream()
        << "decorated: {\n"
        << ind << "target: " << target->pretty(indent + 1) << '\n'
        << ind << "decos: [" << arg_str << ']' << ",\n"
        << std::string(indent * 2, ' ') << '}').str();
  }

  kw_literal_node::kw_literal_node(tok_type lit, pos begin, pos end) :
    lit(lit),
    node(node_type::kw_literal, begin, end) {

  }

  std::string kw_literal_node::pretty(int indent) const {
    return "kw_literal_node: " + to_string(lit);
  }
}