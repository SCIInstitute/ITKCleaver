# the top-level README is used for describing this module, just
# re-used it for documentation here
get_filename_component(MY_CURRENT_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
file(READ "${MY_CURRENT_DIR}/README.rst" DOCUMENTATION)

# define the dependencies of the include module and the tests
itk_module(Cleaver
  DEPENDS
    ITKCommon
  COMPILE_DEPENDS
    ITKThresholding
    ITKMesh
    ITKImageFilterBase
    ITKSmoothing
    ITKImageIntensity
    ITKDistanceMap
  TEST_DEPENDS
    ITKTestKernel
    ITKIONRRD
    ITKIOMeshBase
  DESCRIPTION
    "${DOCUMENTATION}"
  EXCLUDE_FROM_DEFAULT
)
