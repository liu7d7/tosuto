#include <iostream>
#include "src/ami.h"
#include "src/lex.h"
#include "src/parse.h"
#include "src/interpret.h"

int main() {
  ami::lexer lexer{"test.ami"};
  auto toks = lexer.lex();

  ami::parser parser{toks};
  auto ast = parser.global();
  if (!ast.has_value()) throw std::runtime_error(ast.error());
  std::ofstream("out.txt") << (*ast)->pretty(0);

  ami::symbol_table globals
    {
      {
        {"log", std::make_shared<ami::value>(
          std::make_pair([](ami::symbol_table& sym) -> ami::interpret_result {
            auto res = ami_unwrap(sym.get("x"));
            auto display = ami_unwrap(res->display(sym));
            std::cout << display << "\n";
            return ami::value::sym_nil;
          }, std::vector<std::pair<std::string, bool>>{
            {"x", true}}))},

        {"to_str", std::make_shared<ami::value>(
          std::make_pair([](ami::symbol_table& sym) -> ami::interpret_result {
            auto res = ami_unwrap(sym.get("x"));
            auto display = ami_unwrap(res->display(sym));
            return std::make_shared<ami::value>(display);
          }, std::vector<std::pair<std::string, bool>>{
            {"x", true}}))},

        {"has_deco", std::make_shared<ami::value>(
          std::make_pair([](ami::symbol_table& sym) -> ami::interpret_result {
            auto obj = ami_unwrap(sym.get("obj"));
            auto name = ami_unwrap(sym.get("name"));
            if (!name->is<std::string>()) return ami::interpreter::fail("Expected string, got " + name->type_name());
            return obj->has_deco(name->get<std::string>());
          }, std::vector<std::pair<std::string, bool>>{
            {"obj", true}, {"name", true}}))},

        {"get_deco", std::make_shared<ami::value>(
            std::make_pair([](ami::symbol_table& sym) -> ami::interpret_result {
              auto obj = ami_unwrap(sym.get("obj"));
              auto name = ami_unwrap(sym.get("name"));
              if (!name->is<std::string>()) return ami::interpreter::fail("Expected string, got " + name->type_name());
              return obj->get_deco(name->get<std::string>());
            }, std::vector<std::pair<std::string, bool>>{
              {"obj", true}, {"name", true}}))},

        {"false", ami::value::sym_false},
        {"true", ami::value::sym_true},
      },
    };

  ami::interpreter interp;
  auto thing = interp.global(*ast, globals);
  if (!thing.has_value()) throw std::runtime_error(thing.error());

  auto main_try = thing.value().find_first(
    [](std::string const& id, ami::value_ptr const& val) {
      return val->has_deco("entrypoint").value()->get<bool>();
    });

  if (!main_try.has_value()) throw std::runtime_error(main_try.error());

  auto call_try = main_try.value()->call(
    {std::make_shared<ami::value>("test.ami")}, thing.value());
  if (!call_try.has_value()) throw std::runtime_error(call_try.error());
  return 0;
}
