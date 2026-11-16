find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME lua_device_identity
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_device_identity.py)
