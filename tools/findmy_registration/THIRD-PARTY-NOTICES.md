# Third-party notices

`registration.py` adapts the local record-signature structure from
VibeCoderLab/esp32-findmy-integration `host/cloud.py`, commit
`1c0ede3989ee11bd97e9bed169a7fbc6a99db460` (MIT, Copyright (c) 2026 ESP32 Find My
contributors). Full license: `LICENSE.upstream.txt`. The adapter adds full serialized
identifier/zone binding; it does not vendor upstream login, recovery, PCS construction
or deletion framing. Source file pins are in `source.lock.json`.

`test/findmy/registration/fixtures/cloudkit_client.py` is the unchanged public
`findmy/cloudkit/client.py` from parawanderer/FindMy.py commit
`7969003ca785658369b650f75d9e7ca519566bf6` (MIT, Copyright (c) 2023 FindMy.py
Contributors). Its full license is adjacent as `LICENSE.FindMy.txt`. Tests verify its
source SHA-256 and select only `AsyncCloudKitClient.record_save` through AST, with
synthetic transport; they do not import the upstream module or authenticate.

Downloaded source is review data with its original licenses. Hash pinning does not
relicense or approve any code. No AGPL implementation or Apple/private SDK is vendored.
No proprietary runtime is downloaded, installed or executed by this implementation.
