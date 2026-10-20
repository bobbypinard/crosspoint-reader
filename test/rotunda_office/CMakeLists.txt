add_executable(RotundaOfficeTest
  RotundaOfficeTest.cpp
  ${REPO_ROOT}/lib/Rotunda/OfficeTypes.cpp
  ${REPO_ROOT}/lib/Rotunda/TsvReader.cpp
  ${REPO_ROOT}/lib/Rotunda/OfficeHours.cpp
  ${REPO_ROOT}/lib/Rotunda/OfficeIndex.cpp
  ${REPO_ROOT}/lib/Rotunda/SaidLog.cpp
  ${REPO_ROOT}/lib/Rotunda/Clubs.cpp
  ${REPO_ROOT}/lib/Rotunda/OfficeToc.cpp
)

target_include_directories(RotundaOfficeTest PRIVATE
  ${REPO_ROOT}/lib/Rotunda
)

target_compile_definitions(RotundaOfficeTest PRIVATE
  ROTUNDA_SAMPLE_DIR="${REPO_ROOT}/docs/rotunda/sd-sample"
)

target_link_libraries(RotundaOfficeTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(RotundaOfficeTest)
