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
#define ACTOR_NAME "uptime"
#define DEFAULT_LOG_CONFIG "/etc/fty/ftylog.cfg"

int main (int argc, char *argv [])
{
    char *log_config = NULL;
    bool verbose = false;
    int argn;

    for (argn = 1; argn < argc; argn++) {
        if (streq (argv [argn], "--help")
        ||  streq (argv [argn], "-h")) {
            puts ("fty-kpi-power-uptime [options] ...");
            puts ("  --verbose / -v         verbose output");
            puts ("  --help / -h            this information");
            puts ("  --config / -c          agent configuration file");
            return 0;
        }
        else
        if (streq (argv [argn], "--verbose")
        ||  streq (argv [argn], "-v"))
            verbose = true;
        else
        if (streq (argv [argn], "--config")
            ||  streq (argv [argn], "-c")) {
            const char *zconf_path = argv[argn++];

            zconfig_t *zconf = zconfig_load (zconf_path);
            log_config = zconfig_get (zconf, "log/config", NULL);
        }
        else {
            printf ("Unknown option: %s\n", argv [argn]);
        }
    }

    if (!log_config)
        log_config = DEFAULT_LOG_CONFIG;
    ftylog_setInstance (ACTOR_NAME, log_config);
    Ftylog *log = ftylog_getInstance ();

    if (verbose == true) {
        ftylog_setVeboseMode (log);
    }

    log_info ("%s - Main daemon", ACTOR_NAME);
    static const char* endpoint = "ipc://@/malamute";
    //XXX: this comes from old project name - uptime. Don't change if you're not
    //     willing to maintain code which moves things from old path :)
    static const char* dir = "/var/lib/fty/fty-kpi-power-uptime";

    zactor_t *server = zactor_new (fty_kpi_power_uptime_server, ACTOR_NAME);
    zstr_sendx (server, "CONFIG", dir, NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONNECT", endpoint, NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONSUMER", "METRICS", "^status.ups@.*", NULL);
    zstr_sendx (server, "CONSUMER", "METRICS", "^status@.*", NULL);
    zstr_sendx (server, "CONSUMER", "ASSETS", "^datacenter.unknown@.*", NULL);
    zstr_sendx (server, "CONSUMER", "ASSETS", "^datacenter.N_A@.*", NULL);
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
