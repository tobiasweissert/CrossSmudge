# ESP32Drop source provenance

Upstream: https://github.com/s-iwaki-d/ESP32Drop

Immutable commit: `92bdc1221924ca522f3ccbebbd9353612ce5b2d7`.
License: 0BSD; original license is retained in `LICENSE`.

The receiver-only subset contains AWDL plus binary-plist/DNS-SD builders and
the licensed self-signed EC certificate/key constants. Sender, BLE, M5Stack examples, and the
upstream autoaccepting/whole-archive TLS receiver are excluded. A separate
link-only probe in the task evidence compiled the pinned original receiver;
that probe does not execute or enable receiving.

Core headers retain upstream bytes except the capability-gated truthful AirDrop
TXT flag advertisement in `awdl_frame.h`; non-Sticky golden frames are retained. The port and its public header are
adapted for cooperative stop, checked partial acquisition, callback admission,
and a persistent queued TCP/IP drain acknowledgment. `awdl_owned_gate.h` is
new CrossSmudge code. No driver sanity-check bypass, private binary patch,
multiple-definition linker flag, external task deletion, or global NVS/event
teardown was introduced.

The task evidence `qualification/upstream-manifest.json` records upstream
Git blob identities, SHA256, and sizes for every retrieved preimage. Final
`vendor-manifest.json` records the exact vendored bytes, modifications, and
new files. The native TLS/session adapter is maintained separately in `lib/AirDrop/port/`;
its executable reuse and source hashes are recorded in the integration evidence.

The adapted port has host lifecycle coverage. Final complete pinned firmware
build, parent source review and physical radio qualification remain required;
a successful foundation probe is not a complete receiver firmware build.
