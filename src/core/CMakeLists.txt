set(SOURCES
  core.cpp
  thread_pool.cpp
  file_name.cpp
  download.cpp
  utils.cpp
  curl.cpp
  meta.cpp
  hash.cpp
  chunk.cpp
  rate_limiter.cpp
  c_api.cpp
)

add_library(bsd_core SHARED ${SOURCES})

target_compile_definitions(bsd_core PRIVATE BSD_CORE_BUILD)

target_include_directories(bsd_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

find_package(CURL CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(fmt CONFIG REQUIRED)

target_link_libraries(bsd_core
  PRIVATE
    CURL::libcurl
    OpenSSL::Crypto
    fmt::fmt
)

if (WIN32)
  # timeBeginPeriod() used by the global rate limiter.
  target_link_libraries(bsd_core PRIVATE winmm)
endif()

set_target_properties(bsd_core PROPERTIES OUTPUT_NAME bsd_core)
