add_executable(SdCardFontTest
  SdCardFontTest.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFont.cpp
  ${REPO_ROOT}/lib/EpdFont/SdCardFont.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)
target_include_directories(SdCardFontTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/lib/EpdFont
  ${REPO_ROOT}/lib/Utf8
  ${REPO_ROOT}/lib/Memory
)
target_include_directories(SdCardFontTest PRIVATE ${REPO_ROOT}/lib/MemoryBudget ${REPO_ROOT}/lib/Memory)
target_include_directories(SdCardFontTest PRIVATE ${REPO_ROOT}/test/chapter_html_slim_parser/stubs)
target_link_libraries(SdCardFontTest PRIVATE crosspoint_test_common GTest::gtest_main)
target_include_directories(SdCardFontTest PRIVATE ${REPO_ROOT}/freeink-sdk/libs/font/FreeInkFont/include)
target_sources(SdCardFontTest PRIVATE ${REPO_ROOT}/freeink-sdk/libs/font/FreeInkFont/src/FontAlloc.c)
target_compile_options(SdCardFontTest PRIVATE -fno-exceptions)
target_link_options(SdCardFontTest PRIVATE -Wl,--wrap=malloc)
gtest_discover_tests(SdCardFontTest)
