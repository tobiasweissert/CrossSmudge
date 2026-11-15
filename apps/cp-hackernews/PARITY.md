# Hacker News APIv3 preview

New Lua implementation inspired by CrossPlay's MIT HackerNewsActivity; original
attribution retained in LICENSE. Not a full browser or complete feature port.

- Real official HN topstories IDs, up to 500, in provider order. One story loaded
  per explicit previous/next selection; no demo data bundled. Feed refresh loads
  the first story on the following update after completion.
- Title, author, public link, and readable HN story text. Link articles are
  explicitly fetched through Jina Reader, the service used by CrossPlay. The
  selected public URL is disclosed to Jina; no accounts or keys are used.
- Complete responses only, up to 8192 bytes. Large items/articles, redirects,
  compressed responses and incompatible framing fail visibly. No truncated
  article is presented as complete. No comment tree, search or saved-story shelf.
- Buttons: Prev/Next choose menu action or turn text pages; Select executes;
  Back cancels a pending load, returns to menu, then exits. Touch footer uses the
  same actions. Tap left/right content to choose/turn; drag events are ignored.
- Last feed, last successful story, and last article use two checksummed local
  cache slots each, with device-local fetch date. Failed reads/writes retain the
  older valid slot. Cache is read on launch; cached article is reused by item ID.
  Refreshing an article already cached requires clearing its app-local cache
  manually; there is no automatic retry or expiration.
- Frozen firmware requires Wi-Fi selection at launch even to view cached data.
  Cache supports offline *data use* after startup / when Internet requests fail;
  canceling the firmware Wi-Fi picker exits the app. UTC trust is per boot.
- Preview targets S3's 2 MiB Lua budget. Small native 75 KiB cases pass, but
  maximum-data flow fails that cap. Not qualified for X3/X4 C3 or App Store.

Provider documentation: https://github.com/HackerNews/API and
https://jina.ai/reader/. Provider content retains its original rights; no feed or
article content is redistributed in this package. Live wire compatibility and
physical TLS/heap/UI checks are pending; see docs/lua-network-apps.md.
