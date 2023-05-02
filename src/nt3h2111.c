/*
	MIT License

	Copyright (c) 2023 Julian Scheffers

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

// Note: NT3H2111 is a little-endian device.

#include <string.h>
#include <sdkconfig.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include "nt3h2111.h"
#include "managed_i2c.h"



// The number read helper.
#define GEN_RW_UINT(bits, type) \
	static inline type read_uint##bits(const uint8_t *ptr) __attribute__((always_inline)); \
	static inline type read_uint##bits(const uint8_t *ptr) { \
		type out = 0; \
		for (size_t i = 0; i < bits/8; i++) { \
			out |= ptr[i] << (i*8); \
		} \
		return out; \
	} \
	static inline void write_uint##bits(type in, uint8_t *ptr) __attribute__((always_inline)); \
	static inline void write_uint##bits(type in, uint8_t *ptr) { \
		for (size_t i = 0; i < bits/8; i++) { \
			ptr[i] = in >> (i*8); \
		} \
	}

GEN_RW_UINT(16, uint16_t)
GEN_RW_UINT(24, uint32_t)
GEN_RW_UINT(32, uint32_t)
GEN_RW_UINT(40, uint32_t)
GEN_RW_UINT(48, uint32_t)
GEN_RW_UINT(56, uint32_t)
GEN_RW_UINT(64, uint32_t)



// Initialise the device.
esp_err_t nt3h2111_init(NT3H2111 *device, int i2c_bus, int i2c_address) {
	device->i2c_bus     = i2c_bus;
	device->i2c_address = i2c_address;
	return ESP_OK;
}

// Do some cleanup.
esp_err_t nt3h2111_destroy(NT3H2111 *device) {
	// No-op function.
	(void) device;
	return ESP_OK;
}


// Get device serial number.
esp_err_t nt3h2111_get_serial(NT3H2111 *device, uint64_t *serial) {
	uint8_t tmp[6];
	esp_err_t res = nt3h2111_read_raw(device, 1, 6, tmp);
	if (res == 0) {
		*serial = read_uint48(tmp);
	}
	return res;
}

// Get device capability container.
esp_err_t nt3h2111_get_cc(NT3H2111 *device, uint32_t *cc) {
	uint8_t tmp[4];
	esp_err_t res = nt3h2111_read_raw(device, 12, 4, tmp);
	if (res == 0) {
		*cc = read_uint32(tmp);
	}
	return res;
}

// Set device capability container.
esp_err_t nt3h2111_set_cc(NT3H2111 *device, uint32_t cc) {
	uint8_t tmp[4];
	write_uint32(cc, tmp);
	return nt3h2111_write_raw(device, 12, 4, tmp);
}

// Get NDEF encoded NDEF data.
esp_err_t nt3h2111_get_ndef(NT3H2111 *device, size_t *len, uint8_t **data) {
	// Read the header.
	size_t  ndef_len;
	uint8_t offset;
	uint8_t tmp[16];
	nt3h2111_read_page(device, 1, tmp);
	
	// Check magic value.
	if (tmp[0] != 0x03) {
		return ESP_ERR_NOT_FOUND;
	}
	// Determine length.
	if (tmp[1] == 0xff) {
		ndef_len = (tmp[2] << 8) | tmp[3];
		offset   = 4;
	} else {
		ndef_len = tmp[1];
		offset   = 2;
	}
	
	// Allocate memory.
	uint8_t *buf = malloc(ndef_len);
	if (!buf) {
		return ESP_ERR_NO_MEM;
	}
	
	// Read the rest from userdata.
	esp_err_t res = nt3h2111_read_user(device, offset, ndef_len, buf);
	if (res) {
		free(buf);
		return res;
	}
	
	// Output the datas.
	*len  = ndef_len;
	*data = buf;
	return ESP_OK;
}

// Set NDEF encoded NDEF data.
esp_err_t nt3h2111_set_ndef(NT3H2111 *device, size_t len, const uint8_t data[]) {
	// Format header.
	uint8_t tmp[4] = { 0x03, 0x00, 0x00, 0x00 };
	size_t offset;
	esp_err_t res;
	if (len >= NT3H2111_USERDATA_LEN - 4) {
		return ESP_ERR_NO_MEM;
	} else if (len >= 0xff) {
		tmp[1] = 0xff;
		tmp[2] = len >> 8;
		tmp[3] = len;
		res    = nt3h2111_write_user(device, 0, 4, tmp);
		if (res) return res;
		offset = 4;
	} else {
		tmp[1] = len;
		res    = nt3h2111_write_user(device, 0, 2, tmp);
		if (res) return res;
		offset = 2;
	}
	
	// Write the datas.
	res = nt3h2111_write_user(device, offset, len, data);
	
	// Write the terminating verse.
	tmp[0] = 0xfe;
	nt3h2111_write_user(device, offset + len, 1, tmp);
	
	return ESP_OK;
}



// Read user data EEPROM.
esp_err_t nt3h2111_read_user(NT3H2111 *device, uint16_t offset, uint8_t len, uint8_t data[]) {
	if (!len) return ESP_OK;
	
	// Bounds check.
	if (offset + len > NT3H2111_USERDATA_LEN) {
		return ESP_ERR_INVALID_ARG;
	}
	
	// Forward the read.
	return nt3h2111_read_raw(device, 16+offset, len, data);
}

// Write user data EEPROM.
esp_err_t nt3h2111_write_user(NT3H2111 *device, uint16_t offset, uint8_t len, const uint8_t data[]) {
	if (!len) return ESP_OK;
	
	// Bounds check.
	if (offset + len > NT3H2111_USERDATA_LEN) {
		return ESP_ERR_INVALID_ARG;
	}
	
	// Forward the write.
	return nt3h2111_write_raw(device, 16+offset, len, data);
}

// Read SRAM.
esp_err_t nt3h2111_read_sram(NT3H2111 *device, uint8_t offset, uint8_t len, uint8_t data[]) {
	if (!len) return ESP_OK;
	
	// Bounds check.
	if (offset + len > NT3H2111_SRAM_LEN) {
		return ESP_ERR_INVALID_ARG;
	}
	
	// Forward the read.
	return nt3h2111_read_raw(device, 248*16+offset, len, data);
}

// Write SRAM.
esp_err_t nt3h2111_write_sram(NT3H2111 *device, uint8_t offset, uint8_t len, const uint8_t data[]) {
	if (!len) return ESP_OK;
	
	// Bounds check.
	if (offset + len > NT3H2111_SRAM_LEN) {
		return ESP_ERR_INVALID_ARG;
	}
	
	// Forward the write.
	return nt3h2111_write_raw(device, 248*16+offset, len, data);
}


// Unaligned raw read.
esp_err_t nt3h2111_read_raw(NT3H2111 *device, uint8_t offset, uint8_t len, uint8_t data[]) {
	esp_err_t res = 0;
	uint8_t tmp[16];
	
	// First page misaligned read.
	size_t misalign = offset & 15;
	if (misalign) {
		size_t rlen = 16-misalign < len ? 16-misalign : len;
		
		// Read page.
		res = nt3h2111_read_page(device, offset / 16, tmp);
		if (res) return res;
		memcpy(data, tmp + misalign, rlen);
		
		// Increment some pointers.
		data   += rlen;
		len    -= rlen;
		offset += rlen;
	}
	
	// Intermediary pages aligned read.
	while (len >= 16) {
		// Read page.
		res = nt3h2111_read_page(device, offset / 16, data);
		if (res) return res;
		
		// Increment some pointers.
		data   += 16;
		len    -= 16;
		offset += 16;
	}
	
	// Last page misaligned read.
	if (len) {
		// Read page.
		res = nt3h2111_read_page(device, offset / 16, tmp);
		if (res) return res;
		memcpy(data, tmp, len);
	}
	
	return ESP_OK;
}

// Unaligned raw write.
esp_err_t nt3h2111_write_raw(NT3H2111 *device, uint8_t offset, uint8_t len, const uint8_t data[]) {
	esp_err_t res = 0;
	uint8_t tmp[16];
	
	// First page misaligned read.
	size_t misalign = offset & 15;
	if (misalign) {
		size_t rlen = 16-misalign < len ? 16-misalign : len;
		
		// Read page.
		res = nt3h2111_read_page(device, offset / 16, tmp);
		if (res) return res;
		memcpy(tmp + misalign, data, rlen);
		
		// Re-write page.
		res = nt3h2111_write_page(device, offset / 16, tmp);
		if (res) return res;
		
		// Increment some pointers.
		data   += rlen;
		len    -= rlen;
		offset += rlen;
	}
	
	// Intermediary pages aligned read.
	while (len >= 16) {
		// Read page.
		res = nt3h2111_write_page(device, offset / 16, data);
		if (res) return res;
		
		// Increment some pointers.
		data   += 16;
		len    -= 16;
		offset += 16;
	}
	
	// Last page misaligned read.
	if (len) {
		// Read page.
		res = nt3h2111_read_page(device, offset / 16, tmp);
		if (res) return res;
		memcpy(tmp, data, len);
		
		// Re-write page.
		res = nt3h2111_write_page(device, offset / 16, tmp);
		if (res) return res;
	}
	
	return ESP_OK;
}

// Used to keep delays between writes and subsequent accesses.
static int64_t last_write_time = 0;

// Page-aligned raw read.
esp_err_t nt3h2111_read_page(NT3H2111 *device, uint8_t page, uint8_t data[16]) {
	// Wait for EEPROM write if required.
	while (last_write_time + 5000 > esp_timer_get_time()) sched_yield();
	// Send read command.
	return i2c_read_reg(device->i2c_bus, device->i2c_address, page, data, 16);
}

// Page-aligned raw write.
esp_err_t nt3h2111_write_page(NT3H2111 *device, uint8_t page, const uint8_t data[16]) {
	// Wair for EEPROM write if required.
	while (last_write_time + 5000 > esp_timer_get_time()) sched_yield();
	// Set EEPROM write timer if applicable.
	/*if (page < 284 || page >= 248+64/16) */last_write_time = esp_timer_get_time();
	// Send write command.
	return i2c_write_reg_n(device->i2c_bus, device->i2c_address, page, data, 16);
}

