/*
    Copyright 2019 Joel Svensson	svenssonjoel@yahoo.se

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

u8_t ble_in_ring_buffer[RING_BUF_SIZE];

K_MUTEX_DEFINE(uart_io_mutex);
K_MUTEX_DEFINE(ble_uart_mutex);

struct device *dev;

struct ring_buf in_ringbuf;
struct ring_buf out_ringbuf;
struct ring_buf ble_in_ringbuf; 

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

int get_char() {

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

void put_char(int i) {
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

    c = get_char();
    switch (c) {
    case 127: /* fall through to below */
    case '\b': /* backspace character received */
      if (n > 0)
        n--;
      buffer[n] = 0;
      put_char('\b'); /* output backspace character */
      n--; /* set up next iteration to deal with preceding char location */
      break;
    case '\n': /* fall through to \r */
    case '\r':
      buffer[n] = 0;
      return n;
    default:
      if (c != -1 && c < 256) {
	put_char(c);
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

// 0x180d  -- Heart rate service
// 0x1805  -- Current time service
// 0x180A  -- Device Information Service (activated via the prj.conf file)


static const struct bt_data ad[] = {
  BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
  BT_DATA_BYTES(BT_DATA_UUID16_ALL, // Advertise a battery level service.
		0x0f, 0x18),
  BT_DATA_BYTES(BT_DATA_UUID128_ALL, // Advertice BLE UART service
		0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,
		0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E)
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

static u8_t bt_uart_read_buf[20] = "apa";

static ssize_t bt_uart_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     const void *buf, u16_t len, u16_t offset,
			     u8_t flags) {
  //u8_t *value = attr->user_data;

  //if (len > sizeof(bt_uart_write_buf)) {
    //return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
  //  return 0;
  //}

  //memcpy(value + offset, buf, len);

  int n = ring_buf_put(&ble_in_ringbuf,buf, len);
  //usb_printf("%s\n\r", buf);
  
  //usb_printf("%d \n\r", n);
  //bt_gatt_notify(NULL, attr, buf, len);

  return n;
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

  // bool notif_enabled = (value == BT_GATT_CCC_NOTIFY);
}


BT_GATT_SERVICE_DEFINE(bt_uart,
		       BT_GATT_PRIMARY_SERVICE(&bt_uart_base_uuid),
		       BT_GATT_CHARACTERISTIC(&bt_uart_tx_uuid.uuid, // TX from the point of view of the central
					      BT_GATT_CHRC_WRITE,    // central is allowed to write to tx
					      BT_GATT_PERM_WRITE,
					      NULL, bt_uart_write, NULL),
		       BT_GATT_CHARACTERISTIC(&bt_uart_rx_uuid.uuid, // RX from point of view of central
					      BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
					      BT_GATT_PERM_READ,
					      bt_uart_read, NULL, bt_uart_read_buf),
		       BT_GATT_CCC(bt_uart_ccc_changed,
				   BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));


int ble_get_char(void) {

  //k_mutex_lock(&ble_uart_mutex, K_FOREVER);
  int n;
  u8_t c;
  
  n = ring_buf_get(&ble_in_ringbuf, &c, 1);
  
  //k_mutex_unlock(&ble_uart_mutex);
  if (n == 1) {
    return c;
  }
  return -1;
 
}

void ble_put_char(int i) {

  u8_t c = (u8_t) i;

  bt_gatt_notify(NULL, &bt_uart.attrs[2], &c, 1);
}

void ble_printf(char *format, ...) {

  va_list arg;
  va_start(arg, format);
  int len;
  static char print_buffer[4096];

  len = vsnprintf(print_buffer, 4096,format, arg);
  va_end(arg);

  int offset = 0;
  int bytes_left = len;
  while (bytes_left > 0) {
    int chunk_size = (bytes_left > 20 ? 20 : bytes_left);
    bt_gatt_notify(NULL,&bt_uart.attrs[2], print_buffer+offset, chunk_size);
    bytes_left -= chunk_size;
    offset += chunk_size;
  }
}

int ble_inputline(char *buffer, int size) {
  int n = 0;
  int c;
  for (n = 0; n < size - 1; n++) {

    c = ble_get_char();
    switch (c) {
    case 127: /* fall through to below */
    case '\b': /* backspace character received */
      if (n > 0)
        n--;
      buffer[n] = 0;
      ble_put_char('\b'); /* output backspace character */
      n--; /* set up next iteration to deal with preceding char location */
      break;
    case '\n': /* fall through to \r */
    case '\r':
      buffer[n] = 0;
      return n;
    default:
      if (c != -1 && c < 256) {
	ble_put_char(c);
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

static VALUE bas_set_level(VALUE *args, int argn) {
  if (argn != 1) {
    return enc_sym(symrepr_nil());
  }
  int bat_lvl = dec_i(args[0]);

  bt_gatt_bas_set_battery_level((u8_t)bat_lvl);
  return enc_sym(symrepr_true());
}

static VALUE hello_world(VALUE *args, int argn) {
  if (argn != 0) {
    return enc_sym(symrepr_nil());
  }

  ble_printf("Hello world\n\r");

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
  ring_buf_init(&ble_in_ringbuf, sizeof(ble_in_ring_buffer), ble_in_ring_buffer);
  /*
  while (true) {
    uart_line_ctrl_get(dev, LINE_CTRL_DTR, &dtr);
    if (dtr) {
      break;
    } else {
      k_sleep(100);
    }
  }

  ret = uart_line_ctrl_set(dev, LINE_CTRL_DCD, 1);
  if (ret) {
    //LOG_WRN("Failed to set DCD, ret code %d", ret);
  }

  ret = uart_line_ctrl_set(dev, LINE_CTRL_DSR, 1);
  if (ret) {
    //LOG_WRN("Failed to set DSR, ret code %d", ret);
  }
  

  k_busy_wait(1000000);

  ret = uart_line_ctrl_get(dev, LINE_CTRL_BAUD_RATE, &baudrate);
  if (ret) {
    //LOG_WRN("Failed to get baudrate, ret code %d", ret);
  } else {
    //LOG_INF("Baudrate detected: %d", baudrate);
  }

  uart_irq_callback_set(dev, interrupt_handler);
  
  uart_irq_rx_enable(dev);
  */

  err = bt_enable(bt_ready);

  if (err) {
    ble_printf("Error enabling BLE\n");
  }

  bt_set_name("BLE_TOOL_NRF52_FW");
  bt_conn_cb_register(&conn_callbacks);
  //bt_conn_auth_cb_register(&auth_cb_display);

  ble_printf("Allocating input/output buffers\n\r");
  size_t len = 1024;
  char *str = malloc(1024);
  char *outbuf = malloc(4096);
  int res = 0;

  heap_state_t heap_state;

  res = symrepr_init();
  if (res)
    ble_printf("Symrepr initialized.\n\r");
  else {
    ble_printf("Error initializing symrepr!\n\r");
    return;
  }
  int heap_size = 2048;
  res = heap_init(heap_size);
  if (res)
    ble_printf("Heap initialized. Free cons cells: %u\n\r", heap_num_free());
  else {
    ble_printf("Error initializing heap!\n\r");
    return;
  }

  res = eval_cps_init(false);
  if (res)
    ble_printf("Evaluator initialized.\n\r");
  else {
    ble_printf("Error initializing evaluator.\n\r");
  }

  if (extensions_add("set-bat-level", bas_set_level)) {
    ble_printf("set-bat-level extension added.\n\r");
  } else {
    ble_printf("set-bat-level extension failed!\n\r");
  }

  if (extensions_add("hello-world", hello_world)) {
    ble_printf("hello-world extension added.\n\r");
  } else {
    ble_printf("hello-world extension failed!\n\r");
  }
	
  VALUE prelude = prelude_load();
  eval_cps_program(prelude);

  ble_printf("Lisp REPL started (BLE_TOOL_NRF52_FW)!\n\r");
	
  while (1) {
    k_sleep(100);
    ble_printf("# ");
    memset(str,0,len);
    memset(outbuf,0, 1024);
    ble_inputline(str, len);
    ble_printf("\n\r");

    if (strncmp(str, ":info", 5) == 0) {
      ble_printf("##(BLE_TOOL_NRF52_FW)#######################################\n\r");
      ble_printf("Used cons cells: %lu \n\r", heap_size - heap_num_free());
      ble_printf("ENV: "); simple_snprint(outbuf,4095, eval_cps_get_env()); ble_printf("%s \n\r", outbuf);
      heap_get_state(&heap_state);
      ble_printf("GC counter: %lu\n\r", heap_state.gc_num);
      ble_printf("Recovered: %lu\n\r", heap_state.gc_recovered);
      ble_printf("Marked: %lu\n\r", heap_state.gc_marked);
      ble_printf("Free cons cells: %lu\n\r", heap_num_free());
      ble_printf("############################################################\n\r");
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
	ble_printf("Error\n");
      } else {
	ble_printf("> "); simple_snprint(outbuf, 1023, t); ble_printf("%s \n\r", outbuf);
      }
    }
  }

  symrepr_del();
  heap_del();

}
