find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(caller_probe "${CMAKE_CURRENT_BINARY_DIR}/ClippingCallerProbe.cpp")
set(caller_probe_dir "${CMAKE_CURRENT_SOURCE_DIR}/../crosspoint_migration/clipping-release")
add_custom_command(OUTPUT ${caller_probe}
  COMMAND ${Python3_EXECUTABLE} ${caller_probe_dir}/extract_callers.py ${caller_probe}
  DEPENDS ${caller_probe_dir}/extract_callers.py ${caller_probe_dir}/caller_prefix.h
          ${caller_probe_dir}/caller_tests.inc ${REPO_ROOT}/src/activities/home/FileBrowserActivity.cpp)
foreach(target ClippingStoreTest ClippingStoreS3Test)
  add_executable(${target} CheckedAllocationObserver.cpp CheckedStorageTest.cpp ${caller_probe} ClippingStoreTest.cpp ClippingUtf8Test.cpp ClippingV5ContractTest.cpp ClippingReleaseTest.cpp ClippingReviewMoveTest.cpp ${REPO_ROOT}/src/ClippingStore.cpp ${REPO_ROOT}/lib/Utf8/Utf8.cpp ${REPO_ROOT}/lib/uzlib/src/uzlib_checksums.c)
  target_link_libraries(${target} PRIVATE crosspoint_test_common GTest::gtest_main)
  target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} stubs ../library_builder/stubs ${REPO_ROOT}/src ${REPO_ROOT}/lib/Serialization ${REPO_ROOT}/lib/Utf8 ${REPO_ROOT}/lib/uzlib/src)
  gtest_discover_tests(${target} TEST_PREFIX "${target}.")
endforeach()
target_compile_definitions(ClippingStoreS3Test PRIVATE CONFIG_IDF_TARGET_ESP32S3=1)

set(bookmove_probe "${CMAKE_CURRENT_BINARY_DIR}/ClippingBookMoveProbe.cpp")
add_custom_command(OUTPUT ${bookmove_probe}
  COMMAND ${Python3_EXECUTABLE} ${caller_probe_dir}/extract_bookmove.py ${bookmove_probe}
  DEPENDS ${caller_probe_dir}/extract_bookmove.py ${caller_probe_dir}/bookmove_prefix.h
          ${caller_probe_dir}/bookmove_tests.inc ${REPO_ROOT}/src/util/BookMoveUtils.cpp)
foreach(target ClippingBookMoveTest ClippingBookMoveS3Test)
  add_executable(${target} CheckedAllocationObserver.cpp ${bookmove_probe} ${REPO_ROOT}/src/ClippingStore.cpp ${REPO_ROOT}/lib/Utf8/Utf8.cpp ${REPO_ROOT}/lib/uzlib/src/uzlib_checksums.c)
  target_link_libraries(${target} PRIVATE crosspoint_test_common GTest::gtest_main)
  target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} stubs ../library_builder/stubs ${REPO_ROOT}/src ${REPO_ROOT}/lib/Serialization ${REPO_ROOT}/lib/Utf8 ${REPO_ROOT}/lib/uzlib/src)
  gtest_discover_tests(${target} TEST_PREFIX "${target}.")
endforeach()
target_compile_definitions(ClippingBookMoveS3Test PRIVATE CONFIG_IDF_TARGET_ESP32S3=1)
