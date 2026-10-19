find_package(CLI11 CONFIG REQUIRED)

add_executable(bsd
  main.cpp
  option.cpp
  ${PROJECT_SOURCE_DIR}/src/res/res.rc
)

target_include_directories(bsd PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(bsd PRIVATE
  bsd_core
  CLI11::CLI11
)

if (WIN32)
  # bsd.exe loads bsd_core.dll at runtime; keep the DLL (and the DLLs it
  # imports) next to the executable.
  add_custom_command(TARGET bsd POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:bsd_core>" "$<TARGET_FILE_DIR:bsd>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:bsd_core> "$<TARGET_FILE_DIR:bsd>"
    COMMAND_EXPAND_LISTS
    COMMENT "Deploying bsd_core.dll and its runtime dependencies"
  )

  # $<TARGET_RUNTIME_DLLS> does not report every transitive dependency of
  # bsd_core.dll (e.g. OpenSSL and zlib imported through libcurl). When the
  # project is configured with the vcpkg toolchain, deploy the whole runtime
  # bin directory as well so bsd.exe can be started from anywhere.
  if (DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_vcpkg_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    if (EXISTS "${_vcpkg_bin}")
      file(GLOB _vcpkg_runtime_dlls "${_vcpkg_bin}/*.dll")
      add_custom_command(TARGET bsd POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_vcpkg_runtime_dlls} "$<TARGET_FILE_DIR:bsd>"
        COMMAND_EXPAND_LISTS
        COMMENT "Deploying vcpkg runtime DLLs"
      )
    endif()
  endif()
endif()
