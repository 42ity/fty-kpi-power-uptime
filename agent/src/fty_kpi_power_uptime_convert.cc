/*  =========================================================================
    fty_kpi_power_uptime_convert - Converts old binary format state file into new zpl format state file

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

/// fty_kpi_power_uptime_convert - Converts old binary format state file into new zpl format state file

#include "dc.h"

static void s_dc_destructor(void** x)
{
    if (x) { dc_destroy(reinterpret_cast<dc_t**>(x)); }
}

static void s_str_destructor(void** x)
{
    if (x) { zstr_free(reinterpret_cast<char**>(x)); }
}

static void* s_str_duplicator(const void* x)
{
    return x ? strdup(reinterpret_cast<const char*>(x)) : NULL;
}

static int s_load_binary(FILE* file, zhashx_t** ups2dc_p, zhashx_t* dc)
{
    if (ups2dc_p) *ups2dc_p = NULL;

    if (!file) return -1;
    if (!ups2dc_p) return -1;
    if (!dc) return -1;

#if CZMQ_VERSION_MAJOR == 3
    zmsg_t* msg = zmsg_load(nullptr, file);
#else
    zmsg_t* msg = zmsg_load(file);
#endif
    if (!msg) return -2;
    if (!zmsg_is(msg)) { zmsg_destroy(&msg); return -2; }

    char* magic = zmsg_popstr(msg);
    if (!magic || !streq(magic, "upt0x01"))
        { zmsg_destroy(&msg); zstr_free(&magic); return -3; }
    zstr_free(&magic);

    // ups2dc
    zframe_t* frame = zmsg_pop(msg);
    *ups2dc_p = frame ? zhashx_unpack(frame) : NULL;
    if (!(*ups2dc_p))
        { zmsg_destroy(&msg); zframe_destroy(&frame); return -4; }

    zhashx_set_duplicator(*ups2dc_p, s_str_duplicator);
    zhashx_set_destructor(*ups2dc_p, s_str_destructor);
    zframe_destroy(&frame);

    // count
    char* s_size = zmsg_popstr(msg);
    if (!s_size) { zmsg_destroy(&msg); return -4; }

    size_t size = 0;
    sscanf(s_size, "%zu", &size);
    zstr_free(&s_size);

    for (size_t i = 0; i < size; i++) {
        char* key = zmsg_popstr(msg);
        if (!key)
            break;
        frame = zmsg_pop(msg);
        if (!frame) {
            zstr_free(&key);
            break;
        }
        dc_t* dc_item = dc_unpack(frame);
        if (!dc_item) {
            zframe_destroy(&frame);
            zstr_free(&key);
            break;
        }
        if (zhashx_insert(dc, key, dc_item) != 0) {
            // key exist, insert failed
            dc_destroy(&dc_item);
        }
        zframe_destroy(&frame);
        zstr_free(&key);
    }

    zmsg_destroy(&msg);
    return 0; // ok
}

static int s_save_zpl(zhashx_t* ups2dc, zhashx_t* dc, const char* file_path)
{
    if (!ups2dc) return -1;
    if (!dc) return -1;
    if (!file_path) return -1;

    zconfig_t* config_file = zconfig_new("root", nullptr);
    if (!config_file) return -2;

    int j = 1;

    for (void* it = zhashx_first(dc); it; it = zhashx_next(dc)) {
        const char* dc_name = const_cast<char*>(reinterpret_cast<const char*>(zhashx_cursor(dc)));
        //dc_t* dc_struc = reinterpret_cast<dc_t*>(it);

        // list of datacenters
        char* path = zsys_sprintf("dc_list/dc.%d", j);
        zconfig_putf(config_file, path, "%s", dc_name);
        zstr_free(&path);

        // self->ups2dc - list of upses for each dc
        int i = 1;
        for (void* it2 = zhashx_first(ups2dc); it2; it2 = zhashx_next(ups2dc)) {
            char* dc1 = reinterpret_cast<char*>(it2);
            if (streq(dc1, dc_name)) {
                path = zsys_sprintf("dc_upses/%s/ups.%d", dc1, i);
                zconfig_putf(config_file, path, "%s", reinterpret_cast<const char*>(zhashx_cursor(ups2dc)));
                zstr_free(&path);
                i++;
            }
        }

        j++;
    }

    // save the state file
    int r = zconfig_save(config_file, file_path);
    zconfig_destroy(&config_file);
    return (r == 0) ? 0 : -3;
}

int main(int argc, char* argv[])
{
    bool verbose = false;

    for (int argn = 1; argn < argc; argn++) {
        const char* arg = argv[argn];
        if (streq(arg, "--help") || streq(arg, "-h")) {
            printf("fty-kpi-power-uptime-convert [options] [file_name] [old_path] [new_path]\n");
            printf("Converts bios_proto state file to fty_proto state file.\n");
            printf("  -v/--verbose     verbose output\n");
            printf("  -h/--help        this information\n");
            return EXIT_SUCCESS;
        }
        else if (streq(arg, "--verbose") || streq(arg, "-v")) {
            verbose = true;
        }
    }

    if ((verbose && argc != 5) || (!verbose && argc != 4)) {
        fprintf(stderr, "Invalid arguments list\n");
        return EXIT_FAILURE;
    }

    const char* file_name = verbose ? argv[2] : argv[1];
    const char* old_path  = verbose ? argv[3] : argv[2];
    const char* new_path  = verbose ? argv[4] : argv[3];

    printf("file_name = '%s'\n", file_name);
    printf("old_path = '%s'\n", old_path);
    printf("new_path = '%s'\n", new_path);

    zhashx_t* dc = zhashx_new();
    if (!dc) {
        fprintf(stderr, "zhashx_new failed\n");
        return EXIT_FAILURE;
    }
    zhashx_set_destructor(dc, s_dc_destructor);

    zfile_t* file = zfile_new(old_path, file_name);
    if (!(file && zfile_is_regular(file) && zfile_is_readable(file) && (zfile_input(file) == 0))) {
        fprintf(stderr, "File is invalid\n");
        return EXIT_FAILURE;
    }

    FILE* fp = zfile_handle(file);
    if (!fp) {
        fprintf(stderr, "zfile_handle failed\n");
        zfile_close(file);
        zfile_destroy(&file);
        return EXIT_FAILURE;
    }

    bool success = false;

    zhashx_t* ups2dc = NULL;
    int r = s_load_binary(fp, &ups2dc, dc);
    if (r == 0) {
        char* tmp = zsys_sprintf("%s/%s", new_path, file_name);
        r = s_save_zpl(ups2dc, dc, tmp);
        zstr_free(&tmp);
        if (r == 0) {
            success = true;
        }
        else {
            fprintf(stderr, "s_save_zpl failed (r: %d)\n", r);
        }
    }
    else {
        fprintf(stderr, "s_load_binary failed (r: %d)\n", r);
    }

    zfile_close(file);
    zfile_destroy(&file);
    zhashx_destroy(&ups2dc);
    zhashx_destroy(&dc);

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
