/*
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
*/

#include "uptime.h"

int main(int argc, char**argv) {

    static const char* endpoint = "ipc://@/malamute";
    static const char* dir = "/var/lib/bios/uptime/";

    zactor_t *server = zactor_new (upt_server, "uptime");
    zstr_sendx (server, "CONNECT", endpoint, NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONSUMER", "METRICS", "status.ups.*", NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONFIG", dir, NULL);
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
