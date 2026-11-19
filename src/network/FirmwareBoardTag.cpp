#include "FirmwareBoardTag.h"

#include <BoardConfig.h>

#include <cstring>

// Retain the established Sticky transport/recovery board tag. Include every
// competing profile normalized by the pinned upstream SDK, including aliases.
#if !FREEINK_DEVICE_STICKY || FREEINK_DEVICE_X3 || FREEINK_DEVICE_X4 || FREEINK_DEVICE_X4PRO ||               \
    FREEINK_DEVICE_X4CLASSIC || FREEINK_DEVICE_WS397 || FREEINK_DEVICE_ONEPAGE || FREEINK_DEVICE_PAPERMONO || \
    FREEINK_DEVICE_EEGO_A4 || FREEINK_DEVICE_M5 || FREEINK_DEVICE_MURPHY_M4 || FREEINK_DEVICE_MURPHY ||       \
    FREEINK_DEVICE_DELINK || FREEINK_DEVICE_LILYGO || FREEINK_DEVICE_M5PAPER || FREEINK_DEVICE_PAPERS3 ||    \
    FREEINK_DEVICE_SILKSCREEN || FREEINK_DEVICE_METALIO_EINK4 || FREEINK_DEVICE_PICCO
#error "Crosscub requires the Sticky board tag exclusively"
#endif
#define CROSSPOINT_BOARD_NAME "sticky"

namespace board_tag {

namespace {
// The magic's first character must not recur inside it — the scanner restarts
// a failed match with a single-byte lookback, which is only exact then.
constexpr size_t MAGIC_LEN = sizeof("CROSSPOINT-BOARD-V1:") - 1;
}  // namespace

const char TAG[] = "CROSSPOINT-BOARD-V1:" CROSSPOINT_BOARD_NAME ";";

const char* boardName() { return TAG + MAGIC_LEN; }
size_t boardNameLen() { return sizeof(TAG) - 1 - MAGIC_LEN - 1; }  // strip magic and ';'

void Scanner::feed(const uint8_t* data, size_t len) {
  if (mismatchFound) return;
  for (size_t i = 0; i < len; i++) {
    const char c = static_cast<char>(data[i]);
    if (capturing) {
      if (c == ';') {
        capturing = false;
        captured[nameLen] = '\0';
        if (nameLen != boardNameLen() || std::memcmp(captured, boardName(), nameLen) != 0) {
          mismatchFound = true;
          return;
        }
      } else if (nameLen < MAX_NAME && c > 0x20 && c < 0x7F) {
        captured[nameLen++] = c;
      } else {
        // Overlong or non-printable: a chance byte collision, not a real tag.
        capturing = false;
      }
      continue;
    }
    if (c == TAG[magicMatched]) {
      if (++magicMatched == MAGIC_LEN) {
        magicMatched = 0;
        capturing = true;
        nameLen = 0;
      }
    } else {
      magicMatched = (c == TAG[0]) ? 1 : 0;
    }
  }
}

}  // namespace board_tag
