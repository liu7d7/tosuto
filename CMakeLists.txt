cmake_minimum_required(VERSION 3.27)
project(ami)

set(CMAKE_CXX_STANDARD 23)

add_executable(ami main.cpp
        src/ami.h
        src/ami.cpp
        src/lex.h
        src/lex.cpp
        src/parse.h
        src/parse.h
        src/parse.h
        src/parse.cpp
        src/parse.cpp
        src/interpret.h
        src/interpret.h
        src/interpret.cpp
        src/value.h
        src/value.cpp
)
