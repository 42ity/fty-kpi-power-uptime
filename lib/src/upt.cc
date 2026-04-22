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

/// upt - DC uptime

#include "upt.h"
#include "dc.h"
#include <fty_log.h>

static void s_dc_destructor(void** x)
{
    if (x) dc_destroy(reinterpret_cast<dc_t**>(x));
}

static void s_str_destructor(void** x)
{
    if (x) zstr_free(reinterpret_cast<char**>(x));
}

static void* s_str_duplicator(const void* x)
{
    return x ? strdup(reinterpret_cast<const char*>(x)) : NULL;
}

upt_t* upt_new()
{
    upt_t* self = static_cast<upt_t*>(zmalloc(sizeof(*self)));
    if (self) {
        self->dc = zhashx_new();
        self->ups2dc = zhashx_new();

        if (!(self->dc && self->ups2dc)) {
            upt_destroy(&self);
            return NULL;
        }

        zhashx_set_key_duplicator(self->dc, s_str_duplicator);
        zhashx_set_key_destructor(self->dc, s_str_destructor);
        zhashx_set_destructor(self->dc, s_dc_destructor);

        zhashx_set_key_duplicator(self->ups2dc, s_str_duplicator);
        zhashx_set_key_destructor(self->ups2dc, s_str_destructor);
        zhashx_set_duplicator(self->ups2dc, s_str_duplicator);
        zhashx_set_destructor(self->ups2dc, s_str_destructor);
    }
    return self;
}

void upt_destroy(upt_t** self_p)
{
    if (self_p && (*self_p)) {
        upt_t* self = *self_p;
        zhashx_destroy(&self->dc);
        zhashx_destroy(&self->ups2dc);
        free(self);
        *self_p = nullptr;
    }
}

int upt_add(upt_t* self, const char* dc_name, zlistx_t* ups)
{
    if (!self) return -1;
    if (!dc_name) return -2;

    dc_t* dc = reinterpret_cast<dc_t*>(zhashx_lookup(self->dc, dc_name));
    if (!dc) {
        dc = dc_new();
        zhashx_insert(self->dc, dc_name, dc);
    }
    else {
        // dc exists, so
        //  1.) remove all entries from ups2dc, which are not in ups
        //  2.) setup them as online
        zlistx_t* keys = zhashx_keys(self->ups2dc);
        for (void* it = zlistx_first(keys); it; it = zlistx_next(keys)) {
            char* ups_name = reinterpret_cast<char*>(it);
            char* sdc = reinterpret_cast<char*>(zhashx_lookup(self->ups2dc, ups_name));
            if (sdc && streq(sdc, dc_name) && !zlistx_find(ups, ups_name)) {
                zhashx_delete(self->ups2dc, ups_name);
                dc_set_online(dc, ups_name);
            }
        }
        zlistx_destroy(&keys);
    }

    if (ups) {
        for (void* it = zlistx_first(ups); it; it = zlistx_next(ups)) {
            char* ups_name = reinterpret_cast<char*>(it);
            zhashx_update(self->ups2dc, ups_name, const_cast<char*>(dc_name));
        }
    }

    return 0;
}

bool upt_is_offline(upt_t* self, const char* dc_name)
{
    if (!self) return false;
    if (!dc_name) return false;

    dc_t* dc = reinterpret_cast<dc_t*>(zhashx_lookup(self->dc, dc_name));

    return dc ? dc_is_offline(dc) : false;
}

void upt_set_offline(upt_t* self, const char* ups_name)
{
    if (!self) return;
    if (!ups_name) return;

    const char* dc_name = upt_dc_name(self, ups_name);
    if (!dc_name)
        return;

    dc_t* dc = reinterpret_cast<dc_t*>(zhashx_lookup(self->dc, dc_name));
    if (dc) {
        dc_set_offline(dc, const_cast<char*>(ups_name));
    }
}

void upt_set_online(upt_t* self, const char* ups_name)
{
    if (!self) return;
    if (!ups_name) return;

    const char* dc_name = upt_dc_name(self, ups_name);
    if (!dc_name)
        return;

    dc_t* dc = reinterpret_cast<dc_t*>(zhashx_lookup(self->dc, dc_name));
    if (dc) {
        dc_set_online(dc, const_cast<char*>(ups_name));
    }
}

const char* upt_dc_name(upt_t* self, const char* ups_name)
{
    if (!self) return NULL;
    if (!ups_name) return NULL;

    char* dc = reinterpret_cast<char*>(zhashx_lookup(self->ups2dc, ups_name));
    return dc;
}

int upt_uptime(upt_t* self, const char* dc_name, uint64_t* total, uint64_t* offline)
{
    if (!self) return -1;
    if (!dc_name) return -1;

    dc_t* dc = reinterpret_cast<dc_t*>(zhashx_lookup(self->dc, dc_name));
    if (!dc) {
        *total   = 0;
        *offline = 0;
        return -1; // not found
    }

    dc_uptime(dc, total, offline);

    return 0;
}

int upt_save(upt_t* self, const char* file_path)
{
    if (!self) return -1;
    if (!file_path) return -1;

    zconfig_t* config_file = zconfig_new("root", nullptr);

    // self->dc
    dc_t* dc_struc = nullptr;
    int j = 1;
    for (dc_struc = reinterpret_cast<dc_t*>(zhashx_first(self->dc)); dc_struc != nullptr; dc_struc = reinterpret_cast<dc_t*>(zhashx_next(self->dc)))
    {
        int i = 1;

        // list of datacenters
        char* dc_name = const_cast<char*>(reinterpret_cast<const char*>(zhashx_cursor(self->dc)));
        char* path    = zsys_sprintf("dc_list/dc.%d", j);
        zconfig_putf(config_file, path, "%s", dc_name);
        zstr_free(&path);

        path = zsys_sprintf("dc_data/%s/total", dc_name);
        zconfig_putf(config_file, path, "%" SCNu64, dc_total(dc_struc));
        zstr_free(&path);

        path = zsys_sprintf("dc_data/%s/off_line", dc_name);
        zconfig_putf(config_file, path, "%" SCNu64, dc_off_line(dc_struc));
        zstr_free(&path);

        // self->ups2dc - list of upses for each dc
        for (char* dc = reinterpret_cast<char*>(zhashx_first(self->ups2dc)); dc != nullptr; dc = reinterpret_cast<char*>(zhashx_next(self->ups2dc))) {
            if (streq(dc, dc_name)) {
                path      = zsys_sprintf("dc_upses/%s/ups.%d", dc, i);
                char* ups = const_cast<char*>(reinterpret_cast<const char*>(zhashx_cursor(self->ups2dc)));

                zconfig_put(config_file, path, ups);
                zstr_free(&path);
                i++;
            }
        }

        j++;
    } // for

    // save the state file
    int r = zconfig_save(config_file, file_path);
    dc_destroy(&dc_struc);
    zconfig_destroy(&config_file);
    return r;
}

upt_t* upt_load(const char* file_path)
{
    if (!file_path) return NULL;

    upt_t* upt = upt_new();
    if (!upt) return NULL;

    zconfig_t* config_file = zconfig_load(file_path);
    if (!config_file) return upt;

    char* dc_name = nullptr;
    char* ups     = nullptr;

    for (int i = 1;; i++) {
        char* path = zsys_sprintf("dc_list/dc.%d", i);
        dc_name    = zconfig_get(config_file, path, nullptr);
        zstr_free(&path);
        if (!dc_name)
            break;

        dc_t* dc = dc_new();

        // set total
        uint64_t total = 0;
        path = zsys_sprintf("dc_data/%s/total", dc_name);
        char* s_total = zconfig_get(config_file, path, nullptr);
        zstr_free(&path);
        if (s_total) {
            sscanf(s_total, "%" SCNu64, &total);
            set_dc_total(dc, total);
        }

        // set offline
        uint64_t offline = 0;
        path = zsys_sprintf("dc_data/%s/off_line", dc_name);
        char* s_off_line = zconfig_get(config_file, path, nullptr);
        zstr_free(&path);
        if (s_off_line) {
            sscanf(s_off_line, "%" SCNu64, &offline);
            set_dc_off_line(dc, offline);
        }

        zhashx_insert(upt->dc, dc_name, dc);

        for (int j = 1;; j++) {
            path = zsys_sprintf("dc_upses/%s/ups.%d", dc_name, j);
            ups  = zconfig_get(config_file, path, nullptr);
            zstr_free(&path);

            if (!ups)
                break;
            zhashx_insert(upt->ups2dc, ups, dc_name);
        }
    }

    zstr_free(&dc_name);
    zstr_free(&ups);
    zconfig_destroy(&config_file);

    return upt;
}

void upt_print(upt_t* self)
{
    if (!self) { log_debug("upt_print: self is NULL"); return; }

    log_debug("upt_print: self: <%p>\n", self);

    log_debug("self->ups2dc: \n");
    for (void* it = zhashx_first(self->ups2dc); it; it = zhashx_next(self->ups2dc)) {
        char* dc_name = reinterpret_cast<char*>(it);
        const char* ups_name = reinterpret_cast<const char*>(zhashx_cursor(self->ups2dc));
        log_debug("\t'%s' : '%s'\n", ups_name, dc_name);
    }

    log_debug("self->dc: \n");
    for (void* it = zhashx_first(self->dc); it; it = zhashx_next(self->dc)) {
        dc_t* dc = reinterpret_cast<dc_t*>(it);
        const char* dc_name = reinterpret_cast<const char*>(zhashx_cursor(self->dc));
        log_debug("'%s' <%p>:\n", dc_name, dc);
        dc_print(dc);
        log_debug("----------\n");
    }
}
