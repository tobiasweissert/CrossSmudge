# Crosscub Welcome bear

The owner selected **option 1, Welcome**: a standing/waving cub with a book
under its arm. `Crosscub-Welcome-Bear.png` is the original generated raster
source. There is no original editable SVG/vector master. The earlier hand-drawn
bear and the unselected alternatives are not active product assets.

The artwork and deterministic converter are distributed under this
repository's MIT license. This does not change inherited third-party licenses
or credit the mascot's authors with inherited reader code.

Generate into a separate directory (Pillow required):

```sh
python3 scripts/convert_crosscub_bear.py \
  assets/branding/Crosscub-Welcome-Bear.png OUTPUT_DIRECTORY \
  --prepared-120 assets/branding/Crosscub-Welcome-Bear-120.png
```

The approved 120px preview is an explicit reproducibility input. Output is
120×120, 1,800 bytes, row-major MSB-first, 1=white and 0=black. The generated
`CrosscubBear120.h` must equal `src/images/CrosscubBear120.h`. The production
logo helper uses oriented `drawPixel`, not the unrotated `drawImage` bitmap
path. Both boot and default sleep use this helper; custom image paths and
preferences are unchanged. The web and documentation marks use the same
approved 120px raster, not an unrelated icon.

See `selected-artwork-manifest.json` for exact source/header hashes and
selection provenance. `test/crosscub/test_logo.py` executes production default
boot drawing with text stubs; `test_logo_actual.py` additionally qualifies the
bitmap through extracted, unmodified production rotation/pixel code. These
are host captures, not physical e-ink or full-font boot acceptance.
