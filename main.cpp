#include <iostream>
#include <chrono>
#include "src/ami.h"
#include "src/lex.h"
#include "src/parse.h"
#include "src/vm/vm.h"
#include "src/vm/compile.h"
#include "src/vm/serialize.h"

size_t cur_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(
    system_clock::now().time_since_epoch()
  ).count();
}

void test_vm() {
  size_t start, finish;
  start = cur_ms();
  auto lex = ami::lexer{"test.ami"};
  auto toks = lex.lex();
  finish = cur_ms();
  size_t lex_time = finish - start;

  start = cur_ms();
  auto parse = ami::parser{toks};
  auto ast = parse.global();
  finish = cur_ms();
  size_t parse_time = finish - start;
  if (!ast.has_value()) throw std::runtime_error(ast.error());
  std::ofstream("out.txt") << (*ast)->pretty(0);

  start = cur_ms();
  auto compile = ami::vm::compiler{ami::vm::value::fn::type::script};
  auto res = compile.global(ast->get());
  finish = cur_ms();
  size_t compile_time = finish - start;
  if (!res.has_value()) throw std::runtime_error(res.error());
  auto& fn = res.value();
  fn.ch->first.disasm(std::cout);
  std::cout << "\nvm: \n";

  using namespace ami;

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
