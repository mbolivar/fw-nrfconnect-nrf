/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 *  @brief Nordic Battery Service Client sample
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <inttypes.h>
#include <errno.h>
#include <zephyr.h>
#include <misc/printk.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>
#include <bluetooth/services/bas_c.h>
#include <dk_buttons_and_leds.h>


/**
 * Button to read the battery value
 */
#define KEY_READVAL_MASK DK_BTN1_MSK


static struct bt_conn *default_conn;
static struct bt_gatt_bas_c bas_c;


static void bas_c_notify_cb(struct bt_gatt_bas_c *bas_c,
				    u8_t battery_level);


static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(device_info->addr, addr, sizeof(addr));

	printk("Filters matched. Address: %s connectable: %s\n",
		addr, connectable ? "yes" : "no");

	err = bt_scan_stop();
	if (err) {
		printk("Stop LE scan failed (err %d)\n", err);
	}
}

static void scan_connecting_error(struct bt_scan_device_info *device_info)
{
	printk("Connecting failed\n");
}

static void scan_connecting(struct bt_scan_device_info *device_info,
			    struct bt_conn *conn)
{
	default_conn = bt_conn_ref(conn);
}

static struct bt_scan_cb scan_cb = {
	.filter_match = scan_filter_match,
	.connecting_error = scan_connecting_error,
	.connecting = scan_connecting
};

static void discovery_completed_cb(struct bt_gatt_dm *dm,
				   void *context)
{
	int err;

	printk("The discovery procedure succeeded\n");

	bt_gatt_dm_data_print(dm);

	err = bt_gatt_bas_c_handles_assign(dm, &bas_c);
	if (err) {
		printk("Could not init BAS client object, error: %d\n", err);
	}

	err = bt_gatt_bas_c_subscribe(&bas_c, bas_c_notify_cb);
	if (err) {
		printk("Cannot subscribe to BAS value notification (err: %d)\n",
		       err);
		/* Continue anyway */
	}

	err = bt_gatt_dm_data_release(dm);
	if (err) {
		printk("Could not release the discovery data, error "
		       "code: %d\n", err);
	}
}

static void discovery_service_not_found_cb(struct bt_conn *conn,
					   void *context)
{
	printk("The service could not be found during the discovery\n");
}

static void discovery_error_found_cb(struct bt_conn *conn,
				     int err,
				     void *context)
{
	printk("The discovery procedure failed with %d\n", err);
}

static struct bt_gatt_dm_cb discovery_cb = {
	.completed = discovery_completed_cb,
	.service_not_found = discovery_service_not_found_cb,
	.error_found = discovery_error_found_cb,
};

static void connected(struct bt_conn *conn, u8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Failed to connect to %s (%u)\n", addr, conn_err);
		return;
	}

	printk("Connected: %s\n", addr);

	if (bt_conn_security(conn, BT_SECURITY_MEDIUM)) {
		printk("Failed to set security\n");
	}

	if (conn == default_conn) {
		int err = bt_gatt_dm_start(conn,
					   BT_UUID_BAS,
					   &discovery_cb,
					   NULL);
		if (err) {
			printk("Could not start the discovery procedure, error "
			       "code: %d\n", err);
		}
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason %u)\n", addr, reason);

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	/* This demo doesn't require active scan */
	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
	}
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void scan_init(void)
{
	int err;

	struct bt_scan_init_param scan_init = {
		.connect_if_match = 1,
		.scan_param = NULL,
		.conn_param = BT_LE_CONN_PARAM_DEFAULT
	};

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_BAS);
	if (err) {
		printk("Scanning filters cannot be set (err %d)\n", err);

		return;
	}

	err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
	if (err) {
		printk("Filters cannot be turned on (err %d)\n", err);
	}
}

static void bas_c_notify_cb(struct bt_gatt_bas_c *bas_c,
				    u8_t battery_level)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(bt_gatt_bas_c_conn(bas_c)),
			  addr, sizeof(addr));
	if (battery_level == BT_GATT_BAS_VAL_INVALID) {
		printk("[%s] Battery notification aborted\n", addr);
	} else {
		printk("[%s] Battery notification: %"PRIu8"%%\n",
		       addr, battery_level);
	}
}

static void bas_c_read_cb(struct bt_gatt_bas_c *bas_c,
				  u8_t battery_level,
				  int err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(bt_gatt_bas_c_conn(bas_c)),
			  addr, sizeof(addr));
	if (err) {
		printk("[%s] Battery read ERROR: %d\n", addr, err);
		return;
	}
	printk("[%s] Battery read: %"PRIu8"%%\n", addr, battery_level);
}


static void button_readval(void)
{
	int err;

	printk("Reading BAS value:\n");
	err = bt_gatt_bas_c_read(&bas_c, bas_c_read_cb);
	if (err) {
		printk("BAS read call error: %d\n", err);
	}
}


static void button_handler(u32_t button_state, u32_t has_changed)
{
	u32_t button = button_state & has_changed;

	if (button & KEY_READVAL_MASK) {
		button_readval();
	}
}


void main(void)
{
	int err;

	bt_gatt_bas_c_init(&bas_c);

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	scan_init();
	bt_conn_cb_register(&conn_callbacks);

	err = dk_buttons_init(button_handler);
	if (err) {
		printk("Failed to initialize buttons (err %d)\n", err);
		return;
	}

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");
}