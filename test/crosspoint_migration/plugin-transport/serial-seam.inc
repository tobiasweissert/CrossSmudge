// Only serial buffers, output sink and Home-screen admission are doubles.
// handleLine() itself is extracted verbatim from production.
char lineBuffer[128] = {};
size_t lineBufferPos = 0;
bool fileTransferAllowed = false;
bool ensureFileTransferAllowed() { return fileTransferAllowed; }
enum class ProcessResult { None, ScreenshotRequested };
struct SerialSink {
  std::vector<std::string> records;
  void printf(const char* format, const char* record) {
    assert(std::string(format) == "%s\n");
    records.emplace_back(record);
  }
} logSerial;
ProcessResult handleLine();
static void serialCommand(const char* command) {
  lineBufferPos = strlen(command);
  assert(lineBufferPos < sizeof(lineBuffer));
  memcpy(lineBuffer, command, lineBufferPos);
  assert(handleLine() == ProcessResult::None);
}
