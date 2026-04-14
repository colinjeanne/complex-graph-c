#!/bin/bash

cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug
