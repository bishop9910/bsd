add_subdirectory(core)

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
  CLI11::CLI11
)