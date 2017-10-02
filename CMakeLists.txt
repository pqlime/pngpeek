cmake_minimum_required(VERSION 2.6)
project(pngpeek)

add_executable(pngpeek src/main.c src/pngpeek.c lib/lodepng.c)

install(TARGETS pngpeek RUNTIME DESTINATION bin)
