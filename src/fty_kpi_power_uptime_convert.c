/*  =========================================================================
    fty_kpi_power_uptime_convert - Converts old binary format state file into new zpl format state file

    Copyright (C) 2014 - 2017 Eaton

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

/*
@header
    fty_kpi_power_uptime_convert - Converts old binary format state file into new zpl format state file
@discuss
@end
*/

#include "fty_kpi_power_uptime_classes.h"

static void
s_dc_destructor (void ** x)
{
    dc_destroy ((dc_t**) x);
}

static void
s_str_destructor (void ** x)
{
    zstr_free ((char**) x);
}

static void*
s_str_duplicator (const void * x)
{
    return (void*) strdup ((char*) x);
}

static void
s_print (zhashx_t *ups2dc, zhashx_t *dc)
{
    assert (ups2dc);
    assert (dc);
    zsys_debug ("ups2dc: \n");
    for (char* dc_name = (char*) zhashx_first (ups2dc);
               dc_name != NULL;
               dc_name = (char*) zhashx_next (ups2dc))
    {
        zsys_debug ("\t'%s' : '%s'\n",
                (char*) zhashx_cursor (ups2dc), dc_name);
    }
    zsys_debug ("dc: \n");
    for (dc_t *dc_item = (dc_t*) zhashx_first (dc);
               dc_item != NULL;
               dc_item = (dc_t*) zhashx_next (dc))
    {
        zsys_debug ("'%s' <%p>:\n",
                (char*) zhashx_cursor (dc),
                (void*) dc_item);
        dc_print (dc_item);
        zsys_debug ("----------\n");
    }
}

static bool
s_load_binary (FILE *file, zhashx_t **ups2dc_p, zhashx_t **dc_p)
{
    assert (file);
    assert (ups2dc_p);
    assert (dc_p && *dc_p);

    zsys_debug ("TRACE 10");
    zhashx_t *ups2dc = *ups2dc_p;
    zhashx_t *dc = *dc_p;

    zsys_debug ("TRACE 20");
    zmsg_t *msg;
    #if CZMQ_VERSION_MAJOR == 3
        msg = zmsg_load (NULL, file);
    #else
        msg = zmsg_load (file);
    #endif
    zsys_debug ("TRACE 21");
    assert (msg);
    assert (zmsg_is (msg));

    zsys_debug ("TRACE 22");
    char *magic = zmsg_popstr (msg);
    assert (magic);
    assert (streq (magic, "upt0x01"));
    zstr_free (&magic);

    // ups2dc
    zframe_t *frame = zmsg_pop (msg);
    assert (frame);

    zsys_debug ("TRACE 30");
    ups2dc = zhashx_unpack (frame);
    assert (ups2dc);
    zhashx_set_duplicator (ups2dc, s_str_duplicator);
    zhashx_set_destructor (ups2dc, s_str_destructor);
    zframe_destroy (&frame);

    zsys_debug ("TRACE 40");
    // count
    char *s_size = zmsg_popstr (msg);
    assert (s_size);

    size_t size;
    sscanf (s_size, "%zu", &size);
    zstr_free (&s_size);

    zsys_debug ("TRACE 50 size == '%zu'", size);
    if (size == 0)
        return true;

    size_t i = 0;
    char *key;
    dc_t *dc_item;
    do {
        key = zmsg_popstr (msg);
        if (!key)
            break;
        frame = zmsg_pop (msg);
        if (!frame) {
            zstr_free (&key);
            break;
        }
        dc_item = dc_unpack (frame);
        if (!dc_item) {
            zframe_destroy (&frame);
            zstr_free (&key);
            break;
        }
        zhashx_insert (dc, key, dc_item);
        zframe_destroy (&frame);
        zstr_free (&key);
        i++;
    } while (i != size);

    zmsg_destroy (&msg);
    return true;
}

int main (int argc, char *argv [])
{
    bool verbose = false;
    int argn;
    for (argn = 1; argn < argc; argn++) {
        if (streq (argv [argn], "--help")
        ||  streq (argv [argn], "-h")) {
            puts ("fty-kpi-power-uptime-convert [options] [file_name] [old_path] [new_path]");
            puts ("Converts bios_proto state file to fty_proto state file.");
            puts ("  --verbose / -v         verbose test output");
            puts ("  --help / -h            this information");
            return EXIT_SUCCESS;
        }
        else
        if (streq (argv [argn], "--verbose")
        ||  streq (argv [argn], "-v"))
            verbose = true;
    }
    char *file_name = NULL, *old_path = NULL, *new_path = NULL;
    if (verbose) {
        file_name = argv [2];
        old_path = argv [3];
        new_path = argv [4];
    }
    else {
        file_name = argv [1];
        old_path = argv [2];
        new_path = argv [3];
    }

    zsys_debug ("file_name = '%s'", file_name);
    zsys_debug ("old_path = '%s'", old_path);
    zsys_debug ("new_path = '%s'", new_path);


    //  Insert main code here
    if (verbose)
        zsys_info ("fty_kpi_power_uptime_convert - Converts old binary format state file into new zpl format state file");

    zhashx_t *ups2dc = NULL;

    zhashx_t *dc = zhashx_new ();
    assert (dc);
    zhashx_set_key_duplicator (dc, s_str_duplicator);
    zhashx_set_key_destructor (dc, s_str_destructor);
    zhashx_set_destructor (dc, s_dc_destructor);

    zfile_t *file = zfile_new (old_path, file_name);
    assert (file);

    assert (zfile_is_regular (file));
    assert (zfile_is_readable (file));
    assert (zfile_input (file) == 0);

    FILE *fp = zfile_handle (file);
    assert (fp);

    s_load_binary (fp, &ups2dc, &dc);


    zfile_close (file);
    zfile_destroy (&file);

    s_print (ups2dc, dc);

    zhashx_destroy (&ups2dc);
    zhashx_destroy (&dc);

    return 0;
}
