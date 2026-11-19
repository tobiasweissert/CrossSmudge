find_package(EXPAT REQUIRED)

add_executable(LibraryBuilderTest
  LibraryBuilderTest.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryBuilder.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryMetadataCache.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibrarySort.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryIndexFile.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryFormat.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryText.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)
target_include_directories(LibraryBuilderTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/FsHelpers
  ${REPO_ROOT}/lib/LibraryIndex
  ${REPO_ROOT}/lib/Serialization
  ${REPO_ROOT}/lib/Utf8
)
target_link_libraries(LibraryBuilderTest PRIVATE crosspoint_test_common GTest::gtest_main EXPAT::EXPAT)
gtest_discover_tests(LibraryBuilderTest)

add_executable(HalStorageShortWriteTest HalStorageShortWriteTest.cpp)
target_include_directories(HalStorageShortWriteTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stubs)
target_compile_options(HalStorageShortWriteTest PRIVATE -UNDEBUG)
add_test(NAME HalStorageShortWriteTest COMMAND HalStorageShortWriteTest)
