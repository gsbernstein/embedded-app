FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

set(COMPONENT_SRCS ${app_sources})
set(COMPONENT_ADD_INCLUDEDIRS ".")
set(REQUIRES arduino esp-nimble-cpp)

register_component()
