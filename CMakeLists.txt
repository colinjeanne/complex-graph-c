cmake_minimum_required(VERSION 3.28)

project(ComplexGraph)

add_executable(complex-graph)

target_sources(complex-graph
  PRIVATE
    src/main.c
)
