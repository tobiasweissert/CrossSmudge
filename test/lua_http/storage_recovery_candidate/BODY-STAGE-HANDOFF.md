# Unsealed body staging handoff

Status: isolated candidate only; not integrated, published or device-ready.

`BodyStage.h` now stages at most 2 MiB into an admitted fixed numeric inactive slot using a 1024-byte body buffer. Header and footer stay zero/invalid. Cancel/abort never removes a slot or writes a seal; close is attempted after an opened/reset slot. Every operation-error result may leave a modified **inactive** slot; selected-slot bytes are untouched. The caller must hold one exclusive app lock across fresh recovery, admission, staging and eventual publication. A plan is not a lock or an independently trusted authorization token.

Evidence: `body-red.log` records expected missing-header compilation failure; `body-green.log` records first bounded-body green. Routine `python3 run_all.py` compiles and runs recovery/admission/body suites with exceptions and RTTI disabled, both normally and under ASan/UBSan. `all-receipt.json` pins exact source/test/runner hashes and execution outcomes. The synthetic file adapter deliberately mutates before reporting write/reset failures. Body checks cover all success-path storage operation failures, cancellation before/between chunks, invalid plans/size, empty input, old-slot byte preservation and candidate rejection by the actual recovery validator.

Not qualified: real SdFat, card/block-device behavior, a valid previous generation through full transaction reopening, readback/checksum correctness of newly staged bytes, source-read failures, combined failure sequences, final validity sealing or commit outcomes. No target download capability was enabled. Firmware and active integration checkout were not edited.

Next bounded phase: inject source-read and combined-close failures; add pre-seal metadata/checksum readback and corruption tests, then implement three-state sealing and interrupted-reopen/fault matrix. Retain invalid staging until recovery; do not add rename/unlink cleanup. Review and actual pinned FAT block-device qualification remain required before any versioned Lua integration or target activation.
