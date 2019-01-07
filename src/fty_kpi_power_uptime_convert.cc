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
s_dc_destructor (void **x)
{
    dc_destroy ((dc_t **) x);
}

static void
s_str_destructor (void **x)
{
    zstr_free ((char **) x);
}

static void *
s_str_duplicator (const void *x)
{
    return (void *) strdup ((char *) x);
}

static void
s_load_binary (FILE *file, zhashx_t **ups2dc_p, zhashx_t *dc)
{
    assert (file);
    assert (ups2dc_p);
    assert (dc);

    zmsg_t *msg = NULL;
#if CZMQ_VERSION_MAJOR == 3
    msg = zmsg_load (NULL, file);
#else
    msg = zmsg_load (file);
#endif
    assert (msg);
    assert (zmsg_is (msg));

    char *magic = zmsg_popstr (msg);
    assert (magic);
    assert (streq (magic, "upt0x01"));
    zstr_free (&magic);

    // ups2dc
    zframe_t *frame = zmsg_pop (msg);
    assert (frame);
    *ups2dc_p = zhashx_unpack (frame);
    assert (*ups2dc_p);

    zhashx_set_duplicator (*ups2dc_p, s_str_duplicator);
    zhashx_set_destructor (*ups2dc_p, s_str_destructor);
    zframe_destroy (&frame);

    // count
    char *s_size = zmsg_popstr (msg);
    assert (s_size);

    size_t size;
    sscanf (s_size, "%zu", &size);
    zstr_free (&s_size);

    if (size == 0)
        return;

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
    return;
}

static void
s_save_zpl (zhashx_t *ups2dc, zhashx_t *dc, const char *file_path)
{
    assert (ups2dc);
    assert (dc);
    assert (file_path);

    zconfig_t *config_file = zconfig_new ("root", NULL);
    int i = 1;
    int j = 1;

    dc_t *dc_struc = NULL;

    for (dc_struc = (dc_t *) zhashx_first (dc);
         dc_struc != NULL;
         dc_struc = (dc_t *) zhashx_next (dc))
    {
        i = 1;

        // list of datacenters
        char *dc_name = (char*) zhashx_cursor (dc);
        char *path = zsys_sprintf ("dc_list/dc.%d", j);
        zconfig_putf (config_file, path, "%s", dc_name);
        zstr_free (&path);

        // self->ups2dc - list of upses for each dc
        for (char *dc = (char*) zhashx_first (ups2dc);
             dc != NULL;
             dc = (char*) zhashx_next (ups2dc) )
        {
            if (streq (dc, dc_name))
            {
                path = zsys_sprintf ("dc_upses/%s/ups.%d", dc, i);
                zconfig_putf (config_file, path , "%s", (char*) zhashx_cursor (ups2dc));
                zstr_free (&path);
                i++;
            }
        }
        j++;

    } // for

    // save the state file
    int rv = zconfig_save (config_file, file_path);
    assert (rv == 0);
    zconfig_destroy (&config_file);
    return;
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

    printf ("file_name = '%s'\n", file_name);
    printf ("old_path = '%s'\n", old_path);
    printf ("new_path = '%s'\n", new_path);

    zhashx_t *ups2dc = NULL;

    zhashx_t *dc = zhashx_new ();
    assert (dc);
    zhashx_set_destructor (dc, s_dc_destructor);

    zfile_t *file = zfile_new (old_path, file_name);
    assert (file);

    assert (zfile_is_regular (file));
    assert (zfile_is_readable (file));
    assert (zfile_input (file) == 0);

    FILE *fp = zfile_handle (file);
    assert (fp);

    s_load_binary (fp, &ups2dc, dc);

    zfile_close (file);
    zfile_destroy (&file);

    char *tmp = zsys_sprintf ("%s/%s", new_path, file_name);
    s_save_zpl (ups2dc, dc, tmp);
    zstr_free (&tmp);

    zhashx_destroy (&ups2dc);
    zhashx_destroy (&dc);

    return 0;
}
