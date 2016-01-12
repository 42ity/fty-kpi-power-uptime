/*  =========================================================================
    upt_server - Actor computing uptime

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
    upt_server - Actor computing uptime
@discuss
@end
*/

#include "upt_classes.h"

//TODO: should not be mlm_client_t a part of upt_server_t???
struct _upt_server_t {
    bool verbose;
    upt_t *upt;
    char *dir;
};

//  Create a new upt_server
UPT_EXPORT upt_server_t *
    upt_server_new (void)
{
    upt_server_t *server = (upt_server_t*) zmalloc (sizeof (upt_server_t));
    if (!server)
        return NULL;

    server->verbose = false;
    server->upt = upt_new ();

    return server;
}

//  Destroy the upt_server
UPT_EXPORT void
    upt_server_destroy (upt_server_t **self_p)
{
    if (!self_p || !*self_p)
        return;

    upt_server_t *self = *self_p;
    upt_destroy (&self->upt);
    zstr_free (&self->dir);
    free (self);
    *self_p = NULL;
}

//  Set server verbose
UPT_EXPORT void
    upt_server_verbose (upt_server_t *self)
{
    assert (self);
    self->verbose = true;
}

// SET the DIR
UPT_EXPORT void
    upt_server_set_dir (upt_server_t *self, const char* dir)
{
    assert (self);
    assert (dir);

    zstr_free (&self->dir);
    self->dir = strdup (dir);
}

int
    upt_server_load_state (upt_server_t *self)
{
    assert (self);
    assert (self->dir);

    int r;
    char* path;
    r = asprintf (&path, "%s/%s", self->dir, "state");
    assert (r > 0);

    FILE *fp = fopen (path, "r");
    zstr_free (&path);

    if (!fp)
        return -1;

    upt_t *upt = upt_load (fp);
    fclose (fp);

    if (!upt)
        return -1;

    upt_destroy (&self->upt);
    self->upt = upt;
    return 0;
}

int
    upt_server_save_state (upt_server_t *self)
{
    assert (self);
    assert (self->dir);

    int r;
    char* path;
    r = asprintf (&path, "%s/%s", self->dir, "state.new");
    assert (r > 0);

    FILE *fp = fopen (path, "w");
    zstr_free (&path);

    if (!fp)
        return -1;

    r = upt_save (self->upt, fp);
    fflush (fp);
    fdatasync (fileno (fp));
    fclose (fp);

    if (r != 0)
        return -1;

    char* oldpath;
    char* newpath;
    r = asprintf (&oldpath, "%s/%s", self->dir, "state.new");
    assert (r > 0);
    r = asprintf (&newpath, "%s/%s", self->dir, "state");
    assert (r > 0);
    r = rename (oldpath, newpath);
    zstr_free (&oldpath);
    zstr_free (&newpath);

    if (r != 0)
        return r;

    return 0;
}

static void
s_str_destructor (void **x)
{
    zstr_free ((char**) x);
}

static void
s_handle_set (upt_server_t *server, mlm_client_t *client, zmsg_t *msg)
{
    char *dc_name = zmsg_popstr (msg);
    if (!dc_name) {
        zsys_error ("no DC name in message, ignoring");
        return;
    }

    zlistx_t *ups = zlistx_new ();
    zlistx_set_destructor (ups, s_str_destructor);
    char *ups_name;
    while ((ups_name = zmsg_popstr (msg)) != NULL) {
        zlistx_add_end (ups, ups_name);
    }

    upt_add (server->upt, dc_name, ups);

    // recalculate uptime - some modification might have had an impact
    // on a state of DC
    uint64_t total, offline;
    upt_uptime (server->upt, dc_name, &total, &offline);

    zlistx_destroy (&ups);
    zstr_free (&dc_name);

}

static void
s_handle_uptime (upt_server_t *server, mlm_client_t *client, zmsg_t *msg)
{
    int r;
    char *dc_name = zmsg_popstr (msg);
    if (!dc_name) {
        zsys_error ("no DC name in message, ignoring");

        mlm_client_sendtox (
            client,
            mlm_client_sender (client),
            "UPTIME",
            "UPTIME",
            "ERROR",
            "Invalid request: missing DC name", NULL);
    }

    uint64_t total, offline;
    r = upt_uptime (server->upt, dc_name, &total, &offline);

    if (r == -1) {
        zsys_error ("Can't compute uptime, most likely unknown DC");
        mlm_client_sendtox (
            client,
            mlm_client_sender (client),
            "UPTIME",
            "UPTIME",
            "ERROR",
            "Invalid request: DC name is not known", NULL);
    }

    char *s_total, *s_offline;
    r = asprintf (&s_total, "%"PRIu64, total);
    assert (r > 0);
    r = asprintf (&s_offline, "%"PRIu64, offline);
    assert (r > 0);

    mlm_client_sendtox (
        client,
        mlm_client_sender (client),
        "UPTIME",
        "UPTIME",
        s_total,
        s_offline,
        NULL);

    zstr_free (&dc_name);
    zstr_free (&s_total);
    zstr_free (&s_offline);
}

static bool
s_ups_is_onbattery (bios_proto_t *msg)
{
    const char *state = bios_proto_value (msg);
    if (isdigit (state[0])) {
        // see core.git/src/shared/upsstatus.h STATUS_OB == 1 << 4 == 16
        int istate = atoi (state);
        return (istate & 0x10) != 0;
    }
    else
        // this is forward compatible - new protocol allows strings to be passed
        return strstr (state, "OB") != NULL;
}

static void
s_handle_metric (upt_server_t *server, mlm_client_t *client, bios_proto_t *msg)
{
    const char *ups_name = bios_proto_element_src (msg);
    const char *dc_name = upt_dc_name (server->upt, ups_name);

    if (!dc_name)
        return;

    if (s_ups_is_onbattery (msg))
        upt_set_offline (server->upt, ups_name);
    else
        upt_set_online (server->upt, ups_name);

    uint64_t total, offline;
    // recalculate total/offline when we get the metric
    upt_uptime (server->upt, dc_name, &total, &offline);
}

//  Server as an actor
void upt_server (zsock_t *pipe, void *args)
{
    int ret;
    char *name = (char*) args;
    mlm_client_t *client = mlm_client_new ();
    upt_server_t *server = upt_server_new ();
    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (client), NULL);
    zsock_signal (pipe, 0);

    while (!zsys_interrupted)
    {
        void *which = zpoller_wait (poller, -1);
        if (which == pipe) {
            zmsg_t *msg = zmsg_recv (pipe);
            char *cmd = zmsg_popstr (msg);
            if (server->verbose)
                zsys_debug ("%s: API command=%s", name, cmd);

            if (streq (cmd, "$TERM")) {
                zstr_free (&cmd);
                zmsg_destroy (&msg);
                goto exit;
            }
            else
            if (streq (cmd, "VERBOSE")) {
                upt_server_verbose (server);
                zmsg_destroy (&msg);
                zsock_signal (pipe, 0);
            }
            else
            if (streq (cmd, "CONNECT")) {
                char* endpoint = zmsg_popstr (msg);
                int rv = mlm_client_connect (client, endpoint, 1000, name);
                if (rv == -1)
                    zsys_error ("%s: can't connect to malamute endpoint '%s'", name, endpoint);
                zstr_free (&endpoint);
                zsock_signal (pipe, 0);
            }
            else
            if (streq (cmd, "CONSUMER")) {
                char* stream = zmsg_popstr (msg);
                char* pattern = zmsg_popstr (msg);
                int rv = mlm_client_set_consumer (client, stream, pattern);
                if (rv == -1)
                    zsys_error ("%s: can't set consumer on '%s/%s'", stream, pattern);
                zstr_free (&stream);
                zstr_free (&pattern);
                zsock_signal (pipe, 0);
            }
            else
            if (streq (cmd, "CONFIG")) {
                char* dir = zmsg_popstr (msg);
                if (!dir)
                    zsys_error ("%s: CONFIG: directory is empty", name);
                upt_server_set_dir (server, dir);
                int r = upt_server_load_state (server);
                if (!r)
                    zsys_error ("%s: CONFIG: failed to load %s/state", name, dir);
                zstr_free (&dir);
                zsock_signal (pipe, 0);
            }
            zstr_free (&cmd);
            zmsg_destroy (&msg);
            continue;
        }   // which == pipe

        if (server->verbose)
            zsys_debug ("%s: handling the protocol", name);
        zmsg_t *msg = mlm_client_recv (client);
        if (!msg)
            continue;

        if (streq (mlm_client_command (client), "MAILBOX DELIVER"))
        {
            char *command = zmsg_popstr (msg);
            if (server->verbose)
                zsys_debug ("%s:\tProto command=%s", name, command);
            if (!command) {
                zmsg_destroy (&msg);
                continue;
            }
            if (streq (command, "SET")) {
                s_handle_set (server, client, msg);
            }
            else
            if (streq (command, "UPTIME")) {
                s_handle_uptime (server, client, msg);
            }
            zstr_free (&command);
        }
        else
        if (streq (mlm_client_command (client), "STREAM DELIVER"))
        {
            bios_proto_t *bmsg = bios_proto_decode (&msg);
            if (!bmsg) {
                zsys_warning ("Not bios proto, skipping");
            }
            else
            if (bios_proto_id (bmsg) != BIOS_PROTO_METRIC) {
                zsys_warning ("Not bios proto metric, skipping");
            }
            else {
                s_handle_metric (server, client, bmsg);
            }
            bios_proto_destroy (&bmsg);
        }

        zmsg_destroy (&msg);
    }
exit:
    ret = upt_server_save_state (server);
    if (ret != 0)
        zsys_error ("failed to save state to %s", server->dir);
    zpoller_destroy (&poller);
    mlm_client_destroy (&client);
    upt_server_destroy (&server);
}

//  --------------------------------------------------------------------------
//  Self test of this class.

void
upt_server_test (bool verbose)
{
    printf (" * upt_server: ");
    if (verbose)
        printf ("\n");

    //  @selftest
    static const char* endpoint = "inproc://upt-server-test";
    zactor_t *broker = zactor_new (mlm_server, "Malamute");
    zstr_sendx (broker, "BIND", endpoint, NULL);
    if (verbose)
        zstr_send (broker, "VERBOSE");

    mlm_client_t *ui = mlm_client_new ();
    mlm_client_connect (ui, endpoint, 1000, "UI");

    mlm_client_t *ups = mlm_client_new ();
    mlm_client_connect (ups, endpoint, 1000, "UPS");
    mlm_client_set_producer (ups, "METRICS");

    zactor_t *server = zactor_new (upt_server, (void*) "uptime");
    if (verbose) {
        zstr_send (server, "VERBOSE");
        zsock_wait (server);
    }

    zstr_sendx (server, "CONNECT", endpoint, NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONSUMER", "METRICS", "status.ups.*", NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONFIG", "src/", NULL);
    zsock_wait (server);

    zclock_sleep (500);   //THIS IS A HACK TO SETTLE DOWN THINGS

    // add some data centers and ups'es
    zmsg_t *req = zmsg_new ();
    zmsg_addstrf (req, "%s", "SET");
    zmsg_addstrf (req, "%s", "DC007");
    zmsg_addstrf (req, "%s", "UPS007");
    zmsg_addstrf (req, "%s", "UPS001");
    mlm_client_sendto (ui, "uptime", "UPTIME", NULL, 5000, &req);

    int r;
    char *subject, *command, *total, *offline;
    // check the uptime
    zclock_sleep (1000);
    req = zmsg_new ();
    zmsg_addstrf (req, "%s", "UPTIME");
    zmsg_addstrf (req, "%s", "DC007");
    mlm_client_sendto (ui, "uptime", "UPTIME", NULL, 5000, &req);

    r = mlm_client_recvx (ui, &subject, &command, &total, &offline, NULL);
    assert (r != -1);
    assert (streq (subject, "UPTIME"));
    assert (streq (command, "UPTIME"));
    assert (streq (total, "1"));
    assert (streq (offline, "0"));
    zstr_free (&subject);
    zstr_free (&command);
    zstr_free (&total);
    zstr_free (&offline);

    // put ups to onbattery
    zmsg_t *metric = bios_proto_encode_metric (NULL,
            "status.ups", "UPS007", "16", "", -1);
    mlm_client_send (ups, "status.ups@UPS007", &metric);

    // check the uptime
    zclock_sleep (1000);
    req = zmsg_new ();
    zmsg_addstrf (req, "%s", "UPTIME");
    zmsg_addstrf (req, "%s", "DC007");
    mlm_client_sendto (ui, "uptime", "UPTIME", NULL, 5000, &req);

    r = mlm_client_recvx (ui, &subject, &command, &total, &offline, NULL);
    assert (r != -1);
    assert (streq (subject, "UPTIME"));
    assert (streq (command, "UPTIME"));
    assert (streq (total, "2"));
    assert (streq (offline, "1"));
    zstr_free (&subject);
    zstr_free (&command);
    zstr_free (&total);
    zstr_free (&offline);

    //remove UPS007 from DC007, so it become online again
    req = zmsg_new ();
    zmsg_addstrf (req, "%s", "SET");
    zmsg_addstrf (req, "%s", "DC007");
    zmsg_addstrf (req, "%s", "UPS001");
    mlm_client_sendto (ui, "uptime", "UPTIME", NULL, 5000, &req);

    // check the uptime
    zclock_sleep (1000);
    req = zmsg_new ();
    zmsg_addstrf (req, "%s", "UPTIME");
    zmsg_addstrf (req, "%s", "DC007");
    mlm_client_sendto (ui, "uptime", "UPTIME", NULL, 5000, &req);

    r = mlm_client_recvx (ui, &subject, &command, &total, &offline, NULL);
    assert (r != -1);
    assert (streq (subject, "UPTIME"));
    assert (streq (command, "UPTIME"));
    assert (streq (total, "3"));
    assert (streq (offline, "1"));
    zstr_free (&subject);
    zstr_free (&command);
    zstr_free (&total);
    zstr_free (&offline);

    mlm_client_destroy (&ups);
    mlm_client_destroy (&ui);

    zactor_destroy (&server);
    zactor_destroy (&broker);
    //  @end

    printf ("OK\n");
}
