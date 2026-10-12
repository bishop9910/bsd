add_subdirectory(core)

set(SOURCES 
  main.cpp
  res/res.rc
)

add_executable(bsd 
  ${SOURCES}
)

target_include_directories(bsd PUBLIC ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(bsd PRIVATE  
  BSD_CORE
)