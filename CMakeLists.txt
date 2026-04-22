cmake_minimum_required(VERSION 3.16)
project(flow LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(flow
    src/main.c
    src/lexer.c
    src/parse.c
    src/ast.c
    src/value.c
    src/interp.c
    src/pathutil.c
)

target_include_directories(flow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_definitions(flow PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

install(TARGETS flow RUNTIME DESTINATION bin)
