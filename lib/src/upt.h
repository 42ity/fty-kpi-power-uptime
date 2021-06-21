/*  =========================================================================
    upt - DC uptime

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

/*
//#include <zhash.h>
#include <malamute.h>
#include <zhashx.h>
*/

#include <czmq.h>

struct upt_t
{
    zhashx_t* ups2dc; // map ups name to dc name
    zhashx_t* dc;     // map dc name to dc_t struct
};

///  Create a new upt
upt_t* upt_new(void);

///  Destroy the upt
void upt_destroy(upt_t** self_p);

///  Print properties of object
void upt_print(upt_t* self);

int upt_add(upt_t* self, const char* dc_name, zlistx_t* ups_p);

bool upt_is_offline(upt_t* self, const char* dc_name);

void upt_set_offline(upt_t* self, const char* dc_name);

void upt_set_online(upt_t* self, const char* dc_name);

const char* upt_dc_name(upt_t* self, const char* ups_name);

int upt_uptime(upt_t* self, const char* ups_name, uint64_t* total, uint64_t* offline);

/// save upt_t to file
int upt_save(upt_t* self, const char* file_path);

void upt_print(upt_t* self);

/// load upt_t from file
upt_t* upt_load(const char* file_path);
