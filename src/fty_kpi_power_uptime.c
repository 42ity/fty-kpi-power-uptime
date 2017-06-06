/*  =========================================================================
    fty_kpi_power_uptime - Main daemon

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
    fty_kpi_power_uptime - Main daemon
@discuss
@end
*/

#include "fty_kpi_power_uptime_classes.h"

int main (int argc, char *argv [])
{
    bool verbose = false;
    int argn;
    for (argn = 1; argn < argc; argn++) {
        if (streq (argv [argn], "--help")
        ||  streq (argv [argn], "-h")) {
            puts ("fty-kpi-power-uptime [options] ...");
            puts ("  --verbose / -v         verbose test output");
            puts ("  --help / -h            this information");
            return 0;
        }
        else
        if (streq (argv [argn], "--verbose")
        ||  streq (argv [argn], "-v"))
            verbose = true;
        else {
            printf ("Unknown option: %s\n", argv [argn]);
        }
    }
    //  Insert main code here
    if (verbose)
        zsys_info ("fty_kpi_power_uptime - Main daemon");
    static const char* endpoint = "ipc://@/malamute";
    //XXX: this comes from old project name - uptime. Don't change if you're not
    //     willing to maintain code which moves things from old path :)
    static const char* dir = "/var/lib/fty/fty-kpi-power-uptime";

    zactor_t *server = zactor_new (fty_kpi_power_uptime_server, "uptime");
    if (verbose) {
        zstr_sendx (server, "VERBOSE", NULL);
        zsock_wait (server);
    }
    zstr_sendx (server, "CONFIG", dir, NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONNECT", endpoint, NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONSUMER", "METRICS", "^status.ups@.*", NULL);
    zstr_sendx (server, "CONSUMER", "METRICS", "^status@.*", NULL);
    zstr_sendx (server, "CONSUMER", "ASSETS", "^datacenter.unknown@.*", NULL);
    zsock_wait (server);

    //  Accept and print any message back from server
    //  copy from src/malamute.c under MPL license
    while (true) {
        char *message = zstr_recv (server);
        if (message) {
            puts (message);
            free (message);
        }
        else {
            puts ("interrupted");
            break;
        }
    }

    zactor_destroy (&server);
    return 0;
}
