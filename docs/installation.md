---
title: Installation
nav_order: 2
---

# Installing and updating Crosscub

Crosscub is for **Seeed reTerminal Sticky only**. Source is on
[`feat/crosscub-sticky-only`](https://github.com/tobiasweissert/Crosscub/tree/feat/crosscub-sticky-only).
There is **no published Crosscub firmware feed yet**. Do not install a different
product's release just because it lives in the same historical repository.

## SD Card Firmware Update

When a verified Sticky application image is available:

1. Back up the SD card and keep the previous working image.
2. Copy the `.bin` application image to the SD card without formatting it.
3. Open **Settings → System → SD Card Firmware Update**, select the image and
   confirm the update. The existing recovery/update flow remains available.
4. Let the device complete verification and reboot. Keep power connected.

The updater retains chip/integrity/board checks. It is not a factory-image or
partition-migration tool. Existing reading progress, settings, custom fonts,
boot/sleep pictures, apps and save directories remain at their current paths.
Do not erase NVS or rename `.crosspoint`, `.crosssmudge` or `.smudge` to rebrand.

## OTA

**Settings → Firmware Update** checks the owner's repository. Only tagged
`crosscub-v<VERSION>` releases with `crosscub-sticky-v<VERSION>.bin`, a SHA-256
digest, the Sticky board identity and a `Crosscub` application descriptor can
be installed. Redirected release manifests and legacy upstream products are
not accepted. A missing/non-Crosscub latest release is a no-update result, not
an invitation to install upstream firmware. No Crosscub update has been
published yet; manual SD update/recovery remains usable independently.

## USB and first installation

USB upload and the original recovery routes are retained in source. A target
build is not a physical flashing qualification. Use a device-specific verified
procedure and distinguish an application image from a factory image before
writing flash; do not guess offsets or overwrite partitions. Sticky does not
expose an owned USB mass-storage product mode. Use the existing web file
manager, WebDAV, Calibre, Nearby/AirDrop or a card reader for file transfers.

## Build the application

Use the pinned PlatformIO Core **6.1.19**, platform **55.03.37**, Arduino
**3.3.7** and the pinned SDK/submodules. `pio run` selects `sticky`; explicit
`pio run -e sticky` does the same. `sticky-owner-test` is a Sticky-only cache
compatibility alias, not a separate App Store or product. Coordinate all jobs
sharing a PlatformIO cache. No firmware build, flashing or physical acceptance
is implied by the host tests.
