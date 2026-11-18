int main() {
  {
    FileBrowserActivity oom;
    failActivityAllocation = true;
    oom.startMoveFile("/Books/A.epub", "A.epub");
    assert(!oom.child && APP_STATE.hasPendingAlert);
    APP_STATE.hasPendingAlert = false;
    oom.promptDeleteFile("/Books/A.epub", "A.epub");
    assert(!oom.child && APP_STATE.hasPendingAlert && events.empty());
    failActivityAllocation = false;
    APP_STATE.hasPendingAlert = false;
    std::cout << "PASS activity allocation failures preserve files and show an error\n";
  }

  FileBrowserActivity browser;
  browser.promptDeleteFile("/Books/Original.epub", "Original.epub");
  Storage.paths = {{"/Books/A.epub", false}, {"/Books/B.epub", false}, {"/Books/Original.epub", false}};
  ActivityResult cancelled;
  cancelled.isCancelled = true;
  browser.callback(cancelled);
  assert(events.empty());
  Storage.removeOk = false;
  browser.callback(ActivityResult{});
  assert(events == std::vector<std::string>{"remove"} && "failed delete must preserve metadata and progress");
  assert(APP_STATE.hasPendingAlert && "failed delete needs a visible error");
  std::cout << "PASS failed-delete preserves metadata\n";
  events.clear();
  Storage.removeOk = true;
  browser.callback(ActivityResult{});
  assert(APP_STATE.openEpubPath.empty() && "deleted book must not remain the open book");
  assert(std::find(events.begin(), events.end(), "recent") != events.end());
  assert(events.front() == "remove" && events.back() == "refresh");
  assert(browser.selectorIndex == 1);
  browser.mappedInput.held.insert(MappedInputManager::Button::Confirm);
  browser.mappedInput.heldTime = 1200;
  browser.loop();
  assert(!dynamic_cast<FileBrowserActionActivity*>(browser.child.get()) &&
         "post-delete held input must not target the replacement row before paint");
  browser.mappedInput.held.clear();
  browser.mappedInput.heldTime = 0;
  std::cout << "PASS successful-delete synchronizes references and list\n";
  for (const auto& ext : std::vector<std::string>{".xtc", ".txt", ".md"}) {
    const auto path = "/Books/Delete" + ext;
    Storage.paths = {{"/cache" + path, true}};
    events.clear();
    BookActions::clearFileMetadata(path);
    assert(!Storage.paths.count("/cache" + path) && "successful delete must clear all path-keyed format caches");
  }
  std::cout << "PASS deleted file format caches are removed\n";
  {
    FileBrowserActivity cleanup;
    Storage.paths = {{"/Books/Partial.epub", false}, {"/cache/Books/Partial.epub", true}};
    cleanup.promptDeleteFile("/Books/Partial.epub", "Partial.epub");
    Epub::cacheOk = false;
    APP_STATE.hasPendingAlert = false;
    cleanup.callback(ActivityResult{});
    Epub::cacheOk = true;
    assert(!Storage.paths.count("/Books/Partial.epub") && Storage.paths.count("/cache/Books/Partial.epub") &&
           APP_STATE.hasPendingAlert && "post-delete cleanup failure must be visible separately");
  }
  browser.showFileActionMenu("Original.epub");
  auto* menu = dynamic_cast<FileBrowserActionActivity*>(browser.child.get());
  assert(menu);
  assert(std::any_of(menu->items.begin(), menu->items.end(), [](const auto& i) { return int(i.action) == 19; }) &&
         "file menu must expose Move");
  std::cout << "PASS file menu exposes Move\n";
  auto chooseMove = browser.callback;
  chooseMove(ActivityResult{FileBrowserActionResult{19}});
  auto* picker = dynamic_cast<FileBrowserActivity*>(browser.child.get());
  assert(picker && picker->mode == FileBrowserActivity::Mode::PickDirectory && "Move must launch directory selection");
  std::cout << "PASS Move launches directory picker\n";
  picker->files.clear();
  picker->render(Activity::RenderLock(*picker));
  assert(TouchHeaderBackButton::title == "Move to folder" && "Move picker must not be titled receive-folder selection");
  const std::string raw = "Re\xcc\x81sume.MiXeD.png";
  const std::string src = "/Books/" + raw, dst = "/Target/" + raw;
  Storage.paths = {{"/Books", true}, {"/Target", true}, {src, false}};
  browser.startMoveFile(src, raw);
  events.clear();
  auto destination = browser.callback;
  destination(cancelled);
  assert(events.empty());
  destination(ActivityResult{FilePathResult{"/Target"}});
  assert(Storage.paths.count(dst) && !Storage.paths.count(src) &&
         "selected directory must move exact original filename");
  assert(browser.files.empty() && browser.selectorIndex == 0);
  std::cout << "PASS Move exact bytes and empty source-list refresh\n";
  for (const auto& target :
       std::vector<std::string>{"/Books", "/Target", "/Missing", "/Target/../Other", "/.crosspoint", "relative", ""}) {
    Storage.paths = {{"/Books", true}, {"/Target", true}, {src, false}, {dst, false}};
    Storage.moves.clear();
    events.clear();
    APP_STATE.hasPendingAlert = false;
    browser.moveFile(src, raw, target);
    assert(Storage.moves.empty() && Storage.paths.count(src) && Storage.paths.count(dst) &&
           "no-op/collision/invalid directory must not invoke rename");
    if (target == "/Target")
      assert(APP_STATE.hasPendingAlert &&
             std::string(APP_STATE.pendingAlertBody).find("already exists") != std::string::npos &&
             "collision must be explicit");
  }
  std::cout << "PASS Move admission no-op collision invalid directory\n";
  for (const auto& ext : std::vector<std::string>{".epub", ".xtc", ".txt", ".md"}) {
    const auto old = "/Books/Novel" + ext, next = "/Target/Novel" + ext;
    Storage.paths = {{"/Books", true}, {"/Target", true}, {old, false}, {"/cache" + old, true}};
    APP_STATE.openEpubPath = old;
    events.clear();
    browser.moveFile(old, "Novel" + ext, "/Target");
    assert(Storage.paths.count(next) && Storage.paths.count("/cache" + next) && !Storage.paths.count("/cache" + old));
    assert(APP_STATE.openEpubPath == next);
    assert(std::find(events.begin(), events.end(), "recent-move") != events.end());
  }
  std::cout << "PASS book Move preserves path-keyed state for all formats\n";
  {
    Storage.paths = {
        {"/Books/Conflict.epub", false}, {"/cache/Books/Conflict.epub", true}, {"/cache/Target/Conflict.epub", true}};
    Storage.moves.clear();
    events.clear();
    const auto conflict = BookMoveUtils::migrateRenamedBookState(
        "/Books/Conflict.epub", "/Target/Conflict.epub", "/cache/Books/Conflict.epub", "Title", "Author", "epub", true);
    assert(conflict == BookMoveUtils::RenameMigrationResult::RolledBack && events.empty() && Storage.moves.empty() &&
           "preexisting destination state must be refused before preparing metadata");
  }
  std::cout << "PASS destination cache conflict refuses all mutation\n";
  for (bool landscape : {false, true}) {
    FileBrowserActivity folder;
    folder.mode = FileBrowserActivity::Mode::PickDirectory;
    folder.files.clear();
    folder.basepath = "/Empty";
    folder.renderer.w = landscape ? 800 : 480;
    folder.renderer.h = landscape ? 480 : 800;
    UiApp::ScreenType screen(folder.renderer.w, folder.renderer.h);
    folder.buildListScreen(screen);
    assert(std::any_of(screen.buttons.begin(), screen.buttons.end(),
                       [](const auto& b) { return b.props.label && std::string(b.props.label) == "Select folder"; }) &&
           "empty destination folder must have a visible touch acceptance button");
  }
  std::cout << "PASS empty picker Select folder is visible both orientations\n";
  {
    FileBrowserActivity folder;
    folder.mode = FileBrowserActivity::Mode::PickDirectory;
    folder.files.clear();
    folder.basepath = "/Empty";
    folder.mappedInput.released.insert(MappedInputManager::Button::Right);
    folder.loop();
    assert(!folder.finished && "queued Right release before picker paint is not destination consent");
  }
  std::cout << "PASS picker rejects unpainted hardware acceptance\n";
  for (bool landscape : {false, true}) {
    FileBrowserActivity folder;
    folder.mode = FileBrowserActivity::Mode::PickDirectory;
    folder.files.clear();
    folder.basepath = "/Empty";
    folder.renderer.w = landscape ? 800 : 480;
    folder.renderer.h = landscape ? 480 : 800;
    folder.registerActions();
    UiApp::ScreenType screen(folder.renderer.w, folder.renderer.h);
    folder.buildListScreen(screen);
    folder.app.buttons = screen.buttons;
    folder.render(Activity::RenderLock(folder));
    folder.loop();
    folder.loop();
    const auto& box = screen.buttons.back().rect;
    assert(box.height >= 48 && box.x >= 0 && box.y >= 0 && box.x + box.width <= folder.renderer.w &&
           box.y + box.height <= folder.renderer.h);
    folder.mappedInput.down = true;
    folder.mappedInput.contact = true;
    folder.mappedInput.x = box.x + box.width / 2;
    folder.mappedInput.y = box.y + box.height / 2;
    folder.loop();
    folder.mappedInput.frame();
    folder.mappedInput.contact = false;
    folder.mappedInput.tap = true;
    folder.loop();
    assert(folder.finished && !folder.result.isCancelled &&
           std::get<FilePathResult>(folder.result.data).path == "/Empty" &&
           "fresh touch Select folder must return current empty directory");
  }
  std::cout << "PASS fresh touch destination acceptance both orientations\n";
  for (bool landscape : {false, true}) {
    FileBrowserActivity list;
    list.files = {"First.epub", "Selected.png"};
    list.selectorIndex = 1;
    list.renderer.w = landscape ? 800 : 480;
    list.renderer.h = landscape ? 480 : 800;
    list.registerActions();
    UiApp::ScreenType screen(list.renderer.w, list.renderer.h);
    list.buildListScreen(screen);
    const auto action = std::find_if(screen.buttons.begin(), screen.buttons.end(), [](const auto& b) {
      return b.props.label && std::string(b.props.label).find("Actions") == 0;
    });
    assert(action != screen.buttons.end() && "selected file needs visible Actions route");
    assert(list.selectorIndex == 1);
    list.app.buttons = screen.buttons;
    list.render(Activity::RenderLock(list));
    list.mappedInput.x = action->rect.x + action->rect.width / 2;
    list.mappedInput.y = action->rect.y + action->rect.height / 2;
    list.mappedInput.down = true;
    list.loop();
    list.mappedInput.frame();
    list.mappedInput.tap = true;
    list.loop();
    assert(dynamic_cast<FileBrowserActionActivity*>(list.child.get()) && list.selectorIndex == 1);
  }
  std::cout << "PASS visible Actions routes selected file in both orientations\n";
  for (bool landscape : {false, true}) {
    for (const auto& path : std::vector<std::string>{"/", "/Books", "/Books/Empty"}) {
      FileBrowserActivity folder;
      folder.mode = FileBrowserActivity::Mode::PickDirectory;
      folder.files.clear();
      folder.basepath = path;
      folder.renderer.w = landscape ? 800 : 480;
      folder.renderer.h = landscape ? 480 : 800;
      folder.render(Activity::RenderLock(folder));
      folder.loop();
      folder.loop();
      folder.mappedInput.pressed.insert(MappedInputManager::Button::Right);
      folder.loop();
      folder.mappedInput.frame();
      folder.mappedInput.released.insert(MappedInputManager::Button::Right);
      folder.loop();
      assert(folder.finished && !folder.result.isCancelled &&
             std::get<FilePathResult>(folder.result.data).path == path);
    }
    FileBrowserActivity descent;
    descent.mode = FileBrowserActivity::Mode::PickDirectory;
    descent.basepath = "/Books";
    descent.files = {"Child/"};
    descent.selectorIndex = 0;
    Storage.paths = {{"/Books", true}, {"/Books/Child", true}};
    descent.render(Activity::RenderLock(descent));
    descent.loop();
    descent.loop();
    descent.mappedInput.pressed.insert(MappedInputManager::Button::Confirm);
    descent.loop();
    descent.mappedInput.frame();
    descent.mappedInput.released.insert(MappedInputManager::Button::Confirm);
    descent.loop();
    descent.mappedInput.frame();
    assert(descent.basepath == "/Books/Child" && !descent.finished);
    descent.mappedInput.pressed.insert(MappedInputManager::Button::Right);
    descent.loop();
    descent.mappedInput.frame();
    descent.mappedInput.released.insert(MappedInputManager::Button::Right);
    descent.loop();
    assert(!descent.finished && "directory navigation invalidates destination consent until the new path is painted");
  }
  std::cout << "PASS picker hardware root nested empty and descent consent\n";
  for (bool landscape : {false, true}) {
    FileBrowserActivity touch;
    touch.files = {"Photo.png", "Book.epub"};
    touch.selectorIndex = 0;
    touch.renderer.w = landscape ? 800 : 480;
    touch.renderer.h = landscape ? 480 : 800;
    FileBrowserActivity::onRowEvent(fui::ActionEvent{0, false}, &touch);
    assert(touch.openedPath == "/Books/Photo.png");
    FileBrowserActivity::onRowEvent(fui::ActionEvent{1, true}, &touch);
    assert(dynamic_cast<FileBrowserActionActivity*>(touch.child.get()) && touch.selectorIndex == 1);
    FileBrowserActivity hardware;
    hardware.files = {"Book.epub"};
    hardware.selectorIndex = 0;
    hardware.mappedInput.held.insert(MappedInputManager::Button::Confirm);
    hardware.mappedInput.heldTime = 1100;
    hardware.loop();
    assert(dynamic_cast<FileBrowserActionActivity*>(hardware.child.get()));
    FileBrowserActivity cancel;
    cancel.mode = FileBrowserActivity::Mode::PickDirectory;
    cancel.basepath = "/";
    cancel.files.clear();
    cancel.render(Activity::RenderLock(cancel));
    cancel.loop();
    cancel.loop();
    cancel.mappedInput.pressed.insert(MappedInputManager::Button::Back);
    cancel.loop();
    cancel.mappedInput.frame();
    cancel.mappedInput.released.insert(MappedInputManager::Button::Back);
    cancel.loop();
    assert(cancel.finished && cancel.result.isCancelled);
  }
  std::cout << "PASS touch long-press hardware hold ordinary photo and picker Back\n";
  {
    FileBrowserActivity directories;
    directories.basepath = "/";
    directories.showDirectoryActionMenu("Inbox/");
    const auto* menu = dynamic_cast<FileBrowserActionActivity*>(directories.child.get());
    assert(menu);
    assert(std::none_of(menu->items.begin(), menu->items.end(),
                        [](const auto& i) { return i.action == FileBrowserAction::Delete; }) &&
           "file-only mutation scope must not recursively delete hidden Inbox journals");
  }
  {
    Storage.paths = {{"/Books/Recent.epub", false}, {"/cache/Books/Recent.epub", true}};
    Storage.moves.clear();
    Storage.renames = 0;
    Storage.renameFailure = 0;
    RECENT_BOOKS.updateOk = false;
    events.clear();
    const auto result = BookMoveUtils::migrateRenamedBookState(
        "/Books/Recent.epub", "/Target/Recent.epub", "/cache/Books/Recent.epub", "Title", "Author", "epub", true);
    RECENT_BOOKS.updateOk = true;
    assert(result == BookMoveUtils::RenameMigrationResult::Uncertain && Storage.moves.size() == 2 &&
           Storage.paths.count("/Target/Recent.epub") && Storage.paths.count("/cache/Target/Recent.epub") &&
           "uncertain reference persistence must retain published file and cache without speculative rollback");
  }
  std::cout << "PASS post-publication persistence failure preserves candidates\n";
  {
    Storage.paths = {{"/Books/Open.epub", false}, {"/cache/Books/Open.epub", true}};
    Storage.moves.clear();
    Storage.renames = 0;
    APP_STATE.openEpubPath = "/Books/Open.epub";
    APP_STATE.saveOk = false;
    events.clear();
    const auto result = BookMoveUtils::migrateRenamedBookState(
        "/Books/Open.epub", "/Target/Open.epub", "/cache/Books/Open.epub", "Title", "Author", "epub", true);
    APP_STATE.saveOk = true;
    assert(result == BookMoveUtils::RenameMigrationResult::Uncertain && Storage.moves.size() == 2 &&
           Storage.paths.count("/Target/Open.epub") && Storage.paths.count("/cache/Target/Open.epub") &&
           "uncertain open-path save must not initiate rollback");
  }
  std::cout << "PASS open-path persistence failure preserves published state\n";
  {
    Storage.paths = {{"/Books/Commit.epub", false}, {"/cache/Books/Commit.epub", true}};
    Storage.moves.clear();
    Storage.renames = 0;
    events.clear();
    BookmarkStore::commitOk = false;
    const auto result = BookMoveUtils::migrateRenamedBookState(
        "/Books/Commit.epub", "/Target/Commit.epub", "/cache/Books/Commit.epub", "Title", "Author", "epub", true);
    BookmarkStore::commitOk = true;
    assert(result == BookMoveUtils::RenameMigrationResult::Uncertain && Storage.paths.count("/Target/Commit.epub") &&
           Storage.paths.count("/cache/Target/Commit.epub") && "metadata cleanup failure cannot report full success");
  }
  std::cout << "PASS metadata commit failure is explicit\n";
  for (int failure : {1, 2})
    for (int effect : {0, 1, 2}) {
      FileActionPaths::mutationBlocked = false;
      Storage.paths = {
          {"/Books", true}, {"/Target", true}, {"/Books/Alias.epub", false}, {"/cache/Books/Alias.epub", true}};
      Storage.moves.clear();
      Storage.renames = 0;
      Storage.renameFailure = failure;
      Storage.renameFailureEffect = effect;
      events.clear();
      browser.moveFile("/Books/Alias.epub", "Alias.epub", "/Target");
      assert(FileActionPaths::mutationBlocked && Storage.moves.size() == size_t(failure));
      assert(Storage.paths.count("/Books/Alias.epub") || Storage.paths.count("/Target/Alias.epub"));
      assert(Storage.paths.count("/cache/Books/Alias.epub") || Storage.paths.count("/cache/Target/Alias.epub"));
      assert(std::find(events.begin(), events.end(), "bookmark-rollback") == events.end());
    }
  Storage.renameFailureEffect = 0;
  Storage.renameFailure = 0;
  for (int failure : {1, 2}) {
    Storage.paths = {{"/Books/Prepared.epub", false}, {"/cache/Books/Prepared.epub", true}};
    Storage.moves.clear();
    BookmarkStore::beginOk = failure != 1;
    ClippingStore::beginOk = failure != 2;
    const auto result = BookMoveUtils::migrateRenamedBookState(
        "/Books/Prepared.epub", "/Target/Prepared.epub", "/cache/Books/Prepared.epub", "Title", "Author", "epub", true);
    BookmarkStore::beginOk = ClippingStore::beginOk = true;
    assert(result == BookMoveUtils::RenameMigrationResult::RolledBack && Storage.moves.empty() &&
           Storage.paths.count("/Books/Prepared.epub") && Storage.paths.count("/cache/Books/Prepared.epub"));
  }
  std::cout << "PASS fault matrix cache/file negative acknowledgement none alias moved and preparation failure\n";
  FileActionPaths::mutationBlocked = false;
  for (const auto& path : std::vector<std::string>{"/.crosspoint/state.bin", "/Books/.private.txt", "/Books/folder"}) {
    FileBrowserActivity denied;
    Storage.paths = {{path, path == "/Books/folder"}};
    events.clear();
    denied.promptDeleteFile(path, path.substr(path.rfind('/') + 1));
    if (denied.callback) denied.callback(ActivityResult{});
    assert(events.empty() && Storage.paths.count(path) && "Delete must not mutate hidden state or directories");
  }
  std::cout << "PASS Delete excludes hidden state and directory mutation\n";
  Storage.paths = {
      {"/Books", true}, {"/Target", true}, {"/Books/Fault.epub", false}, {"/cache/Books/Fault.epub", true}};
  Storage.moves.clear();
  Storage.renames = 0;
  Storage.renameFailure = 2;
  events.clear();
  browser.moveFile("/Books/Fault.epub", "Fault.epub", "/Target");
  assert(Storage.moves.size() == 2 && "unacknowledged book rename must not trigger destructive rollback renames");
  assert(std::find(events.begin(), events.end(), "bookmark-rollback") == events.end());
  assert(std::find(events.begin(), events.end(), "clipping-rollback") == events.end());
  std::cout << "PASS uncertain rename retains candidates without rollback\n";
  const auto attempts = Storage.moves.size();
  browser.moveFile("/Books/Fault.epub", "Fault.epub", "/Target");
  assert(Storage.moves.size() == attempts && "uncertain Move must latch further file mutations");
  assert(APP_STATE.hasPendingAlert && std::string(APP_STATE.pendingAlertBody).find("Move uncertain") == 0);
  std::cout << "PASS uncertainty is visible and disables mutation retries\n";
  {
    FileBrowserActivity blocked;
    blocked.showFileActionMenu("Fault.epub");
    assert(!blocked.child && "uncertain state must block destructive context actions, not only Move retries");
  }
}
