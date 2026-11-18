void runStateUiCases() {
  using namespace files_fixture;
  auto reset = [](bool fail) {
    io = {};
    io.files["/.crosspoint/state.json"] =
        R"({"openEpubPath":"/Books/Original.png","favoriteSleepImagePath":"/Books/Original.png","favoriteBootImagePath":"/Books/Original.png","readerActivityLoadCount":27,"preferredSleepFolderPath":"/wallpapers"})";
    assert(CrossPointState::getInstance().loadFromFile());
    io.failAnyWriteOpen = fail;
    files_fixture::APP_STATE.openEpubPath = "/Books/Original.png";
    files_fixture::APP_STATE.favoriteSleepImagePath = "/Books/Original.png";
    files_fixture::APP_STATE.favoriteBootImagePath = "/Books/Original.png";
    files_fixture::APP_STATE.hasPendingAlert = false;
    files_fixture::APP_STATE.pendingAlertBody[0] = '\0';
    files_fixture::Storage.paths = {{"/Books", true}, {"/Target", true}, {"/Books/Original.png", false}};
    FileActionPaths::mutationBlocked = false;
  };
  for (bool fail : {true, false}) {
    reset(fail);
    FileBrowserActivity browser;
    browser.promptDeleteFile("/Books/Original.png", "Original.png");
    browser.callback(ActivityResult{});
    assert(!files_fixture::Storage.exists("/Books/Original.png"));
    assert(files_fixture::APP_STATE.hasPendingAlert == fail);
    if (fail) {
      assert(io.files.at("/.crosspoint/state.json").find("Original.png") != std::string::npos);
      assert(std::string(files_fixture::APP_STATE.pendingAlertBody).find("deleted") != std::string::npos);
      assert(std::string(files_fixture::APP_STATE.pendingAlertBody).find("saved") != std::string::npos);
    } else {
      assert(CrossPointState::getInstance().loadFromFile());
      auto& s = CrossPointState::getInstance();
      assert(s.openEpubPath.empty() && s.favoriteSleepImagePath.empty() && s.favoriteBootImagePath.empty());
      assert(s.readerActivityLoadCount == 27 && s.preferredSleepFolderPath == "/wallpapers");
    }
    std::cout << "PASS state UI Delete " << (fail ? "cleanup failure visible" : "healthy reference cleanup") << '\n';
  }
  for (bool fail : {true, false}) {
    reset(false);
    io.files["/.crosspoint/state.json"] =
        R"({"openEpubPath":"/Books/Original.epub","favoriteSleepImagePath":"/sleep.png","favoriteBootImagePath":"/boot.bmp","readerActivityLoadCount":27})";
    assert(CrossPointState::getInstance().loadFromFile());
    files_fixture::APP_STATE.openEpubPath = "/Books/Original.epub";
    files_fixture::APP_STATE.favoriteSleepImagePath = "/sleep.png";
    files_fixture::APP_STATE.favoriteBootImagePath = "/boot.bmp";
    files_fixture::Storage.paths = {{"/Books", true}, {"/Books/Original.epub", false}};
    io.failAnyWriteOpen = fail;
    FileBrowserActivity browser;
    browser.promptDeleteFile("/Books/Original.epub", "Original.epub");
    browser.callback(ActivityResult{});
    assert(!files_fixture::Storage.exists("/Books/Original.epub"));
    assert(files_fixture::APP_STATE.hasPendingAlert == fail);
    if (!fail) {
      assert(CrossPointState::getInstance().loadFromFile());
      auto& s = CrossPointState::getInstance();
      assert(s.openEpubPath.empty() && s.favoriteSleepImagePath == "/sleep.png" &&
             s.favoriteBootImagePath == "/boot.bmp" && s.readerActivityLoadCount == 27);
    }
    std::cout << "PASS state UI current EPUB Delete "
              << (fail ? "cleanup failure visible" : "healthy open reference cleanup") << '\n';
  }
  for (bool fail : {true, false}) {
    reset(fail);
    FileBrowserActivity browser;
    browser.moveFile("/Books/Original.png", "Original.png", "/Target");
    assert(!files_fixture::Storage.exists("/Books/Original.png"));
    assert(files_fixture::Storage.exists("/Target/Original.png"));
    assert(files_fixture::APP_STATE.hasPendingAlert == fail);
    if (fail) {
      assert(io.files.at("/.crosspoint/state.json").find("Original.png") != std::string::npos);
      assert(std::string(files_fixture::APP_STATE.pendingAlertBody).find("moved") != std::string::npos);
      assert(std::string(files_fixture::APP_STATE.pendingAlertBody).find("saved") != std::string::npos);
    } else {
      assert(CrossPointState::getInstance().loadFromFile());
      auto& s = CrossPointState::getInstance();
      assert(s.favoriteSleepImagePath == "/Target/Original.png" && s.favoriteBootImagePath == "/Target/Original.png");
      assert(s.readerActivityLoadCount == 27 && s.preferredSleepFolderPath == "/wallpapers");
    }
    std::cout << "PASS state UI favorite Move "
              << (fail ? "persist failure visible without rollback" : "healthy reference update") << '\n';
  }
}
int main() {
  std::cout << std::unitbuf;
  runStateUiCases();
}
