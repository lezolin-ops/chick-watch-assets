# Lossless resource optimisation and continued development

## Scope and guarantees

- No partition changes, missing animation frames, colour quantisation or alpha thresholding.
- Original PNGs and the editable C images in `firmware/main/` remain unchanged inputs.
- Build output `build/generated/pet_packed_assets.c` uses standard LVGL LZ4-compressed ARGB8888 descriptors. Every compressed block is roundtrip checked against its original bytes and SHA-256 before accepting output.
- Identical payloads share storage. Unreferenced descriptors/payloads are still removed by normal linker garbage collection; the generated test registry is not linked into the device application.
- Existing raw pixel routines for blink, tuft, wing patches and plants receive uncompressed ARGB8888 views. Do not point those routines directly at a compressed descriptor.
- QSPI completion fix, DMA row buffering, brightness, animation timings and walking state machine are preserved.

## Normal development workflow

1. Edit the source PNGs and regenerate assets with the existing simulator tooling.
2. Sync image C arrays and eye/mouth registration metadata into `firmware/main/` as before. Preserve firmware-only platform/resource integration and the QSPI patch rather than blindly overwriting the entire directory with simulator code.
3. `idf.py build` automatically regenerates compressed resources when inputs or packer code change. No Python image/LZ4 package installation is needed: the packer uses standard Python plus a host C compiler and the pinned, licensed LVGL LZ4 source.
4. Read `build/generated/resource_manifest.json` for per-asset raw/stored sizes, SHA-256, CRC32, payload deduplication and cache roles.
5. Validate the raw/packed visual comparison before authorising app-only flashing. Do not change the 10MiB partition merely because the new image fits 8MiB.

Generated resources live in the ignored build directory; do not edit them or treat them as new master artwork. The generated descriptors preserve identifiers, dimensions and stride. New PNGs are supported after the normal C resource generator has produced their descriptors.

## Escape hatch / A-B builds

```sh
# Original uncompressed resources, same behaviour:
idf.py -D PET_PACK_RESOURCES=OFF build
# Re-enable lossless overlay:
idf.py -D PET_PACK_RESOURCES=ON build
```

The output filename is the same in both modes: keep a copy of the desired binary before switching. Never flash a test build automatically.

## Runtime memory contract

- Character poses are tagged by the packer with `LV_IMAGE_FLAGS_USER1` and use a six-entry LRU. Their descriptor addresses stay stable across eviction/reload, so existing animation caches cannot confuse two poses sharing a cache slot.
- The latest two character poses are protected during transitions. Every unloading drops the corresponding LVGL image-cache entry before freeing pixels.
- Static scene artwork and small parts stay resident; total **decoded resource cache** is capped at 3MiB. This is not a limit on total app RAM: existing mutable animation/plant buffers and LVGL draw buffers are additional.
- ESP32 allocations explicitly use PSRAM, preserving internal SRAM for DMA. Safety checks run even when optional LVGL assertions are disabled.
- For future scene switches, delete the old scene objects under the LVGL lock, then call `pet_resource_clear()` before loading a new scene. It unloads decoded pixels while keeping descriptor identities stable. Do not clear while old objects remain visible.
- Add new pose name prefixes to the packer's `pose` classifier if introducing a new resource family. Pose policy is semantic, not a guess based on dimensions. Raise the registry limit only after accounting for metadata; raise the decoded budget only after measuring PSRAM headroom.

## Reproducible verification

```sh
# After building simulator libraries, use the project bundled Python or IDF Python:
python3 firmware/tools/check_resources.py
# Faster codec/cache check only:
python3 firmware/tools/check_resources.py --skip-frames
```

The full command compiles the actual firmware twice, runs the complete animation regression in both modes, and byte-compares every rendered frame. Results are written to `firmware/build/generated/resource_check_report.json`.

`firmware/tools/verify_resource_cache.c` checks every native-decoded image CRC32 against the source and stresses eviction/reload for1,200 pose visits. `lvgl-simulator/tools/verify_daily_animation.c` can include the actual firmware `pet.c` with `PET_TEST_SOURCE` and redirect frames with `PET_PREVIEW_DIR`; compile it in raw and packed modes and compare all resulting PPM files byte-for-byte.

The existing host `lv_conf.h` enables bundled LZ4 for these tests. Source shape/colour edits are not part of this optimisation.

This cache policy targets the existing **single pet**. Multiple simultaneously visible pets need explicit per-object resource pinning rather than assuming that the last two poses protect every actor.
