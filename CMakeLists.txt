cmake_minimum_required(VERSION 3.16)

project(jetpeer_all VERSION 3.0.6 LANGUAGES CXX)

include(FetchContent)

option(JETPEER_SIMULATION           "Jet peer simulation" OFF)
option(JETPEER_TOOLS                "Compile client software" OFF)
option(JETPEER_POST_BUILD_UNITTEST  "Automatically run unit-tests as a post build step" OFF)
option(JETPEER_GENERATE_DOC         "Generate documentation" OFF)

set(jsoncpp_REQUIREDVERSION "1.9.3")
# This imports "jsoncpp_lib" instead of the expected "jsoncpp"
find_package(jsoncpp ${jsoncpp_REQUIREDVERSION} QUIET)
if(jsoncpp_FOUND)
  message(STATUS "Found jsoncpp: ${jsoncpp_VERSION} ${jsoncpp_CONFIG}")
else(jsoncpp_FOUND)
  message(STATUS "Fetching jsoncpp ${jsoncpp_REQUIREDVERSION}...")
  if (CMAKE_VERSION VERSION_GREATER "3.24.0")
    cmake_policy(SET CMP0135 OLD)
  endif()
  set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE INTERNAL "")
  set(JSONCPP_WITH_TESTS OFF CACHE INTERNAL "")
  set(JSONCPP_WITH_STRICT_ISO OFF)
  set(JSONCPP_DOWNLOAD_EXTRACT_TIMESTAMP ON)
  FetchContent_Declare(
    jsoncpp
    URL https://github.com/open-source-parsers/jsoncpp/archive/${jsoncpp_REQUIREDVERSION}.zip
  )
  FetchContent_MakeAvailable(jsoncpp)
endif(jsoncpp_FOUND)


set(hbk_REQUIREDVERSION "2.1.0")
find_package(hbk ${hbk_REQUIREDVERSION} QUIET)
if(hbk_FOUND)
  message(STATUS "Found hbk: ${hbk_VERSION} ${hbk_CONFIG}")
else(hbk_FOUND)
  message(STATUS "Fetching hbk ${hbk_REQUIREDVERSION}...")
  include(FetchContent)
  if(CMAKE_VERSION VERSION_GREATER "3.24.0")
    cmake_policy(SET CMP0135 OLD)
  endif()
  FetchContent_Declare(
    hbk
    GIT_SHALLOW
    GIT_REPOSITORY https://github.com/hbkworld/libhbk
    GIT_TAG v${hbk_REQUIREDVERSION}
  )
  FetchContent_MakeAvailable(hbk)
endif(hbk_FOUND)


set(stream_REQUIREDVERSION "1.0.2")
find_package(stream ${stream_REQUIREDVERSION} QUIET)
if(stream_FOUND)
  message(STATUS "Found libstream: ${stream_VERSION} ${stream_CONFIG}")
else(stream_FOUND)
  message(STATUS "Fetching libstream ${stream_REQUIREDVERSION}...")
  include(FetchContent)
  if(CMAKE_VERSION VERSION_GREATER "3.24.0")
    cmake_policy(SET CMP0135 OLD)
  endif()
  FetchContent_Declare(
    stream
    GIT_SHALLOW
    GIT_REPOSITORY https://github.com/openDAQ/libstream.git
    GIT_TAG v${stream_REQUIREDVERSION}
  )
  FetchContent_MakeAvailable(stream)
endif(stream_FOUND)



if(JETPEER_GENERATE_DOC)
  INCLUDE(cmake/generate_doxygen.cmake)
endif()


if(JETPEER_SIMULATION)
  add_subdirectory(lib_simulation)
else()
  add_subdirectory(lib)
endif()
add_subdirectory(include)

if(JETPEER_TOOLS)
  add_subdirectory(tool)
endif()

if(JETPEER_POST_BUILD_UNITTEST)
enable_testing()
add_subdirectory(test)
endif()
