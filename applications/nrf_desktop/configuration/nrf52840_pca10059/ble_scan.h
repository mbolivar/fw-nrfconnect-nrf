/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/* This configuration file defines filters for BLE central (based on BLE short
 * names). Used only by ble_scan module.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} ble_scan_include_once;

/* Filters used for scanning */
char *bt_peripherals[] = { "Mouse nRF52", "Kbd nRF52" };
