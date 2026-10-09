set(SOURCES 
  utils.cpp
)

add_library(BSD_UTILS STATIC ${SOURCES})

target_include_directories(BSD_UTILS PUBLIC ${PROJECT_SOURCE_DIR}/include)