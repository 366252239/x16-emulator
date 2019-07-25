#include <stdio.h>
#include <stdbool.h>
#include "ps2.h"

#define KBD_BUFFER_SIZE 32
uint8_t kbd_buffer[KBD_BUFFER_SIZE];
uint8_t kbd_buffer_read = 0;
uint8_t kbd_buffer_write = 0;

void
kbd_buffer_add(uint8_t code)
{
	if ((kbd_buffer_write + 1) % KBD_BUFFER_SIZE == kbd_buffer_read) {
		// buffer full
		return;
	}

	kbd_buffer[kbd_buffer_write] = code;
	kbd_buffer_write = (kbd_buffer_write + 1) % KBD_BUFFER_SIZE;
}

uint8_t
kbd_buffer_remove()
{
//	return 0x1d;
	if (kbd_buffer_read == kbd_buffer_write) {
		return 0; // empty
	} else {
		uint8_t code = kbd_buffer[kbd_buffer_read];
		kbd_buffer_read = (kbd_buffer_read + 1) % KBD_BUFFER_SIZE;
		return code;
	}
}

static bool sending = false;
static bool has_byte = false;
static uint8_t current_byte;
static int bit_index = 0;
static int data_bits;
static int send_state = 0;

#define HOLD 25 * 8 /* 25 x ~3 cycles at 8 MHz = 75µs */

int ps2_clk_out, ps2_data_out;
int ps2_clk_in, ps2_data_in;

void
ps2_step()
{
	if (!ps2_clk_in && ps2_data_in) { // communication inhibited
		ps2_clk_out = 0;
		ps2_data_out = 0;
		sending = false;
//		printf("PS2: STATE: communication inhibited.\n");
		return;
	} else if (ps2_clk_in && ps2_data_in) { // idle state
//		printf("PS2: STATE: idle\n");
		if (!sending) {
			// get next byte
			if (!has_byte) {
				current_byte = kbd_buffer_remove();
				if (!current_byte) {
					// we have nothing to send
					ps2_clk_out = 1;
					ps2_data_out = 0;
//					printf("PS2: nothing to send.\n");
					return;
				}
//				printf("PS2: current_byte: %x\n", current_byte);
				has_byte = true;
			}

			data_bits = current_byte << 1 | (1 - __builtin_parity(current_byte)) << 9 | (1 << 10);
//			printf("PS2: data_bits: %x\n", data_bits);
			bit_index = 0;
			send_state = 0;
			sending = true;
		}

		if (send_state <= HOLD) {
			ps2_clk_out = 0; // data ready
			ps2_data_out = data_bits & 1;
//			printf("PS2: [%d]sending #%d: %x\n", send_state, bit_index, data_bits & 1);
			if (send_state == 0 && bit_index == 10) {
				// we have sent the last bit, if the host
				// inhibits now, we'll send the next byte
				has_byte = false;
			}
			if (send_state == HOLD) {
				data_bits >>= 1;
				bit_index++;
			}
			send_state++;
		} else if (send_state <= 2 * HOLD) {
//			printf("PS2: [%d]not ready\n", send_state);
			ps2_clk_out = 1; // not ready
			ps2_data_out = 0;
			if (send_state == 2 * HOLD) {
//				printf("XXX bit_index: %d\n", bit_index);
				if (bit_index < 11) {
					send_state = 0;
				} else {
					sending = false;
				}
			}
			if (send_state) {
				send_state++;
			}
		}
	} else {
		printf("PS2: Warning: unknown PS/2 bus state: CLK_IN=%d, DATA_IN=%d\n", ps2_clk_in, ps2_data_in);
		ps2_clk_out = 0;
		ps2_data_out = 0;
	}
}

