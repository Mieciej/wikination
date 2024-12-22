#!/bin/bash
set -e
set -x
cd build
g++ ../imgui/*.cpp -c
g++ ../imgui/backends/imgui_impl_sdl2.cpp ../imgui/backends/imgui_impl_sdlrenderer2.cpp -I../imgui -c -I/usr/include/SDL2
cd ..
g++ wikination.cpp build/*.o -o wikination -I./imgui -I./imgui/backends -lsqlite3 -Wall -lSDL2

