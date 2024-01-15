#include <iostream>
#include <chrono>
#include "src/tosuto.h"
#include "src/lex.h"
#include "src/parse.h"
#include "src/treewalk/interpret.h"
#include "src/vm/vm.h"
#include "src/vm/compile.h"

void test_file(std::string const& filename) {
  std::cout
    << "testing "
    << filename
    << "\n-\n";
  tosuto::lexer lexer{filename};
  auto toks = lexer.lex();

  tosuto::parser parser{toks};
  auto ast = parser.global();
  if (!ast.has_value()) {
    std::cerr << ast.error() << "\n\n\n";
    return;
  }

  std::ofstream("out.txt") << (*ast)->pretty(0);
  tosuto::tree::symbol_table globals
    {
      {
        {"log", std::make_shared<tosuto::tree::value>(
          std::make_pair([](tosuto::tree::symbol_table& sym) -> tosuto::tree::interpret_result {
            auto res = tosuto_unwrap(sym.get("x"));
            auto display = tosuto_unwrap(res->display(sym));
            std::cout << display << "\n";
            return tosuto::tree::value::sym_nil;
          }, std::vector<std::pair<std::string, bool>>{{"x", true}}))},

        {"to_str", std::make_shared<tosuto::tree::value>(
          std::make_pair([](tosuto::tree::symbol_table& sym) -> tosuto::tree::interpret_result {
            auto res = tosuto_unwrap(sym.get("x"));
            auto display = tosuto_unwrap(res->display(sym));
            return std::make_shared<tosuto::tree::value>(display);
          }, std::vector<std::pair<std::string, bool>>{
            {"x", true}}))},

        {"has_deco", std::make_shared<tosuto::tree::value>(
          std::make_pair([](tosuto::tree::symbol_table& sym) -> tosuto::tree::interpret_result {
            auto obj = tosuto_unwrap(sym.get("obj"));
            auto name = tosuto_unwrap(sym.get("name"));
            if (!name->is<std::string>()) return tosuto::tree::interpreter::fail("Expected str, got " + name->type_name());
            return obj->has_deco(name->get<std::string>());
          }, std::vector<std::pair<std::string, bool>>{
            {"obj", true}, {"name", true}}))},

        {"get_deco", std::make_shared<tosuto::tree::value>(
          std::make_pair([](tosuto::tree::symbol_table& sym) -> tosuto::tree::interpret_result {
            auto obj = tosuto_unwrap(sym.get("obj"));
            auto name = tosuto_unwrap(sym.get("name"));
            if (!name->is<std::string>()) return tosuto::tree::interpreter::fail("Expected str, got " + name->type_name());
            return obj->get_deco(name->get<std::string>());
          }, std::vector<std::pair<std::string, bool>>{
            {"obj", true}, {"name", true}}))},

        {"time", std::make_shared<tosuto::tree::value>(
          std::make_pair([](tosuto::tree::symbol_table& sym) -> tosuto::tree::interpret_result {
            using namespace std::chrono;
            int64_t ms = duration_cast<milliseconds>(
              system_clock::now().time_since_epoch()
            ).count();
            return std::make_shared<tosuto::tree::value>(double(ms));
          }, std::vector<std::pair<std::string, bool>>{}))}
      },
    };

  tosuto::tree::interpreter interp;
  auto thing = interp.global(ast->get(), globals);
  if (!thing.has_value()) {
    std::cerr << thing.error() << "\n\n\n";
    return;
  }

  auto main_try = thing.value().find_first(
    [](std::string const& id, tosuto::tree::value_ptr const& val) {
      return val->has_deco("entrypoint").value()->get<bool>();
    });

  if (!main_try.has_value()) {
    std::cerr << main_try.error() << "\n\n\n";
    return;
  }

  auto call_try = main_try.value()->call(
    {std::make_shared<tosuto::tree::value>(filename)}, thing.value());
  if (!call_try.has_value()) {
    std::cerr << call_try.error() << "\n\n\n";
    return;
  }

  std::cout << "\n\n";
}

size_t cur_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(
    system_clock::now().time_since_epoch()
  ).count();
}

void test_vm() {
  size_t start, finish;
  start = cur_ms();
  auto lex = tosuto::lexer{"test.tosuto"};
  auto toks = lex.lex();
  finish = cur_ms();
  size_t lex_time = finish - start;

  start = cur_ms();
  auto parse = tosuto::parser{toks};
  auto ast = parse.global();
  finish = cur_ms();
  size_t parse_time = finish - start;
  if (!ast.has_value()) throw std::runtime_error(ast.error());
  std::ofstream("out.txt") << (*ast)->pretty(0);

  start = cur_ms();
  auto compile = tosuto::vm::compiler{tosuto::vm::value::fn::type::script};
  auto res = compile.global(ast->get());
  finish = cur_ms();
  size_t compile_time = finish - start;
  if (!res.has_value()) throw std::runtime_error(res.error());
  auto& fn = res.value();
  fn.ch->disasm(std::cout);
  std::cout << "\nvm: \n";

  using namespace tosuto;

  auto vm = vm::vm{fn};
  using nt_ret = std::expected<vm::value, std::string>;
  vm.def_native(
    "log",
    1,
    [](std::span<vm::value> args) {
      std::cout << args[0] << '\n';
      return nt_ret{vm::value::nil{}};
    });

  vm.def_native(
    "time",
    0,
    [](std::span<vm::value> args) {
      using namespace std::chrono;
      size_t ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
      ).count();
      return nt_ret{vm::value{double(ms)}};
    });

  vm.def_native(
    "to_str",
    1,
    [](std::span<vm::value> args) {
      return nt_ret{vm::value{vm::value::str{args[0].to_string()}}};
    });

  start = cur_ms();
  auto interp_res = vm.run(std::cout);
  finish = cur_ms();
  size_t run_time = finish - start;
  if (!interp_res.has_value()) throw std::runtime_error(interp_res.error());

  std::cout << "\n\n\n";

  std::cout << "lex took " << lex_time << "ms" << '\n';
  std::cout << "parse took " << parse_time << "ms" << '\n';
  std::cout << "compile took " << compile_time << "ms" << '\n';
  std::cout << "run took " << run_time << "ms" << '\n';
}

int main() {
  test_vm();
  return 0;
}
