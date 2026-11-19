cmake_minimum_required(VERSION 3.16)
project(CrosscubLibraryFinish LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
include(FetchContent)
FetchContent_Declare(googletest URL https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.17.0 URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c)
FetchContent_MakeAvailable(googletest)
enable_testing()
include(GoogleTest)
add_library(crosspoint_test_common INTERFACE)
target_compile_options(crosspoint_test_common INTERFACE -Wall -Wextra)
foreach(suite library_format library_text library_index_file library_input_buffer library_recent_order)
  add_subdirectory(${suite})
endforeach()
file(GLOB LIBRARY_BUILDER_TESTS CONFIGURE_DEPENDS library_builder/*Test.cpp)
add_executable(LibraryBuilderTest ${LIBRARY_BUILDER_TESTS}
  ${REPO_ROOT}/lib/LibraryIndex/LibraryBuilder.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryIndexFile.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryMetadataCache.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibrarySort.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryFormat.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryText.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp)
target_include_directories(LibraryBuilderTest PRIVATE library_builder/stubs ${REPO_ROOT}/lib/LibraryIndex ${REPO_ROOT}/lib/Serialization ${REPO_ROOT}/lib/Utf8)
target_link_libraries(LibraryBuilderTest PRIVATE crosspoint_test_common GTest::gtest_main)
find_package(EXPAT REQUIRED)
target_link_libraries(LibraryBuilderTest PRIVATE EXPAT::EXPAT)
target_compile_definitions(LibraryBuilderTest PRIVATE LIBRARY_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/upstream-v2.idx")
gtest_discover_tests(LibraryBuilderTest DISCOVERY_TIMEOUT 20)
