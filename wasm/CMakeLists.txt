cmake_minimum_required(VERSION 3.10)
project(ITKCleaverWasm)

set(CMAKE_CXX_STANDARD 17)

if(EMSCRIPTEN)
  set(io_components
    )
elseif(WASI)
  set(io_components
    ITKIONRRD
    ITKIOMeshVTK
    )
else()
  set(io_components
    ITKMeshIO
    ITKImageIO
    )
endif()
find_package(ITK REQUIRED
  COMPONENTS
    ${io_components}
    WebAssemblyInterface
    Cleaver
  )
include(${ITK_USE_FILE})

add_executable(itk-cleaver ITKCleaverWasm.cxx)
target_link_libraries(itk-cleaver PUBLIC ${ITK_LIBRARIES})

enable_testing()
add_test(NAME ITKCleaverWasmLabelImageTest
  COMMAND ITKCleaverWasm
  --input
    ${CMAKE_CURRENT_SOURCE_DIR}/mickey.nrrd
  --triangle
    ${CMAKE_CURRENT_BINARY_DIR}/mickey-triangle.vtk
  )

add_test(NAME ITKCleaverIndicatorFunctionTest
  COMMAND ITKCleaverWasm
  --input
    ${CMAKE_CURRENT_SOURCE_DIR}/spheres1.nrrd
    ${CMAKE_CURRENT_SOURCE_DIR}/spheres2.nrrd
    ${CMAKE_CURRENT_SOURCE_DIR}/spheres3.nrrd
    ${CMAKE_CURRENT_SOURCE_DIR}/spheres4.nrrd
  --triangle
    ${CMAKE_CURRENT_BINARY_DIR}/indicator-triangle.vtk
  )
