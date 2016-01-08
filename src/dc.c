/*  =========================================================================
    dc - DC information

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
    dc - DC information
@discuss
@end
*/

#include "upt_classes.h"

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
    self->last_update = zclock_mono () / 1000;
    self->total = 0;
    self->offline = 0;
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
        zlistx_add_end (self->ups, ups);
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

    int64_t now = zclock_mono() / 1000;
    self->total += (now - self->last_update);
    if (dc_is_offline (self))
        self->offline += (now - self->last_update);

    self->last_update = now;

    *total = self->total;
    *offline = self->offline;
}

void
dc_print (dc_t *self) {
    printf ("last_update: %"PRIi64"\n", self->last_update);
    printf ("total: %"PRIu64"\n", self->total);
    printf ("offline: %"PRIu64"\n", self->offline);
    printf ("ups (%zu):\n", zlistx_size (self->ups));

    for (char* i = (char*) zlistx_first (self->ups);
               i != NULL;
               i = (char*) zlistx_next (self->ups))
    {
        printf ("    %s\n", i);
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

    zclock_sleep (1000);
    dc_uptime (dc, &total, &offline);
    assert (total == 1);
    assert (offline == 1);

    dc_set_online (dc, "UPS001");
    assert (!dc_is_offline (dc));

    zclock_sleep (1000);
    dc_uptime (dc, &total, &offline);
    assert (total == 2);
    assert (offline == 1);

    dc_destroy (&dc);
    //  @end

    printf ("OK\n");
}
