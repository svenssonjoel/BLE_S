
/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Sample echo app for CDC ACM class
 *
 * Sample app for USB CDC ACM class driver. The received data is echoed back
 * to the serial port.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <device.h>
#include <drivers/uart.h>
#include <zephyr.h>
#include <sys/ring_buffer.h>

#include <settings/settings.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/services/bas.h>
#include <bluetooth/services/hrs.h>

#include "heap.h"
#include "symrepr.h"
#include "extensions.h"
#include "eval_cps.h"
#include "print.h"
#include "tokpar.h"
#include "prelude.h"

#define RING_BUF_SIZE 1024
u8_t in_ring_buffer[RING_BUF_SIZE];
u8_t out_ring_buffer[RING_BUF_SIZE];

K_MUTEX_DEFINE(uart_io_mutex);
struct device *dev;

struct ring_buf in_ringbuf;
struct ring_buf out_ringbuf;

static void interrupt_handler(struct device *dev)
{
  while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
    if (uart_irq_rx_ready(dev)) {
      int recv_len, rb_len;
      u8_t buffer[64];
      size_t len = MIN(ring_buf_space_get(&in_ringbuf),
		       sizeof(buffer));

      recv_len = uart_fifo_read(dev, buffer, len);

      rb_len = ring_buf_put(&in_ringbuf, buffer, recv_len);
      if (rb_len < recv_len) {
	//silently dropping bytes 
      }
    }

    if (uart_irq_tx_ready(dev)) {
      u8_t buffer[64];
      int rb_len, send_len;

      rb_len = ring_buf_get(&out_ringbuf, buffer, sizeof(buffer));
      if (!rb_len) {
	uart_irq_tx_disable(dev);
	continue;
      }

      send_len = uart_fifo_fill(dev, buffer, rb_len);
      if (send_len < rb_len) {
	//LOG_ERR("Drop %d bytes", rb_len - send_len);
      }
    }
  }
}

int getChar() {

  k_mutex_lock(&uart_io_mutex, K_FOREVER);
  int n;
  u8_t c;
  unsigned int key = irq_lock();
  n = ring_buf_get(&in_ringbuf, &c, 1);
  irq_unlock(key);
  k_mutex_unlock(&uart_io_mutex);
  if (n == 1) {
    return c;
  }
  return -1;
}

void putChar(int i) {
  if (i >= 0 && i < 256) {
    k_mutex_lock(&uart_io_mutex, K_FOREVER);

    u8_t c = (u8_t)i;
    unsigned int key = irq_lock();
    ring_buf_put(&out_ringbuf, &c, 1);
    uart_irq_tx_enable(dev);
    irq_unlock(key);
    k_mutex_unlock(&uart_io_mutex);
  }
}

void usb_printf(char *format, ...) {
  k_mutex_lock(&uart_io_mutex, K_FOREVER);

  va_list arg;
  va_start(arg, format);
  int len;
  static char print_buffer[4096];
	
  len = vsnprintf(print_buffer, 4096,format, arg);
  va_end(arg);

  int num_written = 0;
  while (len - num_written > 0) {
    unsigned int key = irq_lock();
    num_written +=
      ring_buf_put(&out_ringbuf,
		   (print_buffer + num_written),
		   (len - num_written));
    irq_unlock(key);
    uart_irq_tx_enable(dev);
  }
  k_mutex_unlock(&uart_io_mutex);
}


int inputline(char *buffer, int size) {
  int n = 0;
  int c;
  for (n = 0; n < size - 1; n++) {

    c = getChar();
    switch (c) {
    case 127: /* fall through to below */
    case '\b': /* backspace character received */
      if (n > 0)
        n--;
      buffer[n] = 0;
      putChar('\b'); /* output backspace character */
      n--; /* set up next iteration to deal with preceding char location */
      break;
    case '\n': /* fall through to \r */
    case '\r':
      buffer[n] = 0;
      return n;
    default:
      if (c != -1 && c < 256) {
	putChar(c);
	buffer[n] = c;
      } else {
	n --;
      }

      break;
    }
  }
  buffer[size - 1] = 0;
  return 0; // Filled up buffer without reading a linebreak
}

static const struct bt_data ad[] = {
  BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
  BT_DATA_BYTES(BT_DATA_UUID16_ALL,
  		0x0d, 0x18, 0x0f, 0x18, 0x05, 0x18),
  BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
		0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E)
  /*  BT_DATA_BYTES(BT_DATA_UUID128_ALL,
		0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
		0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12),*/
};

static struct bt_uuid_128 bt_uart_base_uuid =
  BT_UUID_INIT_128(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
		   0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E);

  
static struct bt_uuid_128 bt_uart_tx_uuid =
  BT_UUID_INIT_128(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
		   0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E);

static struct bt_uuid_128 bt_uart_rx_uuid =
  BT_UUID_INIT_128(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
		   0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E);

static u8_t bt_uart_write_buf[20];
static u8_t bt_uart_read_buf[20] = "apa";

static ssize_t bt_uart_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     const void *buf, u16_t len, u16_t offset,
			     u8_t flags) {
  u8_t *value = attr->user_data;
	
  if (len > sizeof(bt_uart_write_buf)) {
    //return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    return 0;
  }
	
  memcpy(value + offset, buf, len);

  bt_gatt_notify(NULL, attr, buf, len);
  
  return len;
}

static ssize_t bt_uart_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			    void *buf, u16_t len, u16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

static void bt_uart_ccc_changed(const struct bt_gatt_attr *attr,
				       u16_t value)
{
  (void) attr;

  bool notif_enabled = (value == BT_GATT_CCC_NOTIFY);
  
  //LOG_INF("BAS Notifications %s", notif_enabled ? "enabled" : "disabled");
}

  
BT_GATT_SERVICE_DEFINE(bt_uart,
		       BT_GATT_PRIMARY_SERVICE(&bt_uart_base_uuid),
		       BT_GATT_CHARACTERISTIC(&bt_uart_tx_uuid.uuid, // TX from the point of view of the central
					      BT_GATT_CHRC_WRITE,    // central is allowed to write to tx
					      BT_GATT_PERM_WRITE,
					      NULL, bt_uart_write, bt_uart_write_buf),
		       BT_GATT_CHARACTERISTIC(&bt_uart_rx_uuid.uuid, // RX from point of view of central
					      BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
					      BT_GATT_PERM_READ,
					      bt_uart_read, NULL, bt_uart_read_buf),
		       BT_GATT_CCC(bt_uart_ccc_changed,
				   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));
		       

static void connected(struct bt_conn *conn, u8_t err)
{
	if (err) {
		usb_printf("Connection failed (err 0x%02x)\n\r", err);
	} else {
		usb_printf("Connected\n\r");
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	usb_printf("Disconnected (reason 0x%02x)\n\r", reason);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
}
static struct bt_conn_auth_cb auth_cb_display = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.cancel = auth_cancel,
};

static void bt_ready(int err)
{
	if (err) {
           usb_printf("Bluetooth init failed (err %d)\n", err);
	   return;
	}

	usb_printf("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		usb_printf("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

static void bas_notify(void)
{
	u8_t battery_level = bt_gatt_bas_get_battery_level();

	battery_level--;

	if (!battery_level) {
		battery_level = 100U;
	}

	bt_gatt_bas_set_battery_level(battery_level);
}


static VALUE bas_set_level(VALUE *args, int argn) 
{
  if (argn != 1) {
    return enc_sym(symrepr_nil());
  }
  int bat_lvl = dec_i(args[0]);
  
  bt_gatt_bas_set_battery_level((u8_t)bat_lvl);
  return enc_sym(symrepr_true());
}

 
void main(void)
{

	u32_t baudrate, dtr = 0U;
	int ret;
	int err;

	dev = device_get_binding("CDC_ACM_0");
	if (!dev) {
		return;
	}

	ring_buf_init(&in_ringbuf, sizeof(in_ring_buffer), in_ring_buffer);
	ring_buf_init(&out_ringbuf, sizeof(out_ring_buffer), out_ring_buffer);

	while (true) {
		uart_line_ctrl_get(dev, LINE_CTRL_DTR, &dtr);
		if (dtr) {
			break;
		} else {
			/* Give CPU resources to low priority threads. */
			k_sleep(100);
		}
	}

	/* They are optional, we use them to test the interrupt endpoint */
	ret = uart_line_ctrl_set(dev, LINE_CTRL_DCD, 1);
	if (ret) {
	  //LOG_WRN("Failed to set DCD, ret code %d", ret);
	}

	ret = uart_line_ctrl_set(dev, LINE_CTRL_DSR, 1);
	if (ret) {
	  //LOG_WRN("Failed to set DSR, ret code %d", ret);
	}

	/* Wait 1 sec for the host to do all settings */
	k_busy_wait(1000000);

	ret = uart_line_ctrl_get(dev, LINE_CTRL_BAUD_RATE, &baudrate);
	if (ret) {
	  //LOG_WRN("Failed to get baudrate, ret code %d", ret);
	} else {
	  //LOG_INF("Baudrate detected: %d", baudrate);
	}

	uart_irq_callback_set(dev, interrupt_handler);

	/* Enable rx interrupts */
	uart_irq_rx_enable(dev);


	err = bt_enable(bt_ready);

	if (err) { 
	  usb_printf("Error enabling BLE\n");
	}
 	
	bt_set_name("BLE_TOOL_NRF52_FW");
	bt_conn_cb_register(&conn_callbacks);
	bt_conn_auth_cb_register(&auth_cb_display);


	usb_printf("Allocating input/output buffers\n\r");
	size_t len = 1024;
	char *str = malloc(1024);
	char *outbuf = malloc(4096);
	int res = 0;

	heap_state_t heap_state;

	res = symrepr_init();
	if (res)
		usb_printf("Symrepr initialized.\n\r");
	else {
		usb_printf("Error initializing symrepr!\n\r");
		return;
	}
	int heap_size = 2048;
	res = heap_init(heap_size);
	if (res)
		usb_printf("Heap initialized. Free cons cells: %u\n\r", heap_num_free());
	else {
		usb_printf("Error initializing heap!\n\r");
		return;
	}

	res = eval_cps_init(false);
	if (res)
		usb_printf("Evaluator initialized.\n\r");
	else {
		usb_printf("Error initializing evaluator.\n\r");
	}

	if (extensions_add("set-bat-level", bas_set_level)) {
	  usb_printf("set-bat-level extension added.\n\r");
	} else {
	  usb_printf("set-bat-level extension failed!\n\r");
	}

	VALUE prelude = prelude_load();
	eval_cps_program(prelude);

	usb_printf("Lisp REPL started (BLE_TOOL_NRF52_FW)!\n\r");

	while (1) {
		k_sleep(100);
		usb_printf("# ");
		memset(str,0,len);
		memset(outbuf,0, 1024);
		inputline(str, len);
		usb_printf("\n\r");

		if (strncmp(str, ":info", 5) == 0) {
			usb_printf("##(BLE_TOOL_NRF52_FW)#######################################\n\r");
			usb_printf("Used cons cells: %lu \n\r", heap_size - heap_num_free());
			usb_printf("ENV: "); simple_snprint(outbuf,4095, eval_cps_get_env()); usb_printf("%s \n\r", outbuf);
			heap_get_state(&heap_state);
			usb_printf("GC counter: %lu\n\r", heap_state.gc_num);
			usb_printf("Recovered: %lu\n\r", heap_state.gc_recovered);
			usb_printf("Marked: %lu\n\r", heap_state.gc_marked);
			usb_printf("Free cons cells: %lu\n\r", heap_num_free());
			usb_printf("############################################################\n\r");
			memset(outbuf,0, 4096);
		} else if (strncmp(str, ":quit", 5) == 0) {
			break;
		} else if (strncmp(str, ":notify", 7) == 0) {
		  bas_notify();
		} else {

			VALUE t;
			t = tokpar_parse(str);

			t = eval_cps_program(t);

			if (dec_sym(t) == symrepr_eerror()) {
				usb_printf("Error\n");
			} else {
				usb_printf("> "); simple_snprint(outbuf, 1023, t); usb_printf("%s \n\r", outbuf);
			}
		}
	}

	symrepr_del();
	heap_del();

}
