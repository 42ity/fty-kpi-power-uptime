/*  =========================================================================
    fty_kpi_power_uptime - Main daemon

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

// fty_kpi_power_uptime - Main daemon

#include "fty_kpi_power_uptime_server.h"
#include <fty_log.h>

#define ACTOR_NAME "fty-kpi-power-uptime" // mlm server address
#define MLM_ENDPOINT "ipc://@/malamute"

static void usage(const char* pname)
{
    printf("%s [options] ...\n", (pname ? pname : "fty-kpi-power-uptime"));
    printf("  -c/--config     load agent configuration file\n");
    printf("  -v/--verbose    set output to verbose\n");
    printf("  -h/--help       show this information\n");
}

int main(int argc, char* argv[])
{
    bool verbose = false;

    for (int argn = 1; argn < argc; argn++) {
        const char* arg = argv[argn];
        const char* param = ((argn + 1) < argc) ? argv[argn + 1] : NULL;

        if (streq(arg, "-c") || streq(arg, "--config")) {
            if (!param) {
                fprintf(stderr, "Missing parameter (option: %s)\n", arg);
                return EXIT_FAILURE;
            }

            zconfig_t* zconf = zconfig_load(param);
            if (!zconf) {
                fprintf(stderr, "Config load failed (param: %s)\n", param);
                return EXIT_FAILURE;
            }
            verbose = streq(zconfig_get(zconf, "server/verbose", "0"), "1");
            zconfig_destroy(&zconf);
        }
        else if (streq(arg, "-v") || streq(arg, "--verbose")) {
            verbose = true;
        }
        else if (streq(arg, "-h") || streq(arg, "--help")) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return EXIT_FAILURE;
        }
    }

    // init logging
    ftylog_setInstance(ACTOR_NAME, FTY_COMMON_LOGGING_DEFAULT_CFG);
    if (verbose) {
        ftylog_setVerboseMode(ftylog_getInstance());
    }

    log_info("%s - starting", ACTOR_NAME);

    // instanciate main actor
    zactor_t* actor = zactor_new(fty_kpi_power_uptime_server, const_cast<char*>(ACTOR_NAME));
    if (!actor) {
        log_error("actor creation failed");
        return EXIT_FAILURE;
    }

    // XXX: this comes from old project name - uptime. Don't change if you're not
    //     willing to maintain code which moves things from old path :)
    zstr_sendx(actor, "CONFIG", "/var/lib/fty/fty-kpi-power-uptime", NULL);
    zsock_wait(actor);

    zstr_sendx(actor, "CONNECT", MLM_ENDPOINT, NULL);
    zsock_wait(actor);

    zstr_sendx(actor, "CONSUMER", "ASSETS", "^datacenter.unknown@.*", NULL);
    zstr_sendx(actor, "CONSUMER", "ASSETS", "^datacenter.N_A@.*", NULL);
    zsock_wait(actor);

    log_info("%s - started", ACTOR_NAME);

    // Main loop, accept any message back from server
    // copy from src/malamute.c under MPL license
    while (!zsys_interrupted) {
        char* msg = zstr_recv(actor);
        if (!msg)
            break;
        log_trace("%s: recv msg '%s'", ACTOR_NAME, msg);
        zstr_free(&msg);
    }

    log_info("%s - ending", ACTOR_NAME);

    zactor_destroy(&actor);

    log_info("%s - ended", ACTOR_NAME);

    return EXIT_SUCCESS;
}
