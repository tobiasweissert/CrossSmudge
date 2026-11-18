# OTA transition regression

Run without SDK, downloads, or firmware builds:

```sh
python3 test/crosscub/run_all.py --ota-only --output /root/github/.host-data/crosssmudge/github-ota-repair/flow/check
```

The normal and `--sanitize` (UBSan only) runs compile and execute the actual
Settings `CheckForUpdates` switch arm and OTA `onEnter`, `onExit`, check,
installation and input-loop method bodies. Network, renderer, allocation and
updater results are controlled hardware doubles, not live network or device
qualification. The Wi-Fi connection helper, main render-stack selector, stack
constants and `ActivityManager::begin` argument are extracted from production;
target names/values come from production `SilentRestart.h`.

Four executable groups (`entry`, `exit`, `startup`, `stack`) cover direct entry,
entry allocation failure, no-update/check-error/OOM exits, explicit install
confirmation, cancellation, failed installation versus successful restart,
radio startup failure, Wi-Fi-child allocation failure, canceled/disconnected
Wi-Fi completion, synchronous-render refusal and S3/C3 stack selection.

The harness intentionally does not compile the entire firmware or renderer,
exercise ActivityManager's scheduler, establish real TLS/HTTP/SD/flash behavior,
or reproduce a physical panic. Existing updater security/SD-recovery code is
unchanged. Target build and on-device low-memory/radio/navigation qualification
remain separate gates. No ASan executable with large virtual-memory reservation
is launched.
