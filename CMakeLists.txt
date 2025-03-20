cmake_minimum_required(VERSION 3.27)
project(ami)

set(CMAKE_CXX_STANDARD 23)

add_executable(ami main.cpp
        src/tosuto.h
        src/tosuto.cpp
        src/lex.h
        src/lex.cpp
        src/parse.h
        src/parse.h
        src/parse.h
        src/parse.cpp
        src/parse.cpp
        src/vm/vm.h
        src/vm/vm.cpp
        src/vm/compile.h
        src/vm/compile.cpp
        src/vm/value.h
        src/vm/value.cpp
        src/rigtorp.h
)

target_compile_options(ami PUBLIC "$<$<CONFIG:Release>:-Ofast>")
