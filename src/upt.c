/*  =========================================================================
    upt - DC uptime

    Copyright (C) 2014 - 2015 Eaton                                        
                                                                           
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

#include "upt_classes.h"

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
    zhashx_set_key_duplicator (self->dc, s_str_duplicator);
    zhashx_set_key_destructor (self->dc, s_str_destructor);
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
upt_add(upt_t *self, const char *dc_name, zlistx_t *ups)
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

int
    upt_save (upt_t *self, FILE* fp)
{
    assert (self);
    assert (fp);

    zmsg_t *msg = zmsg_new ();
    if (!msg)
        return -1;

    zmsg_addstr (msg, "upt0x01");

    zframe_t *frame = zhashx_pack (self->ups2dc);

    if (!frame) {
        zmsg_destroy (&msg);
        return -1;
    }

    zmsg_addmem (msg, zframe_data (frame), zframe_size (frame));
    zframe_destroy (&frame);

    zmsg_addstrf (msg, "%zu", zhashx_size (self->dc));
    dc_t *dc = (dc_t*) zhashx_first (self->dc);
    while (dc)
    {
        char *key = (char*) zhashx_cursor (self->dc);
        zmsg_addstr (msg, key);

        frame = dc_pack (dc);
        //TODO: !frame
        zmsg_addmem (msg, zframe_data (frame), zframe_size (frame));
        zframe_destroy (&frame);
        dc = (dc_t*) zhashx_next (self->dc);
    }

    int r = zmsg_save (msg, fp);
    zmsg_destroy (&msg);
    return r;
}

upt_t*
    upt_load (FILE* fp)
{
    assert (fp);

    zmsg_t *msg = zmsg_load (NULL, fp);

    if (!msg || !zmsg_is (msg)) {
        zmsg_destroy (&msg);
        zsys_error ("upt: can't allocate msg");
        return NULL;
    }

    char *magic = zmsg_popstr (msg);
    if (!magic || !streq (magic, "upt0x01")) {
        zmsg_destroy (&msg);
        zsys_error ("upt: invalid magic, expected 'upt0x01', got '%s'", magic);
        zstr_free (&magic);
        return NULL;
    }
    zstr_free (&magic);

    zframe_t *frame = zmsg_pop (msg);
    if (!frame) {
        zmsg_destroy (&msg);
        zsys_error ("upt: can't decode frame");
        return NULL;
    }

    zhashx_t *ups2dc = zhashx_unpack (frame);
    zframe_destroy (&frame);

    if (!ups2dc) {
        zmsg_destroy (&msg);
        zsys_error ("upt: can't unpack to ups2dc hash");
        return NULL;
    }

    upt_t *self = upt_new ();
    zhashx_destroy (&self->ups2dc);
    self->ups2dc = ups2dc;

    char *s_size = zmsg_popstr (msg);
    if (!s_size) {
        upt_destroy (&self);
        zmsg_destroy (&msg);
        zsys_error ("upt: missing size specifier");
        return NULL;
    }

    size_t size;
    sscanf (s_size, "%zu", &size);
    zstr_free (&s_size);

    if (size != 0) {

        size_t i = 0;
        char *key;
        dc_t *dc;
        do {
            key = zmsg_popstr (msg);
            if (!key)
                break;
            frame = zmsg_pop (msg);
            if (!frame) {
                zstr_free (&key);
                break;
            }
            dc = dc_unpack (frame);
            if (!dc) {
                zframe_destroy (&frame);
                zstr_free (&key);
                break;
            }
            zhashx_insert (self->dc, key, dc);
            dc_destroy (&dc);
            zframe_destroy (&frame);
            zstr_free (&key);
            i++;
        } while (i != size);

    }
    zmsg_destroy (&msg);

    return self;
}

//  --------------------------------------------------------------------------
//  Self test of this class.

void
upt_test (bool verbose)
{
    printf (" * upt: ");

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
    zlistx_add_end (ups, "UPS001");
    zlistx_add_end (ups, "UPS007");

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
    zclock_sleep (1000);

    r = upt_uptime (uptime, "DC007", &total, &offline);
    assert (r == 0);
    assert (total == 1);
    assert (offline == 1);

    upt_set_online (uptime, "UPS007");
    assert (!upt_is_offline(uptime, "DC007"));

    zclock_sleep (1000);
    r = upt_uptime (uptime, "DC007", &total, &offline);
    assert (r == 0);
    assert (total == 2);
    assert (offline == 1);

    // test UPS removal
    ups = zlistx_new ();
    zlistx_add_end (ups, "UPS007");
    r = upt_add (uptime, "DC007", ups);
    assert (r == 0);
    zlistx_destroy (&ups);

    dc_name = upt_dc_name (uptime, "UPS001");
    assert (!dc_name);

    upt_destroy (&uptime);

    //save/load
    uptime = upt_new ();
    FILE *fp = fopen ("src/state", "w");
    upt_save (uptime, fp);
    fflush (fp);
    fclose (fp);

    FILE *fp2 = fopen ("src/state", "r");
    upt_t *uptime2 = upt_load(fp2);
    fclose (fp2);
    assert (uptime2);

    upt_destroy (&uptime2);
    upt_destroy (&uptime);

    r = unlink ("src/state");
    assert (r == 0);
    //  @end

    printf ("OK\n");
}
