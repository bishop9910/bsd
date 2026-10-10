set(SOURCES 
  core.cpp
  thread_pool.cpp
  download.cpp
  utils.cpp
)

add_library(BSD_CORE STATIC ${SOURCES})

target_include_directories(BSD_CORE PUBLIC ${PROJECT_SOURCE_DIR}/include)

find_package(CURL CONFIG REQUIRED)
find_package(stduuid CONFIG REQUIRED)

target_link_libraries(BSD_CORE PRIVATE 
  CURL::libcurl
  stduuid
)