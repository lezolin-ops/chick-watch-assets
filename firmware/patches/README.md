# QSPI display completion fix

## Wi-Fi internal-memory follow-up

The fixed portrait watch does not rotate its display at runtime. Adding Wi-Fi reduced available internal heap and the unnecessary software-rotation buffer failed to allocate during boot. Apply `fixed_portrait_memory.patch` after the existing completion patch and previous buffer adjustments:

```sh
git apply --check firmware/patches/fixed_portrait_memory.patch
git apply firmware/patches/fixed_portrait_memory.patch
```

If already applied, `git apply --reverse --check firmware/patches/fixed_portrait_memory.patch` succeeds; do not actually reverse. Wi-Fi memory options are preserved in `sdkconfig.defaults`. The SPI completion callback and 40-row internal DMA draw buffer remain unchanged. The follow-up was flashed and passed a 15-second startup check on 2026-10-12: display/Wi-Fi initialized without panic or reboot loops. Visual and wireless functional acceptance remain pending.

The board uses a QSPI display, not a parallel RGB panel. Using `lvgl_port_add_disp_rgb` selects the RGB flush path, which reports the draw buffer ready immediately after enqueueing a bitmap transfer. SPI DMA may still be reading it. `lvgl_port_add_disp` instead registers the SPI colour-transfer completion callback and releases the buffer after transmission.

The fix changes only display registration; it leaves existing internal DMA row-buffer, transfer-size, brightness, memory and timing adjustments intact. Current application binary before this fix is preserved in `firmware/backup/pet_watch_before_qspi_fix_20261011.bin`; original factory backup remains unchanged.

Managed components are ignored and may be replaced by the dependency manager. Keep this patch in version control and reapply after dependency regeneration, from the project root:

```sh
git apply --check firmware/patches/qspi_flush_completion.patch
git apply firmware/patches/qspi_flush_completion.patch
```

If already applied, `git apply --reverse --check firmware/patches/qspi_flush_completion.patch` succeeds. Do not actually reverse it to test application. This patch does not reconstruct earlier local DMA/transfer-size adjustments; inspect those separately if replacing the whole component.

After applying, rebuild before `idf.py -p /dev/cu.usbmodem1101 -b 460800 app-flash`. App-only flashing preserves bootloader, partition table and NVS. Pixel correctness still requires visual hardware acceptance; a successful build/flash alone is not proof that horizontal bars are gone.

## Panel handle access for CO5300 sleep (2026-10-14)

Apply `panel_handle_access.patch` after the two existing patches. It only exposes borrowed handles through `bsp_display_get_panel_io`; it does not replace display registration, DMA buffers or callbacks. `watch_panel.c` owns no hardware handle and never deletes them.

```sh
git apply --check firmware/patches/panel_handle_access.patch
git apply firmware/patches/panel_handle_access.patch
```

Already applied: `git apply --reverse --check firmware/patches/panel_handle_access.patch` succeeds; never reverse it as a validation step. Reapply all required patches after replacing managed components.
