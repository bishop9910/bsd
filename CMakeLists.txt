cmake_minimum_required(VERSION 3.31)

project(bsd)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Use the default dynamic CRT (/MD) so that the prebuilt vcpkg libraries
# (fmt, libcurl, openssl, CLI11) are linked against the same C runtime.
add_subdirectory(src)

if(MSVC)
  target_compile_options(bsd_core PRIVATE /utf-8)
  target_compile_options(bsd PRIVATE /utf-8)
endif()
