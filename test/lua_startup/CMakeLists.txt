find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME lua_api3_startup
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run.py)
add_test(NAME lua_api3_startup_sanitize
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run.py --sanitize)
set_tests_properties(lua_api3_startup_sanitize PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
