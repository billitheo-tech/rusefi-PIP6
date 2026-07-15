# Known-working firmware: Ford 4.9L (300 I6) TFI PIP, uaefi_pro (STM32F7)

Preserved build from the first successful compile after the Ford PIP6 trigger
decoder was written, kept as a known-good fallback for the physical hardware.

- Board: `uaefi_pro` (STM32F7)
- Built: 2026-05-08 (per `rusefi_development_2026-05-08_..._update.srec` filename)
- Source commit: `110180414b31c66d6973e7cba6a940ced42bf0fe` ("Update trigger_ford.cpp")
  - Same PIP6 trigger logic as current `master` post-upstream-sync — verified
    2026-07-14 that `configureFordPip6` in `trigger_ford.cpp` is byte-identical
    between this build's source and the rebased `master`
    (commit `a3b2cec54e...` and later).
- Copied from `Downloads/tuning stuff/rusefi_snapshot_bundle_uaefi_pro (2)/rusefi.snapshot.uaefi_pro/`.

## Files

- `rusefi.bin` - complete image (bootloader + firmware) for blank ECUs, STM32F7.
- `rusefi_development_2026-05-08_uaefi_pro_4226383888_4d3623281e5984f41ee11943b3ec49b55fbbac3a_update.srec` - update image for bootloader flashing.
- `rusefi_uaefi_pro.ini` - matching TunerStudio project file for this exact build (required to tune/log against this firmware).
- `rusefi_h7.bin` - equivalent build for the `uaefi_pro_h7` (STM32H7) variant, from the same source snapshot.
