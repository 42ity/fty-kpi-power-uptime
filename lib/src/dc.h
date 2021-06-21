/*  =========================================================================
    dc - DC information

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
#include <czmq.h>

struct dc_t
{
    int64_t last_update;
    uint64_t total;
    uint64_t offline;
    zlistx_t *ups; // list of offline upses
};

///  Create a new dc
dc_t *dc_new (void);

///  Destroy the dc
void dc_destroy (dc_t **self_p);

/// Get total value
uint64_t dc_total (dc_t *self);

/// Get off line value
uint64_t dc_off_line (dc_t *self);

/// Set total value
void set_dc_total (dc_t *self, uint64_t total);

/// Set off line value
void set_dc_off_line (dc_t *self, uint64_t offline);

///  Return if dc is offline
bool dc_is_offline (dc_t *self);

///  Set UPS as as offline
void dc_set_offline (dc_t *self, char *ups);

/// Set UPS as online
void dc_set_online (dc_t *self, char *ups);

/// Compute uptime, return result in total/offline pointers
void dc_uptime (dc_t *self, uint64_t *total, uint64_t *offline);

/// pack dc to frame
zframe_t *dc_pack (dc_t *self);

/// unpack dc class from frame
dc_t *dc_unpack (zframe_t *frame);

///  Print properties of object
void dc_print (dc_t *self);
