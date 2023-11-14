cmake_minimum_required(VERSION 3.8)

project(cpp_jetpeer_simulation LANGUAGES CXX VERSION 1.2.10)

set( INTERFACE_INCLUDE_DIR ../include/jet)

####### libjetpeerasync
SET( PEERASYNC_INTERFACE_HEADERS
    ${INTERFACE_INCLUDE_DIR}/peerasync.hpp
    ${INTERFACE_INCLUDE_DIR}/defines.h
)

set(PEERASYNC_SOURCES
  ${PEERASYNC_INTERFACE_HEADERS}
  peerasync.cpp
  ../lib/jsoncpprpc_exception.cpp
  )

add_library(jetpeerasync_simulation SHARED ${PEERASYNC_SOURCES})
add_library(jet::jetpeerasync ALIAS jetpeerasync_simulation)

set_property(TARGET jetpeerasync_simulation APPEND PROPERTY PUBLIC_HEADER
    ${PEERASYNC_INTERFACE_HEADERS}
)



####### libjetpeer

SET( PEERSYNC_INTERFACE_HEADERS
    ${INTERFACE_INCLUDE_DIR}/peer.hpp
)

set(PEERSYNC_SOURCES
  ${PEERSYNC_INTERFACE_HEADERS}
  peer.cpp
  )

add_library(jetpeer_simulation SHARED ${PEERSYNC_SOURCES})
add_library(jet::jetpeer ALIAS jetpeer_simulation)

set_property(TARGET jetpeer_simulation APPEND PROPERTY PUBLIC_HEADER
    ${PEERSYNC_INTERFACE_HEADERS}
)





include(CMakePackageConfigHelpers)

foreach(tgt jetpeerasync_simulation jetpeer_simulation)
  if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    target_link_libraries( ${tgt}
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

  add_library(jet::${tgt} ALIAS ${tgt})

  write_basic_package_version_file("${tgt}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${tgt}ConfigVersion.cmake" DESTINATION lib/cmake/${tgt})

  # Create and install stuff for find_package
  install(EXPORT ${tgt}
    NAMESPACE jet::
     DESTINATION lib/cmake/${tgt}
     FILE "${tgt}Config.cmake"
  )


  configure_file("pkg-config/${tgt}.pc.in" "pkg-config/${tgt}.pc" @ONLY)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pkg-config/${tgt}.pc" DESTINATION lib/pkgconfig)

  install(TARGETS ${tgt} EXPORT ${tgt}
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib
      PUBLIC_HEADER DESTINATION include/jet
  )
endforeach()




#configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h")
#install (FILES "${PROJECT_BINARY_DIR}/version.h" DESTINATION include/jet)
