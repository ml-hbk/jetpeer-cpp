find_package(Threads)

add_executable(jetcat
  notifier.cpp
  cat.cpp
)
target_link_libraries(jetcat
    jet::jetpeerasync
)

add_executable(jetcatexternaleventloop
  notifier.cpp
  catexternaleventloop.cpp
)
target_link_libraries(jetcatexternaleventloop
    jet::jetpeerasync
)

add_executable(jetget
  notifier.cpp
  get.cpp
)
target_link_libraries( jetget
    jet::jetpeerasync
)

add_executable(jetcache cache.cpp)
target_link_libraries( jetcache
    jet::jetpeerasync
)

add_executable(jetstate jetstate.cpp )
target_link_libraries( jetstate
    jet::jetpeerasync
)

add_executable(jetsetasync jetsetasync.cpp)
target_link_libraries( jetsetasync
    jet::jetpeerasync
)

add_executable(jetmethod jetmethod.cpp)
target_link_libraries( jetmethod
    jet::jetpeerasync
)

add_executable(jetset jetset.cpp)
target_link_libraries( jetset
    jet::jetpeer
    ${CMAKE_THREAD_LIBS_INIT}
)


add_executable(jetexec exec.cpp)
target_link_libraries( jetexec
    jet::jetpeer
    ${CMAKE_THREAD_LIBS_INIT}
)


add_executable( measureSpeed measureSpeed.cpp)
target_link_libraries( measureSpeed
    jet::jetpeer
    ${CMAKE_THREAD_LIBS_INIT}
)

add_executable( jetinfo info.cpp )
target_link_libraries( jetinfo
    jet::jetpeer
    ${CMAKE_THREAD_LIBS_INIT}
)


####### QuantumX specific!
set(QXROUTING_SOURCES
    qxrouting/Routing.cpp
    qxrouting/Device.cpp
    qxrouting/RoutedDevice.cpp
)

add_executable( qxrouting ${QXROUTING_SOURCES} )
target_link_libraries( qxrouting
    jet::jetpeerasync
    ${CMAKE_THREAD_LIBS_INIT}
)

get_property(targets DIRECTORY "${CMAKE_CURRENT_LIST_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
foreach(tgt ${targets})
  get_target_property(target_type ${tgt} TYPE)
  if (target_type STREQUAL "EXECUTABLE")
    set_target_properties(${tgt} PROPERTIES
      CXX_STANDARD_REQUIRED ON
      CXX_STANDARD 17
      CXX_EXTENSIONS OFF
    )
  endif()
  install(TARGETS ${tgt} RUNTIME DESTINATION bin)
endforeach()
