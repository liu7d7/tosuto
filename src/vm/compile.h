#pragma once

#include <iostream>
#include "vm.h"
#include "scan.h"

namespace ami::vm {
  enum class op_prec {
    none,
    define, // :=
    assign, // =
    or_, // |
    and_, // &
    eq, // ==, !=
    comp, // <, >, <=, >=
    term, // +, -
    factor, // *, /, %
    unary, // !, -
    call, // ., ()
    primary
  };

  struct compiler {
    token cur, prev;
    scanner sc;
    chunk ch;

    explicit compiler(std::string const& source) : sc(source) {}

    using parse_fn = std::expected<void, std::string>(compiler::*)();

    struct parse_rule {
      parse_fn prefix;
      parse_fn infix;
      op_prec prec;
    };

    std::unordered_map<tok_type, parse_rule> rules {
      {tok_type::l_paren, {&compiler::grouping, nullptr, op_prec::none}},
      {tok_type::add, {&compiler::unary, &compiler::binary, op_prec::term}},
      {tok_type::sub, {&compiler::unary, &compiler::binary, op_prec::term}},
      {tok_type::mul, {&compiler::unary, &compiler::binary, op_prec::factor}},
      {tok_type::div, {nullptr, &compiler::binary, op_prec::factor}},
      {tok_type::mod, {nullptr, &compiler::binary, op_prec::factor}},
      {tok_type::number, {&compiler::number, nullptr, op_prec::none}},
    };

    static std::unexpected<std::string>
    fail(token const& tok, std::string const& message) {
      return std::unexpected{
        "On line "
        + std::to_string(tok.line)
        + " at "
        + tok.lexeme
        + ": "
        + message};
    }

    std::expected<void, std::string> advance() {
      prev = cur;

      for (;;) {
        cur = ami_unwrap(sc.next());
      }
    }

    std::expected<token, std::string> expect(tok_type type) {
      if (cur.type != type)
        return fail(cur, "Expected type " + to_string(type) + ", got " +
                         to_string(cur.type));

      ami_discard(advance());
      return prev;
    }

    std::expected<void, std::string> number() {
      try {
        double val = std::stod(prev.lexeme);
        auto lit = ch.add_lit(val);
        ch.add(op_code::lit);
        ch.add(lit);
        return {};
      } catch (std::invalid_argument const& ex) {
        return fail(prev, ex.what());
      }
    }

    std::expected<void, std::string> grouping() {
      ami_discard(expr());
      ami_discard(expect(tok_type::r_paren));
      return {};
    }

    std::expected<void, std::string> unary() {
      token op = prev;
      ami_discard(parse_precedence(op_prec::unary));

      switch (op.type) {
        case tok_type::sub: ch.add(op_code::neg);
          break;
        default:
          return fail(op, "Unexpected unary operator " + to_string(op.type));
      }

      return {};
    }

    std::expected<void, std::string> binary() {
      token op = prev;
      parse_rule rule = rules[op.type];
      ami_discard(parse_precedence(op_prec(std::to_underlying(rule.prec) + 1)));

      switch (op.type) {
        case tok_type::add: ch.add(op_code::add);
          break;
        case tok_type::sub: ch.add(op_code::sub);
          break;
        case tok_type::mul: ch.add(op_code::mul);
          break;
        case tok_type::div: ch.add(op_code::div);
          break;
        case tok_type::mod: ch.add(op_code::mod);
          break;
        default: return fail(op, "Unknown binary operator."); // Unreachable.
      }

      return {};
    }

    std::expected<void, std::string> parse_precedence(op_prec p) {
      ami_discard(advance());
      parse_fn prefix = rules[prev.type].prefix;
      if (!prefix) return fail(prev, "Expected expression.");

      (this->*prefix)();

      while (p < rules[cur.type].prec) {
        ami_discard(advance());
        parse_fn infix = rules[prev.type].infix;
        if (!infix) return fail(prev, "Infix rule not found.");
        (this->*infix)();
      }

      return {};
    }

    std::expected<void, std::string> expr() {
      ami_discard(parse_precedence(op_prec::assign));

      return {};
    }

    std::expected<chunk, std::string> compile() {
      ami_discard(advance());
      ami_discard(expr());
      ami_discard(expect(tok_type::eof));
      ch.add(op_code::ret);

      return ch;
    }
  };
}