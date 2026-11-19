# Target asset/include regression

Run the complete bounded suite:

```sh
sh test/crosspoint_migration/target-assets/run-all.sh EVIDENCE_DIRECTORY
```

Uses the existing host `g++`, cached production PNGdec, and unmodified pinned
FreeInkUI headers. No generated API doubles, package installation, image inspection,
image generation, network access, target firmware build, or screenshots.

## Authoritative asset provenance

Headers are restored **byte-for-byte**, not regenerated:

| Destination | Immutable source commit | Declaration / dimensions |
| --- | --- | --- |
| `src/components/icons/book.h` | `9ac3475733e677b7f61ba3f6e8ce41ce1b9b2967` | `BookIcon`, 32x32, 128 bytes |
| `src/components/icons/book24.h` | `818b0befac4d9249c7abded67fa980187c4b41af` | `Book24Icon`, 24x24, 72 bytes |
| `src/images/LoadingIcon.h` | `818b0befac4d9249c7abded67fa980187c4b41af` | `LoadingIcon`, 48x48, 288 bytes |

Original paths equal destination paths. `LICENSE.owner` and `LICENSE.upstream`
preserve the respective source commits' license notices. Source file Git blob IDs,
full header SHA256, and narrowly parsed named-array pixel SHA256 are recorded by
`run.py` in each assets evidence JSON. No unrelated image arrays are read.

The existing CoverGrid, Dashboard, Minimal and main include/use sites need no edits.
Owner Welcome artwork, store, UI and startup behavior are not changed.

## PNGdec boundary

`SleepActivity.cpp` saves `local`, undefines it for the vendor include, then restores
the incoming macro state. PNGdec's zlib internals remain untouched. This prevents
its `#define local static` from corrupting the actual SDK `CatalogWindow local`
declarations. Regression sources extract the boundary directly from the application
source and compile it before the real SDK catalog header; a second case verifies
restoration of an existing macro. The real PNG class and asset declarations must
compile; no stand-in APIs are supplied.

Compiler logs, exit codes, generated compiler inputs, dependency manifests and
input hashes are retained under the caller-specified evidence directory. `red`
records precede repair; `final` records bind the frozen repaired bytes. Host syntax
compilation is not a complete Sticky firmware build or physical-device acceptance.
Other compile partitions remain the coordinator's responsibility.
