# LVGL asset notes

Target: 2.06-inch 410x502 AMOLED.

- The PNG files use RGBA transparency and are suitable as design/master assets.
- Recommended default pet sprite: 256x256. Use 192x192 when more UI space is needed.
- In an SDL simulator, PNG files can be loaded when a PNG decoder and filesystem driver are enabled.
- LVGL 8 uses `lv_img`; LVGL 9 uses `lv_image`.
- For the final ESP32-S3 build, convert frequently used frames to LVGL image C arrays or binary assets.
- Decode or display animation frames one at a time; do not keep all animation frames expanded in RAM.
- A decoded 256x256 RGBA frame requires about 256 KiB.
