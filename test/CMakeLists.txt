cmake_minimum_required(VERSION 3.16)

project(cpp_jet_peer_tests LANGUAGES CXX)

find_package(Threads REQUIRED)

set(GTest_REQUIREDVERSION "1.12.0")

find_package(GTest ${GTest_REQUIREDVERSION} )

if(GTest_FOUND)
  message(STATUS "Found GTest: ${GTest_VERSION} ${GTest_CONFIG}")
else()
  message(STATUS "Fetching GTest ${GTest_REQUIREDVERSION}...")
  include(FetchContent)
  set(GTest_WITH_POST_BUILD_UNITTEST OFF)
  set(GTest_WITH_TESTS OFF)
  set(INSTALL_GTest OFF)
  set(BUILD_GMOCK OFF)
  FetchContent_Declare(
    GTest
    URL https://github.com/google/googletest/archive/release-${GTest_REQUIREDVERSION}.zip
  )
  FetchContent_MakeAvailable(GTest)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CGOV_COMPILE_FLAGS -fno-omit-frame-pointer -fno-optimize-sibling-calls -ftest-coverage -fprofile-arcs)
  set(GCOV_LINK_FLAGS -fprofile-arcs -ftest-coverage)
  set(GCOV_LIB gcov)
#  set(ASAN_COMPILE_FLAGS -fsanitize=address -fno-omit-frame-pointer)
#  set(ASAN_LIB asan)
endif()



add_custom_target(coverage
  COMMAND echo "=================== Run ===================="
  COMMAND ${CMAKE_MAKE_PROGRAM} test ARGS="--output-on-failure"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

set_target_properties(coverage
  PROPERTIES
  ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/*.html"
)

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES coverage)

enable_testing()


if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  SET(CMAKE_CXX_FLAGS "-O0 -fno-omit-frame-pointer -fno-optimize-sibling-calls  -ftest-coverage -fprofile-arcs ${CMAKE_CXX_FLAGS}")
  SET(LDFLAGS "-fprofile-arcs -ftest-coverage ${LDFLAGS}")
endif()

set(PEER_SOURCES
    ../lib/asyncrequest.cpp
    ../lib/peer.cpp
    ../lib/peerasync.cpp
    ../lib/syncrequest.cpp
    ../lib/jsoncpprpc_exception.cpp
)
add_library( peer_test_lib OBJECT ${PEER_SOURCES} )

target_include_directories(peer_test_lib PUBLIC ../include)


target_compile_options(peer_test_lib PRIVATE
 $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
  -Wall -Werror ${CGOV_COMPILE_FLAGS} ${ASAN_COMPILE_FLAGS}>
)
set_target_properties(peer_test_lib PROPERTIES
  CXX_STANDARD_REQUIRED ON
  CXX_STANDARD 17
  CXX_EXTENSIONS OFF
)

target_link_options(peer_test_lib PRIVATE ${GCOV_LINK_FLAGS} )
target_link_libraries(
  peer_test_lib
  ${ASAN_LIB}
  ${GCOV_LIB}
  jsoncpp_lib
  daq::stream
  hbk::hbk
)


####### Depends on a running jet daemon
add_executable( exceptiontest testException.cpp )

####### Depends on a running jet daemon
add_executable( peertest test.cpp )

####### Depends on a running jet daemon
add_executable( asyncpeertest testAsync.cpp )





get_property(targets DIRECTORY "${CMAKE_CURRENT_LIST_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
foreach(tgt ${targets})
  get_target_property(target_type ${tgt} TYPE)
  if (target_type STREQUAL "EXECUTABLE")
    target_compile_options(${tgt} PRIVATE
      $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall -Werror ${CGOV_COMPILE_FLAGS} ${ASAN_COMPILE_FLAGS}>
    )
    target_link_options(${tgt} PRIVATE ${GCOV_LINK_FLAGS} )
    target_link_libraries(${tgt} ${ASAN_LIB} ${GCOV_LIB} Threads::Threads peer_test_lib hbk::hbk daq::stream jsoncpp_lib GTest::gtest GTest::gtest_main)
    #target_link_libraries(${tgt} ${ASAN_LIB} ${GCOV_LIB} Threads::Threads peer_test_lib hbk::hbk rrrrrrrrrrrrrrrrrrrr jsoncpp_lib GTest::gtest GTest::gtest_main)
    target_include_directories(${tgt} PRIVATE ${Boost_INCLUDE_DIRS})


    set_target_properties(${tgt} PROPERTIES
      LINK_FLAGS "${EXTRA_LINK_FLAGS}"
      CXX_STANDARD_REQUIRED ON
      CXX_STANDARD 17
      CXX_EXTENSIONS OFF
    )
    add_test(NAME jet.${tgt} COMMAND ${tgt}
      --report_level=no
      --log_level=all
    )
    add_dependencies(coverage ${tgt})
  endif()
endforeach()

set(COMMON_BRANCH_OPTIONS "--exclude-unreachable-branches" "--exclude-throw-branches")
# exclude tests and external library code form coverage
# note: cmake replaces ' ' in string with '\ ' creating a list solves this problem; add --branches to use branch coverage again
set(EXCLUDED_DIRECTORIES "-e${CMAKE_SOURCE_DIR}/test" "-e${CMAKE_BINARY_DIR}/_deps")



add_custom_command(TARGET coverage
  COMMAND gcovr -r ${CMAKE_SOURCE_DIR} ${EXCLUDED_DIRECTORIES} --object-dir=${CMAKE_BINARY_DIR} ${COMMON_BRANCH_OPTIONS} --html-details -o ${CMAKE_BINARY_DIR}/coverage.html
  COMMAND gcovr -r ${CMAKE_SOURCE_DIR} ${EXCLUDED_DIRECTORIES} --object-dir=${CMAKE_BINARY_DIR} ${COMMON_BRANCH_OPTIONS} --json -o ${CMAKE_BINARY_DIR}/coverage.json
  COMMAND gcovr -r ${CMAKE_SOURCE_DIR} ${EXCLUDED_DIRECTORIES} --object-dir=${CMAKE_BINARY_DIR} ${COMMON_BRANCH_OPTIONS}
)
