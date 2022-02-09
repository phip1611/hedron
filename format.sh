#!/usr/bin/sh

# Convenient wrapper that applies clang-format on the project.

find include -iname \*.hpp | xargs clang-format -i
find src -iname \*.cpp | xargs clang-format -i
