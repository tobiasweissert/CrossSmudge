#pragma once
inline constexpr int STR_EPUB_CHAPTER_INCOMPLETE_TITLE = 0;
inline constexpr int STR_EPUB_CHAPTER_INCOMPLETE_BODY = 1;
inline const char* tr(int key) {
  return key == 0 ? "Chapter incomplete" : "This chapter contains malformed markup. Some text could not be displayed.";
}
