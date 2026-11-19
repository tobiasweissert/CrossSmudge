find_path(ARDUINOJSON_INCLUDE_DIR ArduinoJson.h
  HINTS "$ENV{ARDUINOJSON_INCLUDE_DIR}" "${REPO_ROOT}/.pio/libdeps/simulator/ArduinoJson/src")
if(NOT ARDUINOJSON_INCLUDE_DIR)
  message(FATAL_ERROR "Set ARDUINOJSON_INCLUDE_DIR to the selected ArduinoJson/src directory")
endif()
add_executable(PersistableStoreAtomicTest
  PersistableStoreAtomicTest.cpp
  ${REPO_ROOT}/lib/Serialization/PersistableStore.cpp
)

target_include_directories(PersistableStoreAtomicTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/Serialization
  ${ARDUINOJSON_INCLUDE_DIR}
)

target_link_libraries(PersistableStoreAtomicTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

# This suite executes only storage operations; credential codec is not linked.
target_compile_options(PersistableStoreAtomicTest PRIVATE -ffunction-sections -fdata-sections)
target_link_options(PersistableStoreAtomicTest PRIVATE -Wl,--gc-sections)
gtest_discover_tests(PersistableStoreAtomicTest)
