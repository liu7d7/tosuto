#include <iostream>
#include "src/ami.h"

int main() {
  ami::lexer lexer{"test.pico"};
  auto toks = lexer.lex();
  for (const auto& it : toks)
  {
    std::cout << it.to_string() << '\n';
  }

  ami::parser parser{toks};
  auto ast = parser.global();
  if (!ast.has_value()) throw std::runtime_error(ast.error());
  std::ofstream("out.txt") << (*ast)->pretty(0);

//  ami::symbol_table defaults{
//    {
//      {"log", ami::value{[](std::vector<ami::value> const& args, ami::symbol_table const& sym) -> ami::interpret_result
//                         {
//                           std::cout << args[0].display();
//                         }}}
//    }
//  };
  return 0;
}
