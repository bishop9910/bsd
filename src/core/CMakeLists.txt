set(SOURCES 
  core.cpp
)

add_library(BSD_CORE STATIC ${SOURCES})

target_include_directories(BSD_CORE PUBLIC ${PROJECT_SOURCE_DIR}/include)

find_package(CURL REQUIRED)

target_link_libraries(BSD_CORE PRIVATE CURL::libcurl)