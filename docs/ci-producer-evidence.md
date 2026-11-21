# Private CI producer evidence (preparation, not hosted qualification)

The authoritative loss boundary is pioarduino platform 55.03.311
`builder/frameworks/espidf.py:idf_lib_copy`, not an SDK C++ source file.
The authenticated producer SHA256 is
`3b806c21bd7f0682396c9bda7483cd1f257120a2683ce33e2076b0b3705abc2c`.
It copies IDF output into the Arduino package, removes the outer build directory,
then invokes a recursive application build without forwarding verbosity. The
outer dummy ELF link is not the final application's link.

## Opt-in hosted capture

`ci.yml` enables `CROSSCUB_PRIVATE_EVIDENCE` only for private diagnostic collection.
The existing project pre-script `patch_wolfssl.py` registers the project-owned
`ci_producer_capture.py` middleware. Default build flags, script ordering, packing,
`platformio.ini`, keyboard code and SDK C++ sources are unchanged.

The project installer admits only the hash-pinned producer or its exactly
reversible overlay. The two producer changes are:

* invoke the project snapshot helper immediately before the existing `rmtree`;
* forward `--verbose` to the recursive application build.

`producer-overlay.patch` records the minimal platform overlay even when a cache
already contains its exact installed output. The read-only local SDK is not
modified. The pre-deletion snapshot preserves generated `sdkconfig.h`, JSON,
resolved config and linker scripts; missing files remain explicitly missing.

The main-source middleware matches the actual source node, not its variant path
and not the outer dummy sketch. After actual application object compilation it
records expanded `$CXXCOM` argv, then performs a preprocessing-only diagnostic
with the same defines/includes/ABI/language flags. Only object/dependency-output
switches are replaced by `-E -dM -MD -MF ... -MT ...`; `-MD` deliberately includes
system headers missing from the retained ordinary main `.d`. The actual selected
`sdkconfig.h`, system-inclusive dependency and active macro definitions are
retained together before any later cleanup. The original compile argv and ordinary
`.d` remain diagnostic evidence too. Probe errors never become success.

The startup collector accepts this producer closure only with matching environment,
application source, successful probe, dependency/macro hashes, unique allowed
header path and byte-identical frozen/header snapshot. Active selected macros must
agree with the generated header. Existing project/package/config/header/Kconfig
checks remain fail closed. ABSENT_UNRESOLVED and UNDEFINED are not rewritten.
The historical run cannot be admitted retrospectively from these new fixtures.

## Strict cppcheck diagnostics

The project launcher wraps the pinned Core's actual `execute_check_cmd` boundary,
not the check's flags, severity mapping or final result. All original LOW/MEDIUM/HIGH
failure options remain. The exact generated `lib/I18n/I18nKeys.h` is snapshotted
before analyzer execution. On `syntaxError`, preserve exact analyzer argv and the
still-live `--includes-file` bytes, then append only `-E` for a private preprocessing
diagnostic. Preserve quoted tokens exactly, as Core passes argv directly to Popen.
Additional diagnostics are bounded to three failing commands, 60 seconds and
8 MiB per output. Original analyzer failures are returned unchanged. The header
and raw verbose log survive even when preprocessing fails. No language-enum fix,
suppression or severity relaxation is justified by the historical missing header.

Both jobs require the existing recipient certificate validator and use the unchanged
CMS sealer. Raw files never appear in upload paths or console logs; upload only
`sealed.envelope` and `receipt.json` with 31-day requested retention. There are no
new production keys or storage services. Existing synthetic CMS tests are not
production key provisioning.

## Next real CI acceptance (parent dispatch only)

1. Exact approved union source/SDK pins; standard/owner-test packing unchanged.
2. Decrypt and authenticate the same-run sealed artifact privately. Verify the
   producer overlay and pre-deletion receipt, exact generated config/header bytes,
   actual inner-main argv, system-inclusive `.d`, active macros and frozen selected
   header; no inferred board-header selection.
3. Require `actual_resolved_config_captured=true` only if the existing collector
   checks all pass. A failed/missing probe or conflicting bytes remains a red stage.
4. Inspect the inner verbose final link separately from the outer dummy link.
   The merged-initializer report remains optional/incomplete if driver attribution
   is ambiguous; it is not a new admission or release gate.
5. Authenticate analyzer envelope and inspect exact generated header, original
   command/include-list and `-E` diagnostic/exit. The historical syntaxError remains
   unresolved until reproduced or diagnosed from those actual bytes.
6. Preserve the full workflow conclusion: BIN validation alone is not green CI,
   runtime/hardware qualification, config admission or publication authorization.

Portable Python, Bash/YAML and fake-external-driver tests qualify preparation only.
They do not prove that the hosted compiler has produced the requested closure.
