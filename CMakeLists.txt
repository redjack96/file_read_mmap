cmake_minimum_required(VERSION 3.17)
project(file_read_mmap C)

set(CMAKE_C_STANDARD 11)

add_executable(file_read_mmap main.c)
add_executable(simple_file_read_mmap simple-mmap-date-inverter.c)