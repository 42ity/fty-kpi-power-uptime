/*  =========================================================================
    upt - DC uptime

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
    upt - DC uptime
@discuss
@end
*/

#include "fty_kpi_power_uptime_classes.h"

struct _upt_t {
    zhashx_t *ups2dc;       //map ups name to dc name
    zhashx_t *dc;           //map dc name to dc_t struct
};

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

upt_t*
upt_new ()
{
    upt_t *self = (upt_t*) zmalloc (sizeof (upt_t));
    if (!self)
        return NULL;

    self->dc = zhashx_new ();
    zhashx_set_key_duplicator (self->dc, s_str_duplicator);
    zhashx_set_key_destructor (self->dc, s_str_destructor);
    zhashx_set_destructor (self->dc, s_dc_destructor);
    self->ups2dc = zhashx_new ();
    zhashx_set_key_duplicator (self->ups2dc, s_str_duplicator);
    zhashx_set_key_destructor (self->ups2dc, s_str_destructor);
    zhashx_set_duplicator (self->ups2dc, s_str_duplicator);
    zhashx_set_destructor (self->ups2dc, s_str_destructor);
    return self;
}

void
upt_destroy (upt_t **self_p)
{
    if (!self_p || !*self_p)
        return;

    upt_t *self = *self_p;

    zhashx_destroy (&self->dc);
    zhashx_destroy (&self->ups2dc);
    free (self);

    *self_p = NULL;
}

int
upt_add (upt_t *self, const char *dc_name, zlistx_t *ups)
{
    assert (self);
    assert (dc_name);

    dc_t *dc = (dc_t*) zhashx_lookup (self->dc, dc_name);
    if (!dc) {
        dc = dc_new ();
        zhashx_insert (self->dc, dc_name, dc);
    }
    else {
        // dc exists, so
        //  1.) remove all entries from ups2dc, which are not in ups
        //  2.) setup them as online
        zlistx_t *keys = zhashx_keys (self->ups2dc);
        for (char *ups_name = (char*) zlistx_first (keys);
                   ups_name != NULL;
                   ups_name = (char*) zlistx_next (keys))
        {
            char *sdc = (char*) zhashx_lookup (self->ups2dc, ups_name);
            if (sdc && streq (sdc, dc_name) &&
                !zlistx_find (ups, ups_name)) {
                zhashx_delete (self->ups2dc, ups_name);
                dc_set_online (dc, ups_name);
            }
        }
        zlistx_destroy (&keys);
    }

    if (ups) {
        for (char *ups_name = (char*) zlistx_first (ups);
                ups_name != NULL;
                ups_name = (char*) zlistx_next (ups))
        {
            zhashx_update (self->ups2dc, ups_name, (void*) dc_name);
        }
    }
    return 0;
}

bool
upt_is_offline (upt_t *self, const char* dc_name)
{
    assert (self);
    assert (dc_name);

    dc_t *dc = (dc_t*) zhashx_lookup (self->dc, dc_name);
    if (!dc)
        return false;

    return dc_is_offline (dc);
}

void
upt_set_offline (upt_t *self, const char* ups_name)
{
    assert (self);
    assert (ups_name);

    const char *dc_name = upt_dc_name (self, ups_name);
    if (!dc_name)
        return;

    dc_t *dc = (dc_t*) zhashx_lookup (self->dc, dc_name);
    if (!dc)
        return;

    dc_set_offline (dc, (char*) ups_name);
}

void
upt_set_online (upt_t *self, const char* ups_name)
{
    assert (self);
    assert (ups_name);

    const char *dc_name = upt_dc_name (self, ups_name);
    if (!dc_name)
        return;

    dc_t *dc = (dc_t*) zhashx_lookup (self->dc, dc_name);
    if (!dc)
        return;

    dc_set_online (dc, (char*) ups_name);
}

const char*
upt_dc_name (upt_t *self, const char* ups_name)
{
    assert (self);
    assert (ups_name);

    char *dc = (char*) zhashx_lookup (self->ups2dc, ups_name);
    if (!dc)
        return NULL;

    return dc;
}

int
upt_uptime (upt_t *self, const char* dc_name, uint64_t* total, uint64_t* offline)
{
    assert (self);
    assert (dc_name);

    dc_t *dc = (dc_t*) zhashx_lookup (self->dc, dc_name);
    if (!dc) {
        *total = 0;
        *offline = 0;
        return -1;
    }

    dc_uptime (dc, total, offline);

    return 0;
}

FTY_KPI_POWER_UPTIME_EXPORT void
    upt_print (upt_t* self)
{
    log_debug ("self: <%p>\n", (void*) self);
    log_debug ("self->ups2dc: \n");
    for (char* dc_name = (char*) zhashx_first (self->ups2dc);
               dc_name != NULL;
               dc_name = (char*) zhashx_next (self->ups2dc))
    {
        log_debug ("\t'%s' : '%s'\n",
                (char*) zhashx_cursor (self->ups2dc),
                dc_name);
    }
    log_debug ("self->dc: \n");
    for (dc_t *dc = (dc_t*) zhashx_first (self->dc);
               dc != NULL;
               dc = (dc_t*) zhashx_next (self->dc))
    {
        log_debug ("'%s' <%p>:\n",
                (char*) zhashx_cursor (self->dc),
                (void*) dc);
        dc_print (dc);
        log_debug ("----------\n");
    }
}

int
upt_save (upt_t *self, const char *file_path)
{
    assert (self);
    assert (file_path);

    zconfig_t *config_file = zconfig_new ("root", NULL);
    int i = 1;
    int j = 1;

    //self->dc
    dc_t *dc_struc = NULL;

    for (dc_struc = (dc_t *) zhashx_first (self->dc);
         dc_struc != NULL;
         dc_struc = (dc_t *) zhashx_next (self->dc))
    {
        i = 1;

        // list of datacenters
        char *dc_name = (char*) zhashx_cursor (self->dc);
        char *path = zsys_sprintf ("dc_list/dc.%d", j);
        zconfig_putf (config_file, path, "%s", dc_name);
        zstr_free (&path);

        // self->ups2dc - list of upses for each dc
        for (char *dc = (char*) zhashx_first (self->ups2dc);
             dc != NULL;
             dc = (char*) zhashx_next (self->ups2dc) )
        {
            if (streq (dc, dc_name))
            {
                path = zsys_sprintf ("dc_upses/%s/ups.%d", dc, i);
                char *ups = (char *) zhashx_cursor (self->ups2dc);

                zconfig_put (config_file, path , ups);
                zstr_free (&path);
                i++;
            }
        }
        j++;

    } // for

    // save the state file
    int rv = zconfig_save (config_file, file_path);

    dc_destroy (&dc_struc);
    zconfig_destroy (&config_file);
    return rv;
}

upt_t
*upt_load (const char *file_path)
{
    upt_t *upt = upt_new ();

    zconfig_t *config_file = zconfig_load (file_path);
    if (!config_file)
        return upt;


    char *dc_name = NULL;
    char *ups = NULL;

    for (int i = 1; ; i++)
    {
        char *path = zsys_sprintf ("dc_list/dc.%d", i);
        dc_name = zconfig_get (config_file, path, NULL);
        zstr_free (&path);
        if (!dc_name)
            break;

        dc_t *dc = dc_new ();
        zhashx_insert (upt->dc, dc_name, dc);

        for (int j = 1; ; j++)
        {
            path = zsys_sprintf ("dc_upses/%s/ups.%d", dc_name, j);
            ups = zconfig_get (config_file, path, NULL);
            zstr_free (&path);

            if (!ups)
                break;
            zhashx_insert (upt->ups2dc, ups, (void*) dc_name);
        }
    }

    zstr_free (&dc_name);
    zstr_free (&ups);
    zconfig_destroy (&config_file);

    return upt;
}

//  --------------------------------------------------------------------------
//  Self test of this class.

void
upt_test (bool verbose)
{
    printf (" * upt: ");

    // Note: If your selftest reads SCMed fixture data, please keep it in
    // src/selftest-ro; if your test creates filesystem objects, please
    // do so under src/selftest-rw. They are defined below along with a
    // usecase (asert) to make compilers happy.
    const char *SELFTEST_DIR_RO = "src/selftest-ro";
    const char *SELFTEST_DIR_RW = "src/selftest-rw";
    assert (SELFTEST_DIR_RO);
    assert (SELFTEST_DIR_RW);
    // std::string str_SELFTEST_DIR_RO = std::string(SELFTEST_DIR_RO);
    // std::string str_SELFTEST_DIR_RW = std::string(SELFTEST_DIR_RW);

    //state file - save/load
    // TODO: Should this be same or different as "src/state" file
    // made by fty_kpi_power_uptime_server.c logic and tests?
    // Originally they were "./state" and "./src/state" so I kept them different
    char *state_file = zsys_sprintf ("%s/state-upt", SELFTEST_DIR_RW);
    assert (state_file != NULL);

    //  @selftest
    upt_t *uptime = upt_new ();
    assert (uptime);
    upt_destroy (&uptime);
    assert (!uptime);

    uptime = upt_new ();
    uint64_t total, offline;
    int r;

    r = upt_uptime (uptime, "TEST", &total, &offline);
    assert (r == -1);
    assert (total == 0);
    assert (offline == 0);

    zlistx_t *ups = NULL;
    r = upt_add (uptime, "UPS007", ups);
    assert (r == 0);
    zlistx_destroy (&ups);

    r = upt_uptime (uptime, "TEST", &total, &offline);
    assert (r == -1);
    assert (total == 0);
    assert (offline == 0);

    const char *dc_name;
    dc_name = upt_dc_name (uptime, "UPS007");
    assert (!dc_name);

    ups = zlistx_new ();
    zlistx_add_end (ups, (void*)"UPS001");
    zlistx_add_end (ups, (void*)"UPS007");

    r = upt_add (uptime, "DC007", ups);
    assert (r == 0);
    zlistx_destroy (&ups);

    dc_name = upt_dc_name (uptime, "UPS007");
    assert (streq (dc_name, "DC007"));

    dc_name = upt_dc_name (uptime, "UPS042");
    assert (!dc_name);

    upt_set_online (uptime, "UPS001");
    assert (!upt_is_offline (uptime, "DC007"));

    upt_set_offline (uptime, "UPS007");
    assert (upt_is_offline (uptime, "DC007"));

    upt_set_online (uptime, "DC042");
    assert (upt_is_offline (uptime, "DC007"));

    // uptime works with 1sec precision, so let sleep
    zclock_sleep (2000);

    r = upt_uptime (uptime, "DC007", &total, &offline);
    assert (r == 0);
    assert (total > 1);
    assert (offline > 1);

    upt_set_online (uptime, "UPS007");
    assert (!upt_is_offline(uptime, "DC007"));

    zclock_sleep (1000);
    r = upt_uptime (uptime, "DC007", &total, &offline);
    assert (r == 0);
    assert (total > 2);
    assert (offline > 1);

    // test UPS removal
    ups = zlistx_new ();
    zlistx_add_end (ups, (void*)"UPS007");
    r = upt_add (uptime, "DC007", ups);

    assert (r == 0);
    zlistx_destroy (&ups);

    dc_name = upt_dc_name (uptime, "UPS001");
    assert (!dc_name);

    upt_t *uptime2 = upt_new ();
    //    upt_t *uptime3 = upt_new ();
    zlistx_t *ups2 = zlistx_new ();
    ups = zlistx_new ();

    zlistx_add_end (ups, (void*)"UPS007");
    zlistx_add_end (ups, (void*)"UPS006");
    zlistx_add_end (ups, (void*)"UPS005");
    zlistx_add_end (ups2, (void*)"UPS001");
    zlistx_add_end (ups2, (void*)"UPS002");
    zlistx_add_end (ups2, (void*)"UPS003");

    r = upt_add (uptime2, "DC007", ups);
    assert (r == 0);
    r = upt_add (uptime2, "DC006", ups2);
    assert (r == 0);

    zlistx_add_end (ups2, (void*)"UPS033");
    r = upt_add (uptime2, "DC006", ups2);
    assert (r == 0);

    r = upt_save (uptime2, state_file);
    assert (r == 0);

    upt_t *uptime3 = upt_load (state_file);
    assert (zhashx_size (uptime3->ups2dc) == (zlistx_size (ups) + zlistx_size (ups2)));
    assert (zhashx_size (uptime3->dc) == 2);

    zstr_free (&state_file);

    zlistx_destroy (&ups);
    zlistx_destroy (&ups2);
    upt_destroy (&uptime);
    upt_destroy (&uptime2);
    upt_destroy (&uptime3);

    printf ("OK\n");
}
