cmake_minimum_required(VERSION 3.10.2)
project(Cleaver)

include(FetchContent)
set(_itk_build_testing ${BUILD_TESTING})
#set(BUILD_TESTING OFF)
set(_itk_build_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
#set(cleaver_GIT_REPOSITORY "https://github.com/SCIInstitute/Cleaver2")
set(cleaver_GIT_REPOSITORY "https://github.com/thewtex/Cleaver2")
set(cleaver_GIT_TAG "15d39c2ea70301dd3b6db8f532681d547abea9fe")
FetchContent_Declare(
  cleaver_lib
  GIT_REPOSITORY ${cleaver_GIT_REPOSITORY}
  GIT_TAG ${cleaver_GIT_TAG})
FetchContent_GetProperties(cleaver_lib)
if(NOT cleaver_lib_POPULATED)
  FetchContent_Populate(cleaver_lib)
  add_subdirectory(${cleaver_lib_SOURCE_DIR}/src ${cleaver_lib_BINARY_DIR})
endif()
# Debugging / development, symlink Cleaver2 into the source tree
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Cleaver2/src)
set(BUILD_TESTING ${_itk_build_testing})
set(BUILD_SHARED_LIBS ${_itk_build_shared})

if(NOT ITK_SOURCE_DIR)
  find_package(ITK REQUIRED)
  list(APPEND CMAKE_MODULE_PATH ${ITK_CMAKE_DIR})
  include(ITKModuleExternal)
else()
  set(ITK_DIR ${CMAKE_BINARY_DIR})
  itk_module_impl()
endif()
