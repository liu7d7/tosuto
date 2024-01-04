#include <iostream>
#include <chrono>
#include "src/ami.h"
#include "src/treewalk/lex.h"
#include "src/treewalk/parse.h"
#include "src/treewalk/interpret.h"
#include "src/vm/vm.h"
#include "src/vm/scan.h"

void test_file(std::string const& filename) {
  std::cout
    << "testing "
    << filename
    << "\n-\n";
  ami::tree::lexer lexer{filename};
  auto toks = lexer.lex();

  ami::tree::parser parser{toks};
  auto ast = parser.global();
  if (!ast.has_value()) {
    std::cerr << ast.error() << "\n\n\n";
    return;
  }
  std::ofstream("out.txt") << (*ast)->pretty(0);

  ami::tree::symbol_table globals
    {
      {
        {"log", std::make_shared<ami::tree::value>(
          std::make_pair([](ami::tree::symbol_table& sym) -> ami::tree::interpret_result {
            auto res = ami_unwrap(sym.get("x"));
            auto display = ami_unwrap(res->display(sym));
            std::cout << display << "\n";
            return ami::tree::value::sym_nil;
          }, std::vector<std::pair<std::string, bool>>{
            {"x", true}}))},

        {"to_str", std::make_shared<ami::tree::value>(
          std::make_pair([](ami::tree::symbol_table& sym) -> ami::tree::interpret_result {
            auto res = ami_unwrap(sym.get("x"));
            auto display = ami_unwrap(res->display(sym));
            return std::make_shared<ami::tree::value>(display);
          }, std::vector<std::pair<std::string, bool>>{
            {"x", true}}))},

        {"has_deco", std::make_shared<ami::tree::value>(
          std::make_pair([](ami::tree::symbol_table& sym) -> ami::tree::interpret_result {
            auto obj = ami_unwrap(sym.get("obj"));
            auto name = ami_unwrap(sym.get("name"));
            if (!name->is<std::string>()) return ami::tree::interpreter::fail("Expected string, got " + name->type_name());
            return obj->has_deco(name->get<std::string>());
          }, std::vector<std::pair<std::string, bool>>{
            {"obj", true}, {"name", true}}))},

        {"get_deco", std::make_shared<ami::tree::value>(
          std::make_pair([](ami::tree::symbol_table& sym) -> ami::tree::interpret_result {
            auto obj = ami_unwrap(sym.get("obj"));
            auto name = ami_unwrap(sym.get("name"));
            if (!name->is<std::string>()) return ami::tree::interpreter::fail("Expected string, got " + name->type_name());
            return obj->get_deco(name->get<std::string>());
          }, std::vector<std::pair<std::string, bool>>{
            {"obj", true}, {"name", true}}))},

        {"false", ami::tree::value::sym_false},
        {"true", ami::tree::value::sym_true},

        {"time", std::make_shared<ami::tree::value>(
          std::make_pair([](ami::tree::symbol_table& sym) -> ami::tree::interpret_result {
            using namespace std::chrono;
            int64_t ms = duration_cast<milliseconds>(
              system_clock::now().time_since_epoch()
            ).count();
            return std::make_shared<ami::tree::value>(double(ms));
          }, std::vector<std::pair<std::string, bool>>{}))}
      },
    };

  ami::tree::interpreter interp;
  auto thing = interp.global(ast->get(), globals);
  if (!thing.has_value()) {
    std::cerr << thing.error() << "\n\n\n";
    return;
  }

  auto main_try = thing.value().find_first(
    [](std::string const& id, ami::tree::value_ptr const& val) {
      return val->has_deco("entrypoint").value()->get<bool>();
    });

  if (!main_try.has_value()) {
    std::cerr << main_try.error() << "\n\n\n";
    return;
  }

  auto call_try = main_try.value()->call(
    {std::make_shared<ami::tree::value>(filename)}, thing.value());
  if (!call_try.has_value()) {
    std::cerr << call_try.error() << "\n\n\n";
    return;
  }

  std::cout << "\n\n";
}

int main() {
  test_file("tests/diacritics.ami");
  test_file("tests/half-width-katakana.ami");
  return 0;
}
