itk_module_test()

set(CleaverTests
  itkCleaverImageToMeshFilterTest.cxx
  )

CreateTestDriver(Cleaver "${Cleaver-Test_LIBRARIES}" "${CleaverTests}")

itk_add_test(NAME itkCleaverImageToMeshFilterTest
  COMMAND CleaverTestDriver
  itkCleaverImageToMeshFilterTest
    inputImagePlaceholder
    outputMeshPlaceholder
  )
