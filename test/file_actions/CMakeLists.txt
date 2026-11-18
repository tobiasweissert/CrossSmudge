cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(crosscub_file_actions_host NONE)
  enable_testing()
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME FileActionsHost
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run.py)
add_test(NAME FileActionsHostSanitized
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run.py --sanitize)
add_test(NAME FileActionsNested
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/nested_run.py)
add_test(NAME FileActionsNestedSanitized
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/nested_run.py --sanitize)
add_test(NAME FileActionsState
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/state_run.py)
add_test(NAME FileActionsStateSanitized
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/state_run.py --sanitize)
add_test(NAME FileActionsStateUi
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/state_run.py --ui)
add_test(NAME FileActionsStateUiSanitized
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/state_run.py --ui --sanitize)
set_tests_properties(FileActionsHost FileActionsHostSanitized FileActionsNested
  FileActionsNestedSanitized FileActionsState FileActionsStateSanitized
  FileActionsStateUi FileActionsStateUiSanitized PROPERTIES
  RUN_SERIAL TRUE TIMEOUT 240)
