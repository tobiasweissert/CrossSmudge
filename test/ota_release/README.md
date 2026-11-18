# Crosscub OTA release qualification

Run the stdlib-only regression entrypoint (also run by `release.yml` before builds):

```sh
python3 -m unittest discover -s test/ota_release -v
```

All generated ESP image fixtures are **synthetic test data, not executable firmware**.
They exercise framing, descriptor, board marker, checksum and SHA256 rejection.
Git fixtures are isolated temporary repositories. GitHub transport in publication
unit tests is mocked; these tests do **not** demonstrate a real GitHub publication.

## Repeatable owner release operation

1. Choose an exact numeric version newer than the stable GitHub `/latest` version:
   1–4 decimal components, 1–9 digits per component, at most 31 ASCII bytes.
2. `scripts/release.sh VERSION --write-version` explicitly updates both INI
   version fields portably and prints a **draft** workflow command. It does not
   commit, tag, push, compile or dispatch anything. Without `--write-version`,
   it is a read-only plan. `--config PATH` is available for isolated tooling tests.
3. Qualify the complete firmware/source delta, obtain owner approval, and merge
   only that reviewed delta into `tobiasweissert/Crosscub` main. The operator owns
   committing/pushing; do not release an unmerged task SHA.
4. `scripts/release.sh VERSION --publish` prints the explicit owner command:
   `gh workflow run release.yml --repo tobiasweissert/Crosscub --ref main -f version=VERSION -f publish=true`.
   **Printing is not approval or execution.** Only the repository owner can select
   production publication. Omit `--publish` for `publish=false` and a draft.
5. The workflow checks the exact source SHA is on owner main for **both** dispatch
   and tag pushes. Committed INI versions must already match the requested version;
   CI does not rewrite them. Only the standard Sticky profile is built.
6. Image validation runs before artifact upload and again before GitHub release
   upload. A new draft is created with an exact verified tag target. Asset URL,
   size and GitHub SHA256 digest and tag target are read back before an explicitly
   approved stable publication; stable state and `/latest` are read back afterward.
7. A published stable release for the same tag is a successful **prebuild no-op**.
   Duplicate tag-trigger runs cannot build, upload, redraft or replace it. Existing
   drafts/prereleases are refused rather than overwritten. Release operations are
   serialized. Interrupted drafts require an explicit owner recovery decision;
   rerunning is intentionally not an implicit asset replacement or promotion.

Tag pushes always default to draft; arbitrary CI, debug and release-candidate
builds are not promoted. Drafts do not change the public Pages catalog. GitHub OTA
uses `/latest` directly and does not depend on the optional signed Pages update.

For a read-only local check of a real built artifact:

```sh
python3 scripts/validate_ota_release.py --version VERSION \
  --firmware /path/crosscub-sticky-vVERSION.bin --report /path/validation.json
```

Validation checks Crosscub project/version at the fixed ESP app-descriptor offset,
ESP32-S3 chip ID, present and nonconflicting Sticky marker, canonical filename,
64 KiB minimum, both 0x640000 OTA slot limits, complete segment framing, exact
image size, XOR checksum, and mandatory appended SHA256. The report also records
the complete file's SHA256. It is not a firmware signature or hardware boot test.
