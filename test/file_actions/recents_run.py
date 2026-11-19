#!/usr/bin/env python3
"""Compile exact Recents delete methods with explicit storage/UI boundaries.

This caller-order regression deliberately makes metadata cleanup destructive.
It does not emulate SD/FAT or claim coverage of the shared cleanup internals.
Run with --sanitize for AddressSanitizer + UndefinedBehaviorSanitizer.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CALLERS = ("RecentBooksActivity", "RecentBooksGridActivity")


def production_method(caller):
    text = (ROOT / "src/activities/home" / (caller + ".cpp")).read_text()
    signature = "void " + caller + "::promptDeleteBook("
    if text.count(signature) != 1:
        raise RuntimeError("Expected exactly one production method: " + signature)
    start = text.index(signature)
    # Production top-level closing braces are unindented; nested lambda braces
    # remain indented. Copy the entire method verbatim, including confirmation.
    end = text.index("\n}", start) + 2
    return text[start:end]


PREAMBLE = r'''
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
int errors = 0;
#define LOG_ERR(...) (++errors)
constexpr int STR_DELETE = 0;
const char* tr(int) { return "Delete"; }
struct ActivityResult { bool isCancelled = false; };
struct RecentBook { std::string path, title; };
std::vector<std::string> events;
struct StorageBoundary {
  std::set<std::string> files, progress, bookmarks, clippings;
  bool removeSucceeds = true, cleanupSucceeds = true;
  bool intactAtRemove = false;
  int removeCalls = 0, cleanupCalls = 0;
  bool remove(const char* path) {
    ++removeCalls;
    events.push_back("remove");
    intactAtRemove = files.count(path) && progress.count(path) &&
                     bookmarks.count(path) && clippings.count(path);
    if (!removeSucceeds) return false;
    return files.erase(path) != 0;
  }
} Storage;
namespace BookActions {
// Explicit boundary double: successful cleanup erases all persisted reading
// state; injected cleanup failure leaves progress behind, not the deleted book.
bool clearFileMetadata(const std::string& path) {
  ++Storage.cleanupCalls;
  events.push_back("cleanup");
  if (Storage.cleanupSucceeds) Storage.progress.erase(path);
  Storage.bookmarks.erase(path);
  Storage.clippings.erase(path);
  return Storage.cleanupSucceeds;
}
}
struct RecentsBoundary {
  std::set<std::string> paths;
  int removeCalls = 0;
  bool removeByPath(const std::string& path) {
    ++removeCalls;
    events.push_back("recents");
    return paths.erase(path) != 0;
  }
} RECENT_BOOKS;
struct ConfirmationActivity {
  std::string heading, title;
  ConfirmationActivity(int&, int&, const std::string& h, const std::string& t)
      : heading(h), title(t) {}
};
'''

CLASS = r'''
class CALLER {
 public:
  int renderer = 0, mappedInput = 0, reloadCalls = 0, confirmations = 0;
  std::string heading, title;
  std::function<void(const ActivityResult&)> callback;
  void promptDeleteBook(const RecentBook&);
  void reloadAfterBookAction() { ++reloadCalls; events.push_back("reload"); }
  void startActivityForResult(std::unique_ptr<ConfirmationActivity> activity,
                             std::function<void(const ActivityResult&)> handler) {
    ++confirmations;
    heading = activity->heading;
    title = activity->title;
    callback = std::move(handler);
  }
};
'''

TEST = r'''
int failures = 0;
void check(bool ok, const std::string& label, const char* requirement) {
  if (!ok) { ++failures; std::cerr << "FAIL " << label << ": " << requirement << '\n'; }
}
template<class Caller>
void run(const std::string& caller, const std::string& extension, const std::string& outcome) {
  const std::string label = "recents " + caller + " " + extension + " " + outcome;
  const int before = failures;
  const std::string path = "/Books/Keep" + extension;
  const std::string unrelated = "/Books/Other" + extension;
  Storage = StorageBoundary{};
  RECENT_BOOKS = RecentsBoundary{};
  errors = 0;
  events.clear();
  Storage.files = Storage.progress = Storage.bookmarks = Storage.clippings = {path, unrelated};
  RECENT_BOOKS.paths = {path, unrelated};
  Storage.removeSucceeds = outcome != "remove-failure";
  Storage.cleanupSucceeds = outcome != "cleanup-failure-book-already-deleted";
  Caller activity;
  {
    RecentBook book{path, "Keep"};
    activity.promptDeleteBook(book);
    book.path = "/Books/Changed";  // handler must retain the original path by value
  }
  check(activity.confirmations == 1 && activity.heading == "Delete? " && activity.title == "Keep",
        label, "existing confirmation heading/title retained");
  check(events.empty() && Storage.files.count(path) && Storage.progress.count(path),
        label, "opening confirmation does not mutate storage");
  check(bool(activity.callback), label, "confirmation callback installed");
  if (!activity.callback) return;
  activity.callback({outcome == "cancel"});
  const bool deleted = outcome == "success" || outcome == "cleanup-failure-book-already-deleted";
  check(Storage.files.count(path) == (deleted ? 0u : 1u), label, "book deletion matches acknowledged removal");
  check(Storage.progress.count(path) == (outcome == "success" ? 0u : 1u), label,
        "progress retained on cancel/remove failure; cleanup failure cannot restore deleted book");
  check(Storage.bookmarks.count(path) == (deleted ? 0u : 1u) &&
        Storage.clippings.count(path) == (deleted ? 0u : 1u), label,
        "metadata never erased on cancel/remove failure");
  check(RECENT_BOOKS.paths.count(path) == (deleted ? 0u : 1u), label, "recents match actual book removal");
  check(activity.reloadCalls == (deleted ? 1 : 0) && RECENT_BOOKS.removeCalls == (deleted ? 1 : 0),
        label, "successful delete refreshes recents exactly once even if cleanup fails");
  check(Storage.cleanupCalls == (deleted ? 1 : 0), label, "cleanup only after successful book removal");
  check(Storage.removeCalls == (outcome == "cancel" ? 0 : 1), label, "cancel never attempts removal");
  check(errors == (outcome == "remove-failure" ? 1 : 0), label, "existing removal-failure logging retained");
  if (outcome != "cancel") check(Storage.intactAtRemove, label, "book and all metadata intact at removal boundary");
  const std::vector<std::string> expected = deleted
      ? std::vector<std::string>{"remove", "cleanup", "recents", "reload"}
      : (outcome == "cancel" ? std::vector<std::string>{} : std::vector<std::string>{"remove"});
  check(events == expected, label, "remove precedes cleanup/recents/reload; failed remove stops immediately");
  check(Storage.files.count(unrelated) && Storage.progress.count(unrelated) &&
        Storage.bookmarks.count(unrelated) && Storage.clippings.count(unrelated) &&
        RECENT_BOOKS.paths.count(unrelated), label, "unrelated book/state untouched");
  if (failures == before) std::cout << "PASS " << label << '\n';
}
int main() {
  for (const auto& extension : {".epub", ".txt", ".md", ".xtc"}) {
    for (const auto& outcome : {"remove-failure", "success", "cancel", "cleanup-failure-book-already-deleted"}) {
      run<RecentBooksActivity>("list", extension, outcome);
      run<RecentBooksGridActivity>("grid", extension, outcome);
    }
  }
  return failures ? 1 : 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    source = PREAMBLE
    for caller in CALLERS:
        source += CLASS.replace("CALLER", caller) + production_method(caller) + "\n"
    source += TEST
    with tempfile.TemporaryDirectory(prefix="recents-actions-") as temporary:
        cpp = Path(temporary) / "recents.cpp"
        executable = Path(temporary) / "recents"
        cpp.write_text(source)
        command = shlex.split(os.environ.get("CXX", "g++")) + [
            "-std=c++17", "-Wall", "-Wextra", "-Werror", "-O1", "-g"]
        if args.sanitize:
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-pie", "-no-pie"]
        subprocess.run(command + [str(cpp), "-o", str(executable)], check=True, timeout=90)
        subprocess.run([str(executable)], check=True, timeout=30)


if __name__ == "__main__":
    main()
