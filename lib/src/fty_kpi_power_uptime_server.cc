/*  =========================================================================
    fty_kpi_power_uptime_server - Actor computing uptime

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

/// fty_kpi_power_uptime_server - Actor computing uptime

#include "fty_kpi_power_uptime_server.h"
#include <regex>
#include <fty_log.h>
#include <fty_proto.h>
#include <malamute.h>
#include <fty_shm.h>

//  Structure of our class


//  --------------------------------------------------------------------------
//  Create a new fty_kpi_power_uptime_server

fty_kpi_power_uptime_server_t* fty_kpi_power_uptime_server_new(void)
{
    fty_kpi_power_uptime_server_t* self =
        reinterpret_cast<fty_kpi_power_uptime_server_t*>(zmalloc(sizeof(fty_kpi_power_uptime_server_t)));
    assert(self);

    self->request_counter = 0;
    self->upt             = upt_new();
    self->name            = strdup("uptime");

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the fty_kpi_power_uptime_server

void fty_kpi_power_uptime_server_destroy(fty_kpi_power_uptime_server_t** self_p)
{
    if (!self_p || !*self_p)
        return;

    fty_kpi_power_uptime_server_t* self = *self_p;
    upt_destroy(&self->upt);
    zstr_free(&self->dir);
    zstr_free(&self->name);
    free(self);
    *self_p = nullptr;
}

// SET the DIR to state file
void fty_kpi_power_uptime_server_set_dir(fty_kpi_power_uptime_server_t* self, const char* dir)
{
    assert(self);
    assert(dir);

    zstr_free(&self->dir);
    self->dir = strdup(dir);
}

int fty_kpi_power_uptime_server_load_state(fty_kpi_power_uptime_server_t* self)
{
    assert(self);
    assert(self->dir);

    char* state_file = zsys_sprintf("%s/state", self->dir);

    upt_t* upt = upt_load(state_file);
    if (!upt)
        log_error("error loading state\n");

    zstr_free(&state_file);
    upt_destroy(&self->upt);
    self->upt = upt;

    return 0;
}

int fty_kpi_power_uptime_server_save_state(fty_kpi_power_uptime_server_t* self)
{
    assert(self);

    if (!self->dir) {
        log_error("Saving state directory not configured yet. Probably got some messages before CONFIG.");
        return -1;
    }

    char* state_file = zsys_sprintf("%s/state", self->dir);
    int   rv         = upt_save(self->upt, state_file);
    if (rv != 0) {
        log_error("fty_kpi_power_uptime_server_save_state: error while saving state file");
        zstr_free(&state_file);
        return -1;
    }
    zstr_free(&state_file);
    return 0;
}

void s_set_dc_upses(fty_kpi_power_uptime_server_t* self, fty_proto_t* fmsg)
{
    assert(fmsg);
    assert(self);

    const char* dc_name = fty_proto_name(fmsg);
    if (!dc_name) {
        log_error("s_set_dc_upses: missing DC name in fty-proto message");
        return;
    }

    zhash_t* aux = fty_proto_get_aux(fmsg);
    zhash_autofree(aux);
    if (!aux) {
        log_error("s_set_dc_upses: missing aux in fty-proto message");
        return;
    }

    log_debug("%s:\ts_set_dc_upses \t dc_name: %s", self->name, dc_name);

    zlistx_t* ups = zlistx_new();

    for (uint i = 0; i < zhash_size(aux); ++i) {
        char* key  = zsys_sprintf("ups%d", i);
        void* item = zhash_lookup(aux, key);

        if (item) {
            zlistx_add_end(ups, item);
            log_debug("%s:\ts_set_dc_upses : %s", self->name, reinterpret_cast<char*>(item));
        } else {
            log_info("s_set_dc_upses: not relevant item");
        }
        zstr_free(&key);
    }

    if (zlistx_size(ups) != 0)
        upt_add(self->upt, dc_name, ups);

    // recalculate uptime - some modification might have had an impact on a state of DC
    uint64_t total, offline;
    upt_uptime(self->upt, dc_name, &total, &offline);
    zlistx_destroy(&ups);
    zhash_destroy(&aux);
}

static void s_handle_uptime(fty_kpi_power_uptime_server_t* server, mlm_client_t* client, zmsg_t* msg)
{
    int   r;
    char* dc_name = zmsg_popstr(msg);
    if (!dc_name) {
        log_error("no DC name in message, ignoring");

        mlm_client_sendtox(client, mlm_client_sender(client), "UPTIME", "UPTIME", "ERROR",
            "Invalid request: missing DC name", nullptr);
    }
    log_debug("%s:\tdc_name: '%s'", server->name, dc_name);

    uint64_t total, offline;
    r = upt_uptime(server->upt, dc_name, &total, &offline);

    log_debug("%s:\tr: %d, total: %" PRIu64 ", offline: %" PRIu64 "\n", server->name, r, total, offline);

    if (r == -1) {
        log_error("Can't compute uptime, most likely unknown DC: %s", dc_name);
        mlm_client_sendtox(client, mlm_client_sender(client), "UPTIME", "UPTIME", "ERROR",
            "Invalid request: DC name is not known", nullptr);
    }

    char *s_total, *s_offline;
    r = asprintf(&s_total, "%" PRIu64, total);
    assert(r > 0);
    r = asprintf(&s_offline, "%" PRIu64, offline);
    assert(r > 0);

    mlm_client_sendtox(client, mlm_client_sender(client), "UPTIME", "UPTIME", s_total, s_offline, nullptr);

    zstr_free(&dc_name);
    zstr_free(&s_total);
    zstr_free(&s_offline);
}

static bool s_ups_is_onbattery(fty_proto_t* msg)
{
    const char* state = fty_proto_value(msg);
    if (isdigit(state[0])) {
        // see core.git/src/shared/upsstatus.h STATUS_OB == 1 << 4 == 16
        int istate = atoi(state);
        return (istate & 0x10) != 0;
    } else
        // this is forward compatible - new protocol allows strings to be passed
        return strstr(state, "OB") != nullptr;
}

static void s_handle_metric(fty_kpi_power_uptime_server_t* server, mlm_client_t* /*client*/, fty_proto_t* msg)
{
    const char* ups_name = fty_proto_name(msg);
    const char* dc_name  = upt_dc_name(server->upt, ups_name);

    if (!dc_name)
        return;

    if (s_ups_is_onbattery(msg))
        upt_set_offline(server->upt, ups_name);
    else
        upt_set_online(server->upt, ups_name);

    uint64_t total, offline;
    // recalculate total/offline when we get the metric
    upt_uptime(server->upt, dc_name, &total, &offline);
}


void fty_kpi_power_metric_pull(zsock_t* pipe, void* args)
{
    zpoller_t* poller = zpoller_new(pipe, nullptr);
    zsock_signal(pipe, 0);

    fty_kpi_power_uptime_server_t* server  = reinterpret_cast<fty_kpi_power_uptime_server_t*>(args);
    uint64_t                       timeout = uint64_t(fty_get_polling_interval() * 1000);
    while (!zsys_interrupted) {
        void* which = zpoller_wait(poller, int(timeout));
        if (which == nullptr) {
            if (zpoller_terminated(poller) || zsys_interrupted) {
                break;
            }
            if (zpoller_expired(poller)) {
                fty::shm::shmMetrics result;
                fty::shm::read_metrics(".*", "^status\\.ups|^status", result);
                log_debug("metric reads : %d", result.size());

                for (auto& element : result) {
                    s_handle_metric(server, nullptr, element);
                }
            }
        } else if (which == pipe) {
            zmsg_t* message = zmsg_recv(pipe);
            if (message) {
                char* cmd = zmsg_popstr(message);
                if (cmd) {
                    if (streq(cmd, "$TERM")) {
                        zstr_free(&cmd);
                        zmsg_destroy(&message);
                        break;
                    }
                    zstr_free(&cmd);
                }
                zmsg_destroy(&message);
            }
        }
        timeout = uint64_t(fty_get_polling_interval() * 1000);
    }
    zpoller_destroy(&poller);
}

//  Server as an actor
void fty_kpi_power_uptime_server(zsock_t* pipe, void* args)
{
    int                            ret;
    char*                          name   = reinterpret_cast<char*>(args);
    mlm_client_t*                  client = mlm_client_new();
    fty_kpi_power_uptime_server_t* server = fty_kpi_power_uptime_server_new();
    // FIXME: change constructor or add set_name method ...
    zstr_free(&server->name);
    server->name = strdup(name);

    zpoller_t* poller = zpoller_new(pipe, mlm_client_msgpipe(client), nullptr);
    zsock_signal(pipe, 0);
    zactor_t* kpi_power_metric_pull = zactor_new(fty_kpi_power_metric_pull, server);
    while (!zsys_interrupted) {
        void* which = zpoller_wait(poller, -1);
        if (which == pipe) {
            zmsg_t* msg = zmsg_recv(pipe);
            char*   cmd = zmsg_popstr(msg);

            if (!cmd) {
                log_warning("missing command in pipe");
                zstr_free(&cmd);
                zmsg_destroy(&msg);
                continue;
            }

            log_debug("%s: API command=%s", name, cmd);
            if (streq(cmd, "$TERM")) {
                zstr_free(&cmd);
                zmsg_destroy(&msg);
                goto exit;
            } else if (streq(cmd, "CONNECT")) {
                char* endpoint = zmsg_popstr(msg);
                int   rv       = mlm_client_connect(client, endpoint, 1000, name);
                if (rv == -1)
                    log_error("%s: can't connect to malamute endpoint '%s'", name, endpoint);
                zstr_free(&endpoint);
                zsock_signal(pipe, 0);
            } else if (streq(cmd, "CONSUMER")) {
                char* stream  = zmsg_popstr(msg);
                char* pattern = zmsg_popstr(msg);
                int   rv      = mlm_client_set_consumer(client, stream, pattern);
                if (rv == -1)
                    log_error("%s: can't set consumer on '%s/%s'", stream, pattern);
                zstr_free(&stream);
                zstr_free(&pattern);
                zsock_signal(pipe, 0);
            } else if (streq(cmd, "CONFIG")) {
                char* dir = zmsg_popstr(msg);
                if (!dir)
                    log_error("%s: CONFIG: directory is null", name);
                else {
                    fty_kpi_power_uptime_server_set_dir(server, dir);
                    int r = fty_kpi_power_uptime_server_load_state(server);
                    upt_print(server->upt);
                    if (r == -1)
                        log_error("%s: CONFIG: failed to load %s/state", name, dir);
                }
                zstr_free(&dir);
                zsock_signal(pipe, 0);
            }
            zstr_free(&cmd);
            zmsg_destroy(&msg);
            continue;
        } // which == pipe

        log_debug("%s: handling the protocol", name);
        zmsg_t* msg = mlm_client_recv(client);
        if (!msg)
            continue;


        log_debug("%s:\tcommand=%s", name, mlm_client_command(client));
        log_debug("%s:\tsender=%s", name, mlm_client_sender(client));
        log_debug("%s:\tsubject=%s", name, mlm_client_subject(client));


        if ((server->request_counter++) % 100 == 0) {

            log_debug("%s: saving the state", name);
            fty_kpi_power_uptime_server_save_state(server);
        }

        if (streq(mlm_client_command(client), "MAILBOX DELIVER")) {
            char* command = zmsg_popstr(msg);
            log_debug("%s:\tproto-command=%s", name, command);
            if (!command) {
                zmsg_destroy(&msg);
                mlm_client_sendtox(client, mlm_client_sender(client), "UPTIME", "ERROR", "Unknown command", nullptr);
            } else if (streq(command, "UPTIME")) {
                s_handle_uptime(server, client, msg);
            } else {
                mlm_client_sendtox(client, mlm_client_sender(client), "UPTIME", "ERROR", "Unknown command", nullptr);
            }
            zstr_free(&command);
        } else if (streq(mlm_client_command(client), "STREAM DELIVER")) {
            fty_proto_t* bmsg = fty_proto_decode(&msg);
            if (!bmsg) {
                log_warning("Not fty proto, skipping");
            } else if (fty_proto_id(bmsg) == FTY_PROTO_METRIC) {
                s_handle_metric(server, client, bmsg);
            } else if (fty_proto_id(bmsg) == FTY_PROTO_ASSET) {
                if (streq(fty_proto_aux_string(bmsg, "type", "null"), "datacenter")) {
                    s_set_dc_upses(server, bmsg);
                } else
                    log_debug("%s: invalid asset type: %s", server->name, fty_proto_aux_string(bmsg, "type", "null"));
            } else {
                log_warning("%s: recieved invalid message", server->name);
            }
            fty_proto_destroy(&bmsg);
        }

        zmsg_destroy(&msg);
    }
exit:
    ret = fty_kpi_power_uptime_server_save_state(server);
    if (ret != 0)
        log_error("failed to save state to %s", server->dir);
    zactor_destroy(&kpi_power_metric_pull);
    zpoller_destroy(&poller);
    mlm_client_destroy(&client);
    fty_kpi_power_uptime_server_destroy(&server);
}
