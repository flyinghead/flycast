cmake_minimum_required(VERSION 2.6)
project(xbyak)

file(GLOB headers xbyak/*.h)
install(FILES ${headers} DESTINATION include/xbyak)

