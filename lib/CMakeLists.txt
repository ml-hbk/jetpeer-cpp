project(jetpeer LANGUAGES CXX VERSION "${jetpeer_all_VERSION_MAJOR}.${jetpeer_all_VERSION_MINOR}.${jetpeer_all_VERSION_PATCH}")

set( INTERFACE_INCLUDE_DIR ../include/jet)

find_package(Threads)

####### libjetpeerasync
set( PEERASYNC_INTERFACE_HEADERS
  ${INTERFACE_INCLUDE_DIR}/peerasync.hpp
  ${INTERFACE_INCLUDE_DIR}/defines.h
)
set(PEERASYNC_SOURCES
  ${PEERASYNC_INTERFACE_HEADERS}
  peerasync.cpp
  asyncrequest.cpp
  jsoncpprpc_exception.cpp
)

add_library(jetpeerasync ${PEERASYNC_SOURCES})
set_property(TARGET jetpeerasync APPEND PROPERTY PUBLIC_HEADER
  ${PEERASYNC_INTERFACE_HEADERS}
)


####### libjetpeer
set( PEERSYNC_INTERFACE_HEADERS
    ${INTERFACE_INCLUDE_DIR}/peer.hpp
)
set(PEERSYNC_SOURCES
  ${PEERSYNC_INTERFACE_HEADERS}
  peer.cpp
  syncrequest.cpp
)

add_library(jetpeer ${PEERSYNC_SOURCES})
set_property(TARGET jetpeer APPEND PROPERTY PUBLIC_HEADER
  ${PEERSYNC_INTERFACE_HEADERS}
)

# Synchronous peer is a shell around the asynchronous peer
# Programs using the Synchronous peer, have also to link the asynch peer
# It also requires multithreading
target_link_libraries(jetpeer INTERFACE jetpeerasync Threads::Threads)


include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

foreach(tgt jetpeerasync jetpeer)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    target_link_libraries( ${tgt} INTERFACE
      ws2_32
    )
  endif()

  target_compile_options(${tgt} PRIVATE
   $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
     -fno-common -Wall -Werror -Wshadow -Wextra -Winit-self -Wstrict-overflow=5 -Wunused-result -Wcast-qual -Wcast-align -Wpointer-arith -Wformat=2 -Wwrite-strings -Wmissing-declarations -pedantic -Wno-shadow>
  )

  set_target_properties(
    ${tgt}
    PROPERTIES
    SOVERSION ${PROJECT_VERSION_MAJOR}
    VERSION ${PROJECT_VERSION}
    CXX_STANDARD_REQUIRED ON
    CXX_STANDARD 11
    CXX_EXTENSIONS OFF
  )

  # When using this library, those will be added to the libraries to link against
  # PUBLIC because stuff being build here requires the headers
  target_link_libraries( ${tgt} PUBLIC
    hbk::hbk
    jsoncpp_lib
  )

  add_library(jet::${tgt} ALIAS ${tgt})

  target_include_directories(${tgt} PUBLIC
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  )

  write_basic_package_version_file("${tgt}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${tgt}ConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${tgt})

  # Create and install stuff for find_package
  install(EXPORT ${tgt}
     NAMESPACE jet::
     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${tgt}
     FILE "${tgt}Config.cmake"
  )


  configure_file("pkg-config/${tgt}.pc.in" "pkg-config/${tgt}.pc" @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pkg-config/${tgt}.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

  install(TARGETS ${tgt} EXPORT ${tgt}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jet
  )
endforeach()

configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h" NEWLINE_STYLE UNIX)
install (FILES "${PROJECT_BINARY_DIR}/version.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jet)
