//
// Copyright (c) 2022 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
// which is available at https://www.apache.org/licenses/LICENSE-2.0.
//
// SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
//
// Contributors:
//   ZettaScale Zenoh Team, <zenoh@zettascale.tech>
//

#include <stdlib.h>
#include <string.h>
#include "zenoh-pico/config.h"
#include "zenoh-pico/link/manager.h"
#include "zenoh-pico/link/config/udp.h"
#include "zenoh-pico/system/link/udp.h"

#if ZN_LINK_UDP_MULTICAST == 1

z_str_t _zn_parse_port_segment_udp_multicast(const z_str_t address)
{
    z_str_t p_start = strrchr(address, ':');
    if (p_start == NULL)
        return NULL;
    p_start++;

    z_str_t p_end = &address[strlen(address)];

    int len = p_end - p_start;
    z_str_t port = (z_str_t)z_malloc((len + 1) * sizeof(char));
    strncpy(port, p_start, len);
    port[len] = '\0';

    return port;
}

z_str_t _zn_parse_address_segment_udp_multicast(const z_str_t address)
{
    z_str_t p_start = &address[0];
    z_str_t p_end = strrchr(address, ':');

    if (*p_start == '[' && *(p_end - 1) == ']')
    {
        p_start++;
        p_end--;
        int len = p_end - p_start;
        z_str_t ip6_addr = (z_str_t)z_malloc((len + 1) * sizeof(char));
        strncpy(ip6_addr, p_start, len);
        ip6_addr[len] = '\0';

        return ip6_addr;
    }
    else
    {
        int len = p_end - p_start;
        z_str_t ip4_addr_or_domain = (z_str_t)z_malloc((len + 1) * sizeof(char));
        strncpy(ip4_addr_or_domain, p_start, len);
        ip4_addr_or_domain[len] = '\0';

        return ip4_addr_or_domain;
    }

    return NULL;
}

int _zn_f_link_open_udp_multicast(void *arg)
{
    _zn_link_t *self = (_zn_link_t *)arg;

    const z_str_t iface = _z_str_intmap_get(&self->endpoint.config, UDP_CONFIG_IFACE_KEY);
    if (iface == NULL)
        goto ERR;

    clock_t timeout = ZN_CONFIG_SOCKET_TIMEOUT_DEFAULT;
    z_str_t tout = _z_str_intmap_get(&self->endpoint.config, UDP_CONFIG_TOUT_KEY);
    if (tout != NULL)
        timeout = strtol(tout, NULL, 10);

    if (_zn_open_udp_multicast(self->socket.udp.raddr, &self->socket.udp.laddr, timeout, iface) < 0)
        goto ERR;

    return 0;

ERR:
    return -1;
}

int _zn_f_link_listen_udp_multicast(void *arg)
{
    _zn_link_t *self = (_zn_link_t *)arg;

    const z_str_t iface = _z_str_intmap_get(&self->endpoint.config, UDP_CONFIG_IFACE_KEY);
    if (iface == NULL)
        goto ERR_1;

    self->socket.udp.sock = _zn_listen_udp_multicast(self->socket.udp.raddr, ZN_CONFIG_SOCKET_TIMEOUT_DEFAULT, iface);
    if (self->socket.udp.sock < 0)
        goto ERR_1;

    self->socket.udp.msock = _zn_open_udp_multicast(self->socket.udp.raddr, &self->socket.udp.laddr, ZN_CONFIG_SOCKET_TIMEOUT_DEFAULT, iface);
    if (self->socket.udp.msock < 0)
        goto ERR_2;

    return 0;

ERR_2:
    _zn_close_udp_multicast(self->socket.udp.sock, self->socket.udp.msock, self->socket.udp.raddr);

ERR_1:
    return -1;
}

void _zn_f_link_close_udp_multicast(void *arg)
{
    _zn_link_t *self = (_zn_link_t *)arg;

    _zn_close_udp_multicast(self->socket.udp.sock, self->socket.udp.msock, self->socket.udp.raddr);
}

void _zn_f_link_free_udp_multicast(void *arg)
{
    _zn_link_t *self = (_zn_link_t *)arg;

    _zn_free_endpoint_udp(self->socket.udp.laddr);
    _zn_free_endpoint_udp(self->socket.udp.raddr);
}

size_t _zn_f_link_write_udp_multicast(const void *arg, const uint8_t *ptr, size_t len)
{
    const _zn_link_t *self = (const _zn_link_t *)arg;

    return _zn_send_udp_multicast(self->socket.udp.msock, ptr, len, self->socket.udp.raddr);
}

size_t _zn_f_link_write_all_udp_multicast(const void *arg, const uint8_t *ptr, size_t len)
{
    const _zn_link_t *self = (const _zn_link_t *)arg;

    return _zn_send_udp_multicast(self->socket.udp.msock, ptr, len, self->socket.udp.raddr);
}

size_t _zn_f_link_read_udp_multicast(const void *arg, uint8_t *ptr, size_t len, z_bytes_t *addr)
{
    const _zn_link_t *self = (const _zn_link_t *)arg;

    return _zn_read_udp_multicast(self->socket.udp.sock, ptr, len, self->socket.udp.laddr, addr);
}

size_t _zn_f_link_read_exact_udp_multicast(const void *arg, uint8_t *ptr, size_t len, z_bytes_t *addr)
{
    const _zn_link_t *self = (const _zn_link_t *)arg;

    return _zn_read_exact_udp_multicast(self->socket.udp.sock, ptr, len, self->socket.udp.laddr, addr);
}

uint16_t _zn_get_link_mtu_udp_multicast(void)
{
    // @TODO: the return value should change depending on the target platform.
    return 1450;
}

_zn_link_t *_zn_new_link_udp_multicast(_zn_endpoint_t endpoint)
{
    _zn_link_t *lt = (_zn_link_t *)z_malloc(sizeof(_zn_link_t));

    lt->is_reliable = 0;
    lt->is_streamed = 0;
    lt->is_multicast = 1;
    lt->mtu = _zn_get_link_mtu_udp_multicast();

    lt->endpoint = endpoint;

    lt->socket.udp.sock = -1;
    lt->socket.udp.msock = -1;
    z_str_t s_addr = _zn_parse_address_segment_udp_multicast(endpoint.locator.address);
    z_str_t s_port = _zn_parse_port_segment_udp_multicast(endpoint.locator.address);
    lt->socket.udp.raddr = _zn_create_endpoint_udp(s_addr, s_port);
    lt->socket.udp.laddr = NULL;
    z_free(s_addr);
    z_free(s_port);

    lt->open_f = _zn_f_link_open_udp_multicast;
    lt->listen_f = _zn_f_link_listen_udp_multicast;
    lt->close_f = _zn_f_link_close_udp_multicast;
    lt->free_f = _zn_f_link_free_udp_multicast;

    lt->write_f = _zn_f_link_write_udp_multicast;
    lt->write_all_f = _zn_f_link_write_all_udp_multicast;
    lt->read_f = _zn_f_link_read_udp_multicast;
    lt->read_exact_f = _zn_f_link_read_exact_udp_multicast;

    return lt;
}

#endif
