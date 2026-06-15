// USB ethernet and networking on the Pico using TinyUSB

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Matthew Bennett
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef USB_NETWORK_H
#define USB_NETWORK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lwip/ip.h>
#include <stdbool.h>

/**
 * @brief Initialize TinyUSB and add a lwIP network interface for USB networking.
 *
 * Generates a locally-administered MAC address from the board's unique ID,
 * creates a lwIP netif, and blocks until the interface reports as up.
 *
 * @param ownip     IPv4 address to assign to the device interface.
 * @param netmask   Subnet mask for the interface.
 * @param gateway   Default gateway address (use 0.0.0.0 if none).
 * @param init_lwip If true, calls lwip_init() before adding the netif.
 *                  Pass false when lwIP was already initialized (e.g. by cyw43).
 * @return true  Network interface started and is up.
 * @return false TinyUSB initialization or lwIP netif creation failed.
 */
bool usb_network_init(const ip4_addr_t *ownip, const ip4_addr_t *netmask, const ip4_addr_t *gateway, bool init_lwip);

/**
 * @brief Check whether the USB device stack is enumerated and ready.
 *
 * @return true  TinyUSB reports the device is ready (connected to a host).
 * @return false Device is not yet connected or has been disconnected.
 */
bool usb_network_is_up();

/**
 * @brief Drive TinyUSB and lwIP processing; must be called from the main loop.
 *
 * Calls tud_task() to handle USB events, then passes any buffered Ethernet
 * frames up the lwIP stack and services lwIP software timers.
 */
void usb_network_update();

/**
 * @brief Tear down the USB network interface and release lwIP resources.
 *
 * Removes the lwIP netif and deinitializes TinyUSB. Safe to call even if
 * initialization never completed.
 */
void usb_network_deinit();

#ifdef __cplusplus
}
#endif

#endif // USB_NETWORK_H