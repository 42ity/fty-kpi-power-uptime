/*  =========================================================================
    fty_kpi_power_uptime_server - Actor computing uptime

    Copyright (C) 2014 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

#pragma once
#include "upt.h"
#include <czmq.h>
#include <fty_proto.h>

struct fty_kpi_power_uptime_server_t
{
    int    request_counter;
    upt_t* upt;
    char*  dir;
    char*  name;
};

//  Create new fty-kpi-power-uptime instance.
//
//      zactor_t *server = zactor_new (fty_kpi_power_uptime_server, "uptime");
//
//  Destroy the instance
//      zsock_destroy (&server);
//
//  All fty_kpi_power_uptime_server commands are synchronous, so must be followed by zsock_wait
//
//  Set verbose mode
//      zsock_send (server, "VERBOSE");
//      zsock_wait (server);
//
//  Connect to malamute endpoint (it'll interally subscribe on METRIC stream and ups.status.* messages)
//      zsock_sendx (server, "CONNECT", "ipc://@/malamute", NULL);
//      zsock_wait (server);
//
//  Setup directory for storing the state
//      zsock_sendx (server, "CONFIG", "src/", NULL);
//      zsock_wait (server);
//
void fty_kpi_power_uptime_server(zsock_t* pipe, void* args);

fty_kpi_power_uptime_server_t* fty_kpi_power_uptime_server_new(void);
int                            fty_kpi_power_uptime_server_save_state(fty_kpi_power_uptime_server_t* self);
int                            fty_kpi_power_uptime_server_load_state(fty_kpi_power_uptime_server_t* self);
void                           fty_kpi_power_uptime_server_destroy(fty_kpi_power_uptime_server_t** self_p);
void                           s_set_dc_upses(fty_kpi_power_uptime_server_t* self, fty_proto_t* fmsg);
void fty_kpi_power_uptime_server_set_dir(fty_kpi_power_uptime_server_t* self, const char* dir);
