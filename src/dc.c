/*  =========================================================================
    dc - DC information

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
    dc - DC information
@discuss
@end
*/

#include "fty_kpi_power_uptime_classes.h"

struct _dc_t {
    int64_t last_update;
    uint64_t total;
    uint64_t offline;
    zlistx_t *ups;  // list of offline upses
};

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

static int
s_str_comparator (const void *a, const void *b)
{
    return strcmp ((const char*) a, (const char*) b);
}

dc_t*
dc_new (void)
{
    dc_t *self = (dc_t*) zmalloc (sizeof (dc_t));
    if (!self)
        return NULL;
    self->last_update = zclock_mono () / 1000LL;
    self->total = 0LL;
    self->offline = 0LL;
    self->ups = zlistx_new ();
    zlistx_set_duplicator (self->ups, s_str_duplicator);
    zlistx_set_destructor (self->ups, s_str_destructor);
    zlistx_set_comparator (self->ups, s_str_comparator);
    return self;
}

void
dc_destroy (dc_t **self_p)
{
    if (!self_p || !*self_p)
        return;

    dc_t *self = *self_p;

    zlistx_destroy (&self->ups);
    free (self);
    *self_p = NULL;
}

bool
dc_is_offline (dc_t *self)
{
    assert (self);

    return zlistx_size (self->ups) > 0;
}

void
dc_set_offline (dc_t *self, char* ups)
{
    assert (self);

    void *foo = zlistx_find (self->ups, ups);
    if (!foo)
    {
        zlistx_add_end (self->ups, ups);
        zsys_debug ("uptime: ups %s set offline", ups);
    }
}

void
dc_set_online (dc_t *self, char* ups)
{
    assert (self);

    void *foo = zlistx_find (self->ups, ups);
    if (!foo)
        return;
    zlistx_delete (self->ups, foo);
}

void
dc_uptime (dc_t *self, uint64_t* total, uint64_t* offline)
{
    assert (self);

    int64_t now = (zclock_mono() / 1000LL);
    int64_t time_diff = (now - self->last_update);

    // XXX: this should not happen due mono clock used, but we already got
    // weird total time, so newer add negative number typecasted to unsigned
    if (time_diff > 0LL) {

        self->total += time_diff;
        if (dc_is_offline (self))
            self->offline += time_diff;

        self->last_update = now;
    }

    *total = self->total;
    *offline = self->offline;
}

zframe_t *
dc_pack (dc_t *self)
{

    assert (self);

    zmsg_t *msg = zmsg_new ();
    zmsg_addstr (msg, "dc0x01");
    zmsg_addstrf (msg, "%"PRIi64, self->last_update);
    zmsg_addstrf (msg, "%"PRIu64, self->total);
    zmsg_addstrf (msg, "%"PRIu64, self->offline);
    zmsg_addstrf (msg, "%zu", zlistx_size (self->ups));

    char *ups = (char*) zlistx_first (self->ups);
    while (ups != NULL)
    {
        zmsg_addstr (msg, ups);
        ups = (char*) zlistx_next (self->ups);
    }

/* Note: the CZMQ_VERSION_MAJOR comparisons below actually assume versions
 * we know and care about - v3.0.2 (our legacy default, already obsoleted
 * by upstream), and v4.x that is in current upstream master. If the API
 * evolves later (incompatibly), these macros will need to be amended.
 */
    size_t size = 0;    // Note: the zmsg_encode() and zframe_size()
                        // below return a platform-dependent size_t,
                        // and unlike some other similar code, here we
                        // do not use fixed uint64_t due to protocol
    zframe_t *frame = NULL;
#if CZMQ_VERSION_MAJOR == 3
    byte *buffer;
    size = zmsg_encode (msg, &buffer);

    if (!buffer) {
        zmsg_destroy (&msg);
        return NULL;
    }
#else
    frame = zmsg_encode (msg);
    size = zframe_size (frame);
#endif

    if (size == 0) {
        zmsg_destroy (&msg);
        return NULL;
    }

#if CZMQ_VERSION_MAJOR == 3
    frame = zframe_new (buffer, size);
    free (buffer);
#endif
    zmsg_destroy (&msg);

    return frame;
}

dc_t *
dc_unpack (zframe_t *frame)
{
    assert (frame);

    zmsg_t *msg = NULL;
#if CZMQ_VERSION_MAJOR == 3
    msg = zmsg_decode (zframe_data (frame), zframe_size (frame));
#else
    msg = zmsg_decode (frame);
#endif

    if (!msg)
        return NULL;

    if (zmsg_size (msg) < 4) {
        zmsg_destroy (&msg);
        return NULL;
    }

    char *magic = zmsg_popstr (msg);
    if (!magic || !streq (magic, "dc0x01")) {
        zsys_error ("unknown magic %s", magic);
        zmsg_destroy (&msg);
        return NULL;
    }

    zstr_free (&magic);

    int64_t last_update;
    uint64_t total, offline;
    size_t size;

    char *s_last_update = zmsg_popstr (msg);
    char *s_total = zmsg_popstr (msg);
    char *s_offline = zmsg_popstr (msg);
    char *s_size = zmsg_popstr (msg);

    if (!s_last_update || !s_total || !s_offline || !s_size) {
        zsys_error ("missing last_update, total, offline or size fields");
        zstr_free (&s_last_update);
        zstr_free (&s_total);
        zstr_free (&s_offline);
        zstr_free (&s_size);
        zmsg_destroy (&msg);
        return NULL;
    }

    sscanf (s_last_update, "%"SCNi64, &last_update);
    sscanf (s_total, "%"SCNu64, &total);
    sscanf (s_offline, "%"SCNu64, &offline);
    sscanf (s_size, "%zu", &size);

    zstr_free (&s_offline);
    zstr_free (&s_total);
    zstr_free (&s_last_update);
    zstr_free (&s_size);

    dc_t *dc = dc_new ();

    if (!dc) {
        zmsg_destroy (&msg);
        return NULL;
    }

    dc->last_update = last_update;
    dc->total = total;
    dc->offline = offline;

    if (size != 0) {
        char *ups = zmsg_popstr (msg);
        size_t i = 0;
        while (ups && i != size) {
            dc_set_offline (dc, ups);
            zstr_free (&ups);
            ups = zmsg_popstr (msg);
            i += 1;
        }
        zstr_free (&ups);
    }

    zmsg_destroy (&msg);
    return dc;
}

void
dc_print (dc_t *self) {
    zsys_debug ("last_update: %"PRIi64"\n", self->last_update);
    zsys_debug ("total: %"PRIu64"\n", self->total);
    zsys_debug ("offline: %"PRIu64"\n", self->offline);
    zsys_debug ("ups (%zu):\n", zlistx_size (self->ups));

    for (char* i = (char*) zlistx_first (self->ups);
               i != NULL;
               i = (char*) zlistx_next (self->ups))
    {
        zsys_debug ("    %s\n", i);
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class.

void
dc_test (bool verbose)
{
    printf (" * dc: ");

    //  @selftest
    dc_t *dc = dc_new ();

    assert (!dc_is_offline (dc));

    dc_set_online (dc, "UPS007");
    assert (!dc_is_offline (dc));

    dc_set_offline (dc, "UPS001");
    assert (dc_is_offline (dc));

    uint64_t total, offline;

    zclock_sleep (3000);
    dc_uptime (dc, &total, &offline);
    printf ("total:  %"PRIi64, total);
    printf ("offline:  %"PRIi64, offline);

    assert (total > 1);
    assert (offline > 1);

    dc_set_online (dc, "UPS001");
    assert (!dc_is_offline (dc));

    zclock_sleep (3000);
    dc_uptime (dc, &total, &offline);
    assert (total > 2);
    assert (offline > 1);

    dc_destroy (&dc);

    // pack/unpack
    dc = dc_new ();
    // XXX: dirty tricks for test - class intentionally don't allow to test those directly
    dc->last_update = 42;
    dc->total = 1042;
    dc->offline = 17;
    dc_set_offline (dc, "UPS001");
    dc_set_offline (dc, "UPS002");
    dc_set_offline (dc, "UPS003");

    zframe_t *frame = dc_pack (dc);
    assert (frame);

    dc_t *dc2 = dc_unpack (frame);
    assert (dc2);
    assert (dc->last_update == dc2->last_update);
    assert (dc->total == dc2->total);
    assert (dc->offline == dc2->offline);
    assert (dc_is_offline (dc2));
    assert (zlistx_size (dc2->ups) == 3);
    assert (streq ((char*) zlistx_first (dc2->ups), "UPS001"));
    assert (streq ((char*) zlistx_next (dc2->ups), "UPS002"));
    assert (streq ((char*) zlistx_next (dc2->ups), "UPS003"));

    zframe_destroy (&frame);
    dc_destroy (&dc2);
    dc_destroy (&dc);

    // pack/unpack of empty struct
    dc = dc_new ();
    frame = dc_pack (dc);
    assert (frame);

    dc2 = dc_unpack (frame);
    assert (dc2);
    assert (zlistx_size (dc2->ups) == 0);
    zframe_destroy (&frame);
    dc_destroy (&dc2);
    dc_destroy (&dc);

    //  @end

    printf ("OK\n");
}
