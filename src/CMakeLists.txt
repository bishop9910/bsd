add_subdirectory(core)
add_subdirectory(utils)

set(SOURCES 
  main.cpp
)

add_executable(bsd 
  ${SOURCES}
)

find_package(CLI11 CONFIG REQUIRED)

target_include_directories(bsd PUBLIC ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(bsd PRIVATE  
  BSD_CORE
  BSD_UTILS
  CLI11::CLI11
)