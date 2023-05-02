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

#pragma once

#include <esp_system.h>

#ifdef __cplusplus
extern "C" {
#endif


// Byte size of device user memory.
#define NT3H2111_USERDATA_LEN 884
// Byte size of device SRAM.
#define NT3H2111_SRAM_LEN 64


// Info required to interact with the device.
typedef struct NT3H2111 {
	int i2c_bus;
	int i2c_address;
} NT3H2111;


// Initialise the device.
esp_err_t nt3h2111_init			(NT3H2111 *device, int i2c_bus, int i2c_address);
// Do some cleanup.
esp_err_t nt3h2111_destroy		(NT3H2111 *device);

// Get device serial number.
esp_err_t nt3h2111_get_serial	(NT3H2111 *device, uint64_t *serial);
// Get device capability container.
esp_err_t nt3h2111_get_cc		(NT3H2111 *device, uint32_t *cc);
// Set device capability container.
esp_err_t nt3h2111_set_cc		(NT3H2111 *device, uint32_t cc);
// Get NDEF encoded NDEF data.
esp_err_t nt3h2111_get_ndef		(NT3H2111 *device, size_t *len, uint8_t **data);
// Set NDEF encoded NDEF data.
esp_err_t nt3h2111_set_ndef		(NT3H2111 *device, size_t len, const uint8_t data[]);

// Read user data EEPROM.
esp_err_t nt3h2111_read_user	(NT3H2111 *device, uint16_t offset, uint8_t len, uint8_t data[]);
// Write user data EEPROM.
esp_err_t nt3h2111_write_user	(NT3H2111 *device, uint16_t offset, uint8_t len, const uint8_t data[]);
// Read SRAM.
esp_err_t nt3h2111_read_sram	(NT3H2111 *device, uint8_t offset,  uint8_t len, uint8_t data[]);
// Write SRAM.
esp_err_t nt3h2111_write_sram	(NT3H2111 *device, uint8_t offset,  uint8_t len, const uint8_t data[]);

// Unaligned raw read.
esp_err_t nt3h2111_read_raw		(NT3H2111 *device, uint8_t offset,  uint8_t len, uint8_t data[]);
// Unaligned raw write.
esp_err_t nt3h2111_write_raw	(NT3H2111 *device, uint8_t offset,  uint8_t len, const uint8_t data[]);
// Page-aligned raw read.
esp_err_t nt3h2111_read_page	(NT3H2111 *device, uint8_t page,    uint8_t data[16]);
// Page-aligned raw write.
esp_err_t nt3h2111_write_page	(NT3H2111 *device, uint8_t page,    const uint8_t data[16]);

#ifdef __cplusplus
} // extern "C"
#endif
