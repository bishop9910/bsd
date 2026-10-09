add_subdirectory(core)
add_subdirectory(utils)

set(SOURCES 
  main.cpp
)

add_executable(bsd 
  ${SOURCES}
)

target_include_directories(bsd PUBLIC ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(bsd PRIVATE  
  BSD_CORE
  BSD_UTILS
)