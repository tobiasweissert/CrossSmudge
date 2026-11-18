cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(crosscub_product_tests NONE)
  enable_testing()
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME crosscub_reading_heatmap
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../reading_heatmap/run.py)
add_test(NAME crosscub_reading_heatmap_sanitize
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../reading_heatmap/run.py --sanitize)
foreach(case product web release ota_activity logo stopwatch)
  add_test(NAME crosscub_${case}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_${case}.py)
endforeach()

add_test(NAME crosscub_catalog_preservation
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../integration_catalog/check_catalog.py)
