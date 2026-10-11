set(SOURCES 
  core.cpp
  thread_pool.cpp
  download.cpp
  utils.cpp
  option.cpp
  curl.cpp
  meta.cpp
)

add_library(BSD_CORE STATIC ${SOURCES})

target_include_directories(BSD_CORE PUBLIC ${PROJECT_SOURCE_DIR}/include)

find_package(CURL CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

target_link_libraries(BSD_CORE 
  PUBLIC
    CURL::libcurl
  PRIVATE
    CLI11::CLI11
)