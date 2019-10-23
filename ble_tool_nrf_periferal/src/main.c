
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

#include <logging/log.h>
LOG_MODULE_REGISTER(cdc_acm_echo, LOG_LEVEL_INF);


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
				LOG_ERR("Drop %u bytes", recv_len - rb_len);
			}

			LOG_DBG("tty fifo -> ringbuf %d bytes", rb_len);
		}

		if (uart_irq_tx_ready(dev)) {
			u8_t buffer[64];
			int rb_len, send_len;

			rb_len = ring_buf_get(&out_ringbuf, buffer, sizeof(buffer));
			if (!rb_len) {
				LOG_DBG("Ring buffer empty, disable TX IRQ");
				uart_irq_tx_disable(dev);
				continue;
			}

			send_len = uart_fifo_fill(dev, buffer, rb_len);
			if (send_len < rb_len) {
				LOG_ERR("Drop %d bytes", rb_len - send_len);
			}

			LOG_DBG("ringbuf -> tty fifo %d bytes", send_len);
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
	static char print_buffer[1025];

	len = vsnprintf(print_buffer, 1024,format, arg);
	va_end(arg);

	int num_written = 0;
	while (len - num_written > 0) {
		unsigned int key = irq_lock();
		num_written +=
			ring_buf_put(&out_ringbuf,
				         (print_buffer + num_written),
				         (len - num_written));
		uart_irq_tx_enable(dev);
		irq_unlock(key);
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



void main(void)
{

	u32_t baudrate, dtr = 0U;
	int ret;

	dev = device_get_binding("CDC_ACM_0");
	if (!dev) {
		LOG_ERR("CDC ACM device not found");
		return;
	}

	ring_buf_init(&in_ringbuf, sizeof(in_ring_buffer), in_ring_buffer);
	ring_buf_init(&out_ringbuf, sizeof(out_ring_buffer), out_ring_buffer);

	LOG_INF("Wait for DTR");

	while (true) {
		uart_line_ctrl_get(dev, LINE_CTRL_DTR, &dtr);
		if (dtr) {
			break;
		} else {
			/* Give CPU resources to low priority threads. */
			k_sleep(100);
		}
	}

	LOG_INF("DTR set");

	/* They are optional, we use them to test the interrupt endpoint */
	ret = uart_line_ctrl_set(dev, LINE_CTRL_DCD, 1);
	if (ret) {
		LOG_WRN("Failed to set DCD, ret code %d", ret);
	}

	ret = uart_line_ctrl_set(dev, LINE_CTRL_DSR, 1);
	if (ret) {
		LOG_WRN("Failed to set DSR, ret code %d", ret);
	}

	/* Wait 1 sec for the host to do all settings */
	k_busy_wait(1000000);

	ret = uart_line_ctrl_get(dev, LINE_CTRL_BAUD_RATE, &baudrate);
	if (ret) {
		LOG_WRN("Failed to get baudrate, ret code %d", ret);
	} else {
		LOG_INF("Baudrate detected: %d", baudrate);
	}

	uart_irq_callback_set(dev, interrupt_handler);

	/* Enable rx interrupts */
	uart_irq_rx_enable(dev);




	/*
	usb_printf("Press \'s\' to start\n\r");

	while (getChar() != 's');

	char *data = (char *)malloc(1024);

	usb_printf("ALLOC %s \n\r", (data == NULL) ? "FAILURE" : "SUCCESS");

	free(data);

	data = (char *)malloc(10240);

	usb_printf("ALLOC %s \n\r", (data == NULL) ? "FAILURE" : "SUCCESS");

	free(data);

	data = (char *)malloc(32768);

	usb_printf("ALLOC %s \n\r", (data == NULL) ? "FAILURE" : "SUCCESS");

	free(data);
 	*/

	usb_printf("Allocating input/output buffers\n\r");
	size_t len = 1024;
	char *str = malloc(1024);
	char *outbuf = malloc(1024);
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

	//VALUE prelude = prelude_load();
	//eval_cps_program(prelude);

	usb_printf("Lisp REPL started (ZEPHYR - NRF52)!\n\r");

	while (1) {
		k_sleep(100);
		usb_printf("# ");
		memset(str,0,len);
		memset(outbuf,0, 1024);
		inputline(str, len);
		usb_printf("\n\r");

		if (strncmp(str, ":info", 5) == 0) {
			usb_printf("##(FMRC)####################################################\n\r");
			usb_printf("Used cons cells: %lu \n\r", heap_size - heap_num_free());
			usb_printf("ENV: "); simple_snprint(outbuf,1023, eval_cps_get_env()); usb_printf("%s \n\r", outbuf);
			heap_get_state(&heap_state);
			usb_printf("GC counter: %lu\n\r", heap_state.gc_num);
			usb_printf("Recovered: %lu\n\r", heap_state.gc_recovered);
			usb_printf("Marked: %lu\n\r", heap_state.gc_marked);
			usb_printf("Free cons cells: %lu\n\r", heap_num_free());
			usb_printf("############################################################\n\r");
			memset(outbuf,0, 1024);
		} else if (strncmp(str, ":quit", 5) == 0) {
			break;
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
