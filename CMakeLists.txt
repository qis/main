cmake_minimum_required(VERSION 3.11.3 FATAL_ERROR)
project(main VERSION 0.1.0 LANGUAGES CXX)

set(PROJECT "Main")
set(COMPANY "Xiphos")
set(COPYING "2018 Alexej Harm")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_ATL_SECURE_NO_DEPRECATE)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=_CPPUNWIND)
  add_definitions(-D_SILENCE_CXX17_ADAPTOR_TYPEDEFS_DEPRECATION_WARNING)
  add_definitions(-D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  string(APPEND CMAKE_CXX_FLAGS " /await")
  foreach(config RELEASE RELWITHDEBINFO MINSIZEREL)
    string(APPEND CMAKE_CXX_FLAGS_${config} " /await:heapelide")
  endforeach()
else()
  add_compile_options(-fcoroutines-ts)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  string(REGEX REPLACE "[/-]W[0-9]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(APPEND CMAKE_CXX_FLAGS " /W4 /permissive-")
else()
  string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Wpedantic -Wconversion -Wno-unused-parameter")
  string(APPEND CMAKE_CXX_FLAGS " -fdiagnostics-absolute-paths -fcolor-diagnostics")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER build)

file(GLOB_RECURSE sources src/*.h src/*.cpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX src FILES ${sources})

configure_file(res/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
source_group("res" FILES ${CMAKE_CURRENT_BINARY_DIR}/config.h)
list(APPEND sources ${CMAKE_CURRENT_BINARY_DIR}/config.h)

if(WIN32)
  file(GLOB res res/resource.h res/resource.rc res/resource.manifest)
  source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/res PREFIX res FILES ${res})
  list(APPEND sources ${res})
endif()

add_executable(main WIN32 ${sources})
target_include_directories(main PRIVATE ${CMAKE_CURRENT_BINARY_DIR} res src)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_link_libraries(main PRIVATE c++experimental)
endif()

if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(main PRIVATE Threads::Threads)
endif()

find_package(unofficial-date REQUIRED)
target_link_libraries(main PRIVATE unofficial::date::date)

find_package(Boost 1.67.0 REQUIRED COMPONENTS system)
target_link_libraries(main PRIVATE Boost::system)

install(TARGETS main RUNTIME DESTINATION bin)

set_target_properties(main PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT main)

if(WIN32 AND NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/res/tzdata/windowsZones.xml)
  file(DOWNLOAD "https://unicode.org/repos/cldr/trunk/common/supplemental/windowsZones.xml"
    ${CMAKE_CURRENT_SOURCE_DIR}/res/tzdata/windowsZones.xml)
endif()

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/res/tzdata/version)
  if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/tzdata/tzdata.tar.gz)
    file(DOWNLOAD "https://data.iana.org/time-zones/releases/tzdata2018e.tar.gz"
      ${CMAKE_CURRENT_BINARY_DIR}/tzdata/tzdata.tar.gz)
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf tzdata.tar.gz
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tzdata)
  foreach(name africa antarctica asia australasia backward etcetera
      europe leapseconds northamerica pacificnew southamerica systemv version)
    file(COPY ${CMAKE_CURRENT_BINARY_DIR}/tzdata/${name} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/res/tzdata)
  endforeach()
endif()
