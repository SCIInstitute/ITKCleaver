itk_module_test()

set(CleaverTests
  itkCleaverImageToMeshFilterTest.cxx
  )

CreateTestDriver(Cleaver "${Cleaver-Test_LIBRARIES}" "${CleaverTests}")

itk_add_test(NAME itkCleaverImageToMeshFilterTest
  COMMAND CleaverTestDriver
    --compare
    DATA{Baseline/itkCleaverImageToMeshFilterTestOutput.mha}
    ${ITK_TEST_OUTPUT_DIR}/itkCleaverImageToMeshFilterTestOutput.mha
  itkCleaverImageToMeshFilterTest
    ${ITK_TEST_OUTPUT_DIR}/itkCleaverImageToMeshFilterTestOutput.mha
  )
