# QSPI display completion fix

The board uses a QSPI display, not a parallel RGB panel. Using `lvgl_port_add_disp_rgb` selects the RGB flush path, which reports the draw buffer ready immediately after enqueueing a bitmap transfer. SPI DMA may still be reading it. `lvgl_port_add_disp` instead registers the SPI colour-transfer completion callback and releases the buffer after transmission.

The fix changes only display registration; it leaves existing internal DMA row-buffer, transfer-size, brightness, memory and timing adjustments intact. Current application binary before this fix is preserved in `firmware/backup/pet_watch_before_qspi_fix_20261011.bin`; original factory backup remains unchanged.

Managed components are ignored and may be replaced by the dependency manager. Keep this patch in version control and reapply after dependency regeneration, from the project root:

```sh
git apply --check firmware/patches/qspi_flush_completion.patch
git apply firmware/patches/qspi_flush_completion.patch
```

If already applied, `git apply --reverse --check firmware/patches/qspi_flush_completion.patch` succeeds. Do not actually reverse it to test application. This patch does not reconstruct earlier local DMA/transfer-size adjustments; inspect those separately if replacing the whole component.

After applying, rebuild before `idf.py -p /dev/cu.usbmodem1101 -b 460800 app-flash`. App-only flashing preserves bootloader, partition table and NVS. Pixel correctness still requires visual hardware acceptance; a successful build/flash alone is not proof that horizontal bars are gone.
