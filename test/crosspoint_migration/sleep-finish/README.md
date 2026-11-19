# Sleep import-parity qualification

Run from the repository:

```
python3 test/crosspoint_migration/sleep-finish/run.py --evidence /PATH/normal
python3 test/crosspoint_migration/sleep-finish/run.py --sanitize --evidence /PATH/sanitize
```

Each child has a 90-second deadline; dispatch compiler/executable have separate 60/30-second bounds. The runner persists commands, exits, logs and source/test input hashes. It is the standalone routine entrypoint; no unowned root test registration was changed.

- `preservation.py`: ten complete legacy Sleep method bodies remain byte-identical to the retained preimage, allowing only the required FsFile → HalFile SDK spelling migration. This includes default bear/art call, custom image selection, Page Overlay, statistics and Quick Resume painting.
- `dispatch.py`: actual `onEnter` body and current settings raw sleep enum, with synthetic renderer/activity dependencies. All fourteen raw modes, independent prior driver inversion and saved inversion, reader/non-reader entry, timeout override. The timeout flag enables the preserved timeout Quick Resume preference in those cases. Modern modes preserve visible polarity before compositing and do not paint a destructive entering-sleep popup; old entry behavior remains unchanged.
- `composition.cpp`: complete modern Sleep methods, cover method, BMP alpha/placement/selection helper bodies and actual production `grayPlanePixel`, with explicitly synthetic HAL/storage, Bitmap and PNG decoder doubles. Covers differential/absolute/direct-capability orchestration, filters/fit/crop, cover/retained-frame composition, BMP alpha and white-key/PNG branches, root and directory sources, alpha/white-erasure/gray-plane writes, snapshot allocation denial, row allocation failure, decoder pass failures, rejected grayscale base, no-overlay fallback, loaning cover-build failure and borrowed pointer cleanup. Decode faults can mutate the framebuffer before returning failure, proving first-pass rollback. These are operation traces, NOT screenshots or fabricated image/panel pixels.

The retained preimage reproducibly fails the dispatch assertion for the modern modes. An initial partial-PNG failure regression separately reproduced underlay corruption and is now covered by checked one-snapshot rollback before retry. Initial-draw snapshots are discarded before the optional differential-gray keeper takes its snapshot; there are no nested framebuffer snapshot owners in the new composition pipeline.

Not qualified: whole Sleep translation-unit/SDK target compilation, actual PNG/BMP codec output, real fonts or screenshots, physical Sticky grayscale/refresh/power/wake/memory, existing reader gray-underlay fidelity, FAT durability or live OTA/radio. No SDK initialization, installs, firmware build, input/wake policy edits, commits, index/ref mutations, pushes or publication are performed by this suite.
