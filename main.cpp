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


  return 0;
}
