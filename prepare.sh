#!/bin/sh
cmake -S . -G "Ninja" -S . -B debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
