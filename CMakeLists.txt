cmake_minimum_required(VERSION 3.28)

project(ComplexGraph)

add_executable(complex-graph)
find_package(PNG)
target_link_libraries(complex-graph
  PRIVATE m
  PRIVATE PNG::PNG
)
set_property(TARGET complex-graph PROPERTY C_STANDARD 23)
set_property(TARGET complex-graph PROPERTY COMPILE_WARNING_AS_ERROR ON)

target_sources(complex-graph
  PRIVATE
    src/main.c
    src/deque.c
    src/expression.c
    src/interval.c
    src/options.c
    src/write_png.c
)

add_executable(complex-graph-test)
target_link_libraries(complex-graph-test PRIVATE m)
set_property(TARGET complex-graph-test PROPERTY C_STANDARD 23)
set_property(TARGET complex-graph-test PROPERTY COMPILE_WARNING_AS_ERROR ON)

target_sources(complex-graph-test
  PRIVATE
    test/expression_test.c
    test/interval_test.c
    test/main.c
    test/variable_test.c
    src/deque.c
    src/expression.c
    src/interval.c
)

target_include_directories(complex-graph-test
  PRIVATE
    src
)

enable_testing()

add_test(NAME expression-test COMMAND ./debug/complex-graph-test)
