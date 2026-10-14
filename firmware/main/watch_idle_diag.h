#pragma once
/* Two bounded snapshots per boot, only register-address + read transfers.
 * Caller holds display lock. No external power or codec configuration writes. */
void watch_idle_diag_snapshot(unsigned phase); /* 0 boot, 1 first black screen */
