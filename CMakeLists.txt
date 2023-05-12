cmake_minimum_required(VERSION 3.16.3)
project(Cleaver)

set(CMAKE_CXX_STANDARD 17)

# CMake option standard variable override
if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

include(FetchContent)
if(DEFINED BUILD_TESTING)
  set(_itk_build_testing ${BUILD_TESTING})
else()
  set(_itk_build_testing OFF) # Default
endif()
if(WASI OR EMSCRIPTEN)
  # Cleaver lib vendored GTest incompatibility
  set(BUILD_TESTING OFF CACHE BOOL "Build testing code" FORCE)
endif()
if (CMAKE_CXX_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-maybe-uninitialized")
endif()
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-copy")
endif()
set(_itk_build_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
set(cleaver_GIT_REPOSITORY "https://github.com/SCIInstitute/Cleaver")
set(cleaver_GIT_TAG "bb1d7e0604171b52f4e2f3b64178860dde0e4a58")
FetchContent_Declare(
  cleaver_lib
  GIT_REPOSITORY ${cleaver_GIT_REPOSITORY}
  GIT_TAG ${cleaver_GIT_TAG})
FetchContent_GetProperties(cleaver_lib)
if(NOT cleaver_lib_POPULATED)
  FetchContent_Populate(cleaver_lib)
  add_subdirectory(${cleaver_lib_SOURCE_DIR}/src ${cleaver_lib_BINARY_DIR})
endif()
# Debugging / development, symlink Cleaver into the source tree
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Cleaver/src)
set(BUILD_TESTING ${_itk_build_testing} CACHE BOOL "Build testing code" FORCE)
set(BUILD_SHARED_LIBS ${_itk_build_shared})

set(Cleaver_LIBRARIES cleaver jsoncpp)
set(Cleaver_INCLUDE_DIRS
  ${Cleaver_SOURCE_DIR}/include
  ${cleaver_lib_SOURCE_DIR}/src/lib
  )

if(NOT ITK_SOURCE_DIR)
  find_package(ITK REQUIRED)
  list(APPEND CMAKE_MODULE_PATH ${ITK_CMAKE_DIR})
  include(ITKModuleExternal)
  if(WASI OR EMSCRIPTEN)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    add_subdirectory(wasm)
  endif()
else()
  set(ITK_DIR ${CMAKE_BINARY_DIR})
  itk_module_impl()
endif()

itk_module_target(cleaver NO_INSTALL)
itk_module_target(jsoncpp NO_INSTALL)
