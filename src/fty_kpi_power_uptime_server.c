/*  =========================================================================
    fty_kpi_power_uptime_server - Actor computing uptime

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
    fty_kpi_power_uptime_server - Actor computing uptime
@discuss
@end
*/

#include "fty_kpi_power_uptime_classes.h"

//  Structure of our class

struct _fty_kpi_power_uptime_server_t {
    bool verbose;
    int request_counter;
    upt_t *upt;
    char *dir;
    char *name;
};


//  --------------------------------------------------------------------------
//  Create a new fty_kpi_power_uptime_server

FTY_KPI_POWER_UPTIME_EXPORT fty_kpi_power_uptime_server_t *
fty_kpi_power_uptime_server_new (void)
{
    fty_kpi_power_uptime_server_t *self = (fty_kpi_power_uptime_server_t *) zmalloc (sizeof (fty_kpi_power_uptime_server_t));
    assert (self);

    self->verbose = false;
    self->request_counter = 0;
    self->upt = upt_new ();
    self->name = strdup ("uptime");

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the fty_kpi_power_uptime_server

FTY_KPI_POWER_UPTIME_EXPORT void
fty_kpi_power_uptime_server_destroy (fty_kpi_power_uptime_server_t **self_p)
{
    if (!self_p || !*self_p)
        return;

    fty_kpi_power_uptime_server_t *self = *self_p;
    upt_destroy (&self->upt);
    zstr_free (&self->dir);
    zstr_free (&self->name);
    free (self);
    *self_p = NULL;
}

//  Set server verbose
FTY_KPI_POWER_UPTIME_EXPORT  void
    fty_kpi_power_uptime_server_verbose (fty_kpi_power_uptime_server_t *self)
{
    assert (self);
    self->verbose = true;
}

// SET the DIR to state file
FTY_KPI_POWER_UPTIME_EXPORT  void
    fty_kpi_power_uptime_server_set_dir (fty_kpi_power_uptime_server_t *self, const char* dir)
{
    assert (self);
    assert (dir);

    zstr_free (&self->dir);
    self->dir = strdup (dir);
}

int
fty_kpi_power_uptime_server_load_state (fty_kpi_power_uptime_server_t *self)
{
    assert (self);
    assert (self->dir);

    char *state_file = zsys_sprintf ("%s/state", self->dir);

    upt_t *upt = upt_load (state_file);
    if (!upt)
        zsys_error ("error loading state\n");

    zstr_free (&state_file);
    upt_destroy (&self->upt);
    self->upt = upt;

    return 0;
}

int
fty_kpi_power_uptime_server_save_state (fty_kpi_power_uptime_server_t *self)
{
    assert (self);

    if (!self->dir) {
        zsys_error ("Saving state directory not configured yet. Probably got some messages before CONFIG.");
        return -1;
    }

    char *state_file = zsys_sprintf ("%s/state", self->dir);
    int rv = upt_save (self->upt, state_file);
    if (rv != 0)
    {
        zsys_error ("fty_kpi_power_uptime_server_save_state: error while saving state file");
        zstr_free (&state_file);
        return -1;
    }
    zstr_free (&state_file);
    return 0;
}

static void
s_set_dc_upses (fty_kpi_power_uptime_server_t *self, fty_proto_t *fmsg)
{
    assert (fmsg);
    assert (self);

    const char *dc_name = fty_proto_name (fmsg);
    if (!dc_name)
    {
        zsys_error ("s_set_dc_upses: missing DC name in fty-proto message");
        return;
    }

    zhash_t *aux = fty_proto_aux (fmsg);
    if (!aux)
    {
        zsys_error ("s_set_dc_upses: missing aux in fty-proto message");
        return;
    }

    if (self->verbose)
        zsys_debug ("%s:\ts_set_dc_upses \t dc_name: %s", self->name, dc_name);

    zlistx_t *ups = zlistx_new ();

    for (int i = 0; i < zhash_size (aux); ++i)
    {
        char *key = zsys_sprintf ("ups%d", i);
        void *item = zhash_lookup (aux, key);

        if (item)
        {
            zlistx_add_end (ups, item);

            if (self->verbose)
                zsys_debug ("%s:\ts_set_dc_upses : %s", self->name, (char *) item);
        }
        else
        {
            if (self->verbose)
                zsys_info ("s_set_dc_upses: not relevant item");
        }
            zstr_free (&key);
    }

    if (zlistx_size (ups) != 0)
        upt_add (self->upt, dc_name, ups);

    // recalculate uptime - some modification might have had an impact on a state of DC
    uint64_t total, offline;
    upt_uptime (self->upt, dc_name, &total, &offline);
    zlistx_destroy (&ups);
}

static void
s_handle_uptime (fty_kpi_power_uptime_server_t *server, mlm_client_t *client, zmsg_t *msg)
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
    if (server->verbose)
        zsys_debug ("%s:\tdc_name: '%s'", server->name, dc_name);

    uint64_t total, offline;
    r = upt_uptime (server->upt, dc_name, &total, &offline);
    if (server->verbose)
        zsys_debug ("%s:\tr: %d, total: %"PRIu64", offline: %"PRIu64"\n",
                server->name,
                r,
                total,
                offline
                );

    if (r == -1) {
        zsys_error ("Can't compute uptime, most likely unknown DC: %s", dc_name);
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
s_ups_is_onbattery (fty_proto_t *msg)
{
    const char *state = fty_proto_value (msg);
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
s_handle_metric (fty_kpi_power_uptime_server_t *server, mlm_client_t *client, fty_proto_t *msg)
{
    const char *ups_name = fty_proto_name (msg);
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
void fty_kpi_power_uptime_server (zsock_t *pipe, void *args)
{
    int ret;
    char *name = (char*) args;
    mlm_client_t *client = mlm_client_new ();
    fty_kpi_power_uptime_server_t *server = fty_kpi_power_uptime_server_new ();
    //FIXME: change constructor or add set_name method ...
    zstr_free (&server->name);
    server->name = strdup (name);

    zpoller_t *poller = zpoller_new (pipe, mlm_client_msgpipe (client), NULL);
    zsock_signal (pipe, 0);

    while (!zsys_interrupted)
    {
        void *which = zpoller_wait (poller, -1);
        if (which == pipe) {
            zmsg_t *msg = zmsg_recv (pipe);
            char *cmd = zmsg_popstr (msg);

            if (!cmd) {
                zsys_warning ("missing command in pipe");
                zstr_free (&cmd);
                zmsg_destroy (&msg);
                continue;
            }

            if (server->verbose)
                zsys_debug ("%s: API command=%s", name, cmd);

            if (streq (cmd, "$TERM")) {
                zstr_free (&cmd);
                zmsg_destroy (&msg);
                goto exit;
            }
            else
            if (streq (cmd, "VERBOSE")) {
                fty_kpi_power_uptime_server_verbose (server);
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
                    zsys_error ("%s: CONFIG: directory is null", name);
                else {
                    fty_kpi_power_uptime_server_set_dir (server, dir);
                    int r = fty_kpi_power_uptime_server_load_state (server);
                    if (server->verbose)
                        upt_print (server->upt);
                    if (r == -1)
                        zsys_error ("%s: CONFIG: failed to load %s/state", name, dir);
                }
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

        if (server->verbose) {
            zsys_debug ("%s:\tcommand=%s", name, mlm_client_command (client));
            zsys_debug ("%s:\tsender=%s", name, mlm_client_sender (client));
            zsys_debug ("%s:\tsubject=%s", name, mlm_client_subject (client));
        }

        if ((server->request_counter++) % 100 == 0)
        {
            if (server->verbose)
                zsys_debug ("%s: saving the state", name);
            fty_kpi_power_uptime_server_save_state (server);
        }

        if (streq (mlm_client_command (client), "MAILBOX DELIVER"))
        {
            char *command = zmsg_popstr (msg);
            if (server->verbose)
                zsys_debug ("%s:\tproto-command=%s", name, command);
            if (!command) {
                zmsg_destroy (&msg);
                mlm_client_sendtox (
                    client,
                    mlm_client_sender (client),
                    "UPTIME",
                    "ERROR",
                    "Unknown command",
                    NULL
                );
            }
            else
            if (streq (command, "UPTIME")) {
                s_handle_uptime (server, client, msg);
            }
            else {
                mlm_client_sendtox (
                    client,
                    mlm_client_sender (client),
                    "UPTIME",
                    "ERROR",
                    "Unknown command",
                    NULL
                );
            }
            zstr_free (&command);
        }
        else
        if (streq (mlm_client_command (client), "STREAM DELIVER"))
        {
            fty_proto_t *bmsg = fty_proto_decode (&msg);
            if (!bmsg) {
                zsys_warning ("Not fty proto, skipping");
            }
            else
            if (fty_proto_id (bmsg) == FTY_PROTO_METRIC)
            {
                s_handle_metric (server, client, bmsg);
            }
            else
            if (fty_proto_id (bmsg) == FTY_PROTO_ASSET)
            {
                if (streq (fty_proto_aux_string (bmsg, "type", "null"), "datacenter"))
                {
                    s_set_dc_upses (server, bmsg);
                }
                else
                    zsys_debug ("%s: invalid asset type: %s", server->name,
                                fty_proto_aux_string (bmsg, "type", "null"));
            }
            else
            {
                zsys_warning ("%s: recieved invalid message", server->name);
            }
            fty_proto_destroy(&bmsg);
        }

        zmsg_destroy (&msg);
    }
exit:
    ret = fty_kpi_power_uptime_server_save_state (server);
    if (ret != 0)
        zsys_error ("failed to save state to %s", server->dir);
    zpoller_destroy (&poller);
    mlm_client_destroy (&client);
    fty_kpi_power_uptime_server_destroy (&server);
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
fty_kpi_power_uptime_server_test (bool verbose)
{
    printf (" * fty_kpi_power_uptime_server: ");
    if (verbose)
        printf ("\n");

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

    //  @selftest
    static const char* endpoint = "inproc://upt-server-test";
    zactor_t *broker = zactor_new (mlm_server, "Malamute");
    zstr_sendx (broker, "BIND", endpoint, NULL);
    if (verbose)
        zstr_send (broker, "VERBOSE");

    mlm_client_t *ui_metr = mlm_client_new ();
    mlm_client_connect (ui_metr, endpoint, 1000, "UI-M");

    mlm_client_t *ups = mlm_client_new ();
    mlm_client_connect (ups, endpoint, 1000, "UPS");
    mlm_client_set_producer (ups, "METRICS");

    mlm_client_t *ups_dc = mlm_client_new ();
    mlm_client_connect (ups_dc, endpoint, 1000, "UPS_DC");
    mlm_client_set_producer (ups_dc, "ASSETS");

    fty_kpi_power_uptime_server_t *kpi = fty_kpi_power_uptime_server_new ();

    zactor_t *server = zactor_new (fty_kpi_power_uptime_server, (void*) "uptime");
    if (verbose) {
        zstr_send (server, "VERBOSE");
        zsock_wait (server);
    }

    zstr_sendx (server, "CONFIG", SELFTEST_DIR_RW, NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONNECT", endpoint, NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONSUMER", "METRICS", "status.ups.*", NULL);
    zsock_wait (server);
    zstr_sendx (server, "CONSUMER", "ASSETS", "datacenter.unknown@.*", NULL);
    zsock_wait (server);
    zstr_sendx (server, "VERBOSE", NULL);
    zsock_wait (server);
    zclock_sleep (500);   //THIS IS A HACK TO SETTLE DOWN THINGS

    // ---------- test of the new fn ------------------------
    zhash_t *aux = zhash_new();
    zhash_autofree(aux);
    zhash_insert (aux, "ups1", (void* ) "roz.ups33");
    zhash_insert (aux, "ups2", (void* ) "roz.ups36");
    zhash_insert (aux, "ups3", (void* ) "roz.ups38");
    zhash_insert (aux, "type", (void* ) "datacenter");
    zhash_insert (aux, "test", (void* ) "test");
    zmsg_t *msg = fty_proto_encode_asset (
        aux,
        "my-dc",
        "inventory",
        NULL
    );
    assert(msg);
    fty_proto_t *fmsg = fty_proto_decode (&msg);
    assert(fmsg);

    s_set_dc_upses (kpi, fmsg);
    zclock_sleep (500);

    zhash_destroy (&aux);
    fty_proto_destroy (&fmsg);
    fty_kpi_power_uptime_server_destroy (&kpi);

    // -------------- test of the whole component ----------
    const char *subject = "datacenter.unknown@my-dc";

    zhash_t *aux2 = zhash_new();
    zhash_autofree (aux2);
    zhash_insert (aux2, "ups1", (void* ) "roz.ups33");
    zhash_insert (aux2, "ups2", (void* ) "roz.ups36");
    zhash_insert (aux2, "ups3", (void* ) "roz.ups38");
    zhash_insert (aux2, "test", (void* ) "test");
    zhash_insert (aux2, "type", (void* ) "datacenter");

    zmsg_t *msg2 = fty_proto_encode_asset (
        aux2,
        "my-dc",
        "inventory",
        NULL
    );

    int rv = mlm_client_send (ups_dc, subject, &msg2);
    assert (rv == 0);
    zclock_sleep (500);
    zhash_destroy (&aux2);

    // set ups to on battery
    zmsg_t *metric = fty_proto_encode_metric (NULL,
                                              time (NULL),
                                              100,
                                              "status.ups",
                                              "roz.ups33",
                                              "16",
                                              ""
                                              );

    mlm_client_send (ups, "status.ups@roz.ups33", &metric);

    char *subject2, *command, *total, *offline;
    zclock_sleep (1000);
    zmsg_t *req = zmsg_new ();
    zmsg_addstrf (req, "%s", "UPTIME");
    zmsg_addstrf (req, "%s", "my-dc");
    mlm_client_sendto (ui_metr, "uptime", "UPTIME", NULL, 5000, &req);

    zclock_sleep (3000);
    int r = mlm_client_recvx (ui_metr, &subject2, &command, &total, &offline, NULL);
    assert (r != -1);
    assert (streq (subject2, "UPTIME"));
    assert (streq (command, "UPTIME"));
    assert (atoi (total) > 0);
    assert (atoi (offline) > 0);

    zmsg_destroy (&metric);
    zstr_free (&subject2);
    zstr_free (&command);
    zstr_free (&total);
    zstr_free (&offline);

    mlm_client_destroy (&ups_dc);
    mlm_client_destroy (&ups);
    mlm_client_destroy (&ui_metr);
    zactor_destroy (&server);
    zactor_destroy (&broker);

    // test for private function only!! UGLY REDONE DO NOT READ!!
    fty_kpi_power_uptime_server_t *s = fty_kpi_power_uptime_server_new ();

    upt_t *upt = upt_new ();
    zlistx_t *upsl = zlistx_new ();

    zlistx_add_end (upsl, "UPS007");
    zlistx_add_end (upsl, "UPS006");
    r = upt_add (upt, "DC007", upsl);
    assert (r == 0);

    fty_kpi_power_uptime_server_set_dir (s, SELFTEST_DIR_RW);
    zclock_sleep (1000);
    r = fty_kpi_power_uptime_server_save_state(s);
    assert (r == 0);
    r = fty_kpi_power_uptime_server_load_state (s);
    assert (r == 0);

    zlistx_destroy (&upsl);
    upt_destroy (&upt);
    fty_kpi_power_uptime_server_destroy (&s);
    //  @end

    printf ("OK\n");
}
