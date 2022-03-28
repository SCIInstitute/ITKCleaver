itk_module_test()

set(CleaverTests
  itkCleaverImageToMeshFilterTest.cxx
  )

CreateTestDriver(Cleaver "${Cleaver-Test_LIBRARIES}" "${CleaverTests}")

itk_add_test(NAME itkCleaverImageToMeshFilterLabelImageTest
  COMMAND CleaverTestDriver
  itkCleaverImageToMeshFilterTest
    ${ITK_TEST_OUTPUT_DIR}/itkCleaverImageToMeshFilterLabelImageTest.vtk
    DATA{Input/mickey.nrrd}
  )

itk_add_test(NAME itkCleaverImageToMeshFilterIndicatorFunctionTest
  COMMAND CleaverTestDriver
  itkCleaverImageToMeshFilterTest
    ${ITK_TEST_OUTPUT_DIR}/itkCleaverImageToMeshFilterIndicatorFunctionTest.vtk
    DATA{Input/spheres1.nrrd}
    DATA{Input/spheres2.nrrd}
    DATA{Input/spheres3.nrrd}
    DATA{Input/spheres4.nrrd}
  )
