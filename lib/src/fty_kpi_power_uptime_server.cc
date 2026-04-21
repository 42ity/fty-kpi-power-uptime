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

#include <fty_log.h>
#include <fty_proto.h>
#include <malamute.h>
#include <fty_shm.h>
#include <regex>

//  Structure of our class


//  --------------------------------------------------------------------------
//  Create a new fty_kpi_power_uptime_server

fty_kpi_power_uptime_server_t* fty_kpi_power_uptime_server_new(const char* name)
{
    fty_kpi_power_uptime_server_t* self = static_cast<fty_kpi_power_uptime_server_t*>(zmalloc(sizeof(*self)));

    if (self) {
        self->name = strdup(name ? name : "uptime"); // mlm mailbox address
        self->upt  = upt_new();
        self->dir  = NULL;
        self->save_counter = 0;

        if (!(self->name && self->upt)) {
            fty_kpi_power_uptime_server_destroy(&self);
            return NULL;
        }
    }
    else {
        log_error("fty_kpi_power_uptime_server_new failed");
    }

    return self;
}

//  --------------------------------------------------------------------------
//  Destroy the fty_kpi_power_uptime_server

void fty_kpi_power_uptime_server_destroy(fty_kpi_power_uptime_server_t** self_p)
{
    if (self_p && (*self_p)) {
        fty_kpi_power_uptime_server_t* self = *self_p;
        zstr_free(&self->name);
        upt_destroy(&self->upt);
        zstr_free(&self->dir);
        self->save_counter = 0;
        free(self);
        *self_p = nullptr;
    }
}

// SET the DIR to state file
void fty_kpi_power_uptime_server_set_dir(fty_kpi_power_uptime_server_t* self, const char* dir)
{
    if (!self) return;

    zstr_free(&self->dir);
    self->dir = dir ? strdup(dir) : NULL;
}

int fty_kpi_power_uptime_server_load_state(fty_kpi_power_uptime_server_t* self)
{
    if (!self) return -1;
    if (!self->dir) return 0; //nop

    char* state_file = zsys_sprintf("%s/state", self->dir);

    upt_t* upt = upt_load(state_file);
    if (!upt) {
        log_error("error loading state (%s)", state_file);
        zstr_free(&state_file);
        return -1;
    }
    zstr_free(&state_file);

    upt_destroy(&self->upt);
    self->upt = upt;

    return 0;
}

int fty_kpi_power_uptime_server_save_state(fty_kpi_power_uptime_server_t* self)
{
    if (!self) return -1;

    if (!self->dir) {
        log_error("Saving state directory not configured yet. Probably got some messages before CONFIG.");
        return -1;
    }

    char* state_file = zsys_sprintf("%s/state", self->dir);
    log_debug("state_file=%s", state_file);
    int r = upt_save(self->upt, state_file);
    if (r != 0) {
        log_error("fty_kpi_power_uptime_server_save_state failed (r=%d, %s)", r, state_file);
    }
    zstr_free(&state_file);

    return (r == 0) ? 0 : -1;
}

void s_set_dc_upses(fty_kpi_power_uptime_server_t* self, fty_proto_t* fmsg)
{
    if (!self) return;

    if (!fmsg) {
        log_error("s_set_dc_upses: NULL fty-proto message");
        return;
    }

    const char* dc_name = fty_proto_name(fmsg);
    if (!dc_name) {
        log_error("s_set_dc_upses: missing DC name in fty-proto message");
        return;
    }

    zhash_t* aux = fty_proto_get_aux(fmsg); // take ownership
    if (!aux) {
        log_error("s_set_dc_upses: missing aux in fty-proto message");
        return;
    }

    zlistx_t* ups = zlistx_new();
    if (!ups) {
        log_error("s_set_dc_upses: ups zlistx_new() failed");
        zhash_destroy(&aux);
        return;
    }

    log_debug("%s: s_set_dc_upses, dc_name: %s", self->name, dc_name);

    for (size_t i = 0; i < zhash_size(aux); i++) {
        char key[32];
        snprintf(key, sizeof(key), "ups%zu", i);

        void* item = zhash_lookup(aux, key);
        if (item) {
            zlistx_add_end(ups, item);
            log_debug("%s: s_set_dc_upses : %s", self->name, reinterpret_cast<char*>(item));
        }
        else {
            log_info("s_set_dc_upses: not relevant item (key=%s)", key);
        }
    }

    if (zlistx_size(ups) != 0) {
        upt_add(self->upt, dc_name, ups);
    }

    // recalculate uptime - some modification might have had an impact on a state of DC
    uint64_t total = 0, offline = 0;
    upt_uptime(self->upt, dc_name, &total, &offline);

    zlistx_destroy(&ups);
    zhash_destroy(&aux);
}

static void s_handle_uptime(fty_kpi_power_uptime_server_t* server, mlm_client_t* client, zmsg_t* msg)
{
    if (!(server && client)) return;

    zmsg_t* reply = zmsg_new();
    if (!reply) {
        log_error("zmsg_new failed");
        return;
    }

    char* dc_name = NULL;

    do { // for break facilities
        dc_name = zmsg_popstr(msg);
        if (!dc_name) {
            log_error("no DC name in message, ignoring");
            zmsg_addstr(reply, "ERROR");
            zmsg_addstr(reply, "Missing DC name"); //reason
            break;
        }

        log_debug("%s: dc_name: '%s'", server->name, dc_name);

        uint64_t total = 0, offline = 0;
        int r = upt_uptime(server->upt, dc_name, &total, &offline);

        log_debug("%s: r: %d, total: %" PRIu64 ", offline: %" PRIu64 "\n", server->name, r, total, offline);

        if (r != 0) {
            log_error("Can't compute uptime, most likely unknown DC: %s", dc_name);
            zmsg_addstr(reply, "ERROR");
            zmsg_addstr(reply, "Unknown DC name"); //reason
            break;
        }

        char s_total[32], s_offline[32];
        snprintf(s_total, sizeof(s_total), "%" PRIu64, total);
        snprintf(s_offline, sizeof(s_offline), "%" PRIu64, offline);

        zmsg_addstr(reply, "UPTIME");
        zmsg_addstr(reply, s_total);
        zmsg_addstr(reply, s_offline);
        break;
    } while(0);

    zstr_free(&dc_name);

    // Send reply
    const char* sender = mlm_client_sender(client);
    const char* subject = mlm_client_subject(client); //UPTIME
    int r = mlm_client_sendto(client, sender, subject, NULL, 5000, &reply);
    zmsg_destroy(&reply);
    if (r != 0) {
        log_error("mlm_client_sendto() reply failed (r: %d, sender; %s)", r, sender);
    }
}

static bool s_ups_is_onbattery(fty_proto_t* msg)
{
    const char* state = msg ? fty_proto_value(msg) : NULL;
    if (!state) return false;

    if (isdigit(state[0])) {
        // see core.git/src/shared/upsstatus.h STATUS_OB == 1 << 4 == 16
        int istate = atoi(state);
        return (istate & 0x10) != 0;
    }

    // this is forward compatible - new protocol allows strings to be passed
    return strstr(state, "OB") != nullptr;
}

static void s_handle_metric(fty_kpi_power_uptime_server_t* server, fty_proto_t* msg)
{
    if (!server) return;

    const char* ups_name = msg ? fty_proto_name(msg) : NULL;
    const char* dc_name  = ups_name ? upt_dc_name(server->upt, ups_name) : NULL;

    if (!dc_name) return;

    if (s_ups_is_onbattery(msg)) {
        upt_set_offline(server->upt, ups_name);
    }
    else {
        upt_set_online(server->upt, ups_name);
    }

    // recalculate total/offline when we get the metric
    uint64_t total = 0, offline = 0;
    upt_uptime(server->upt, dc_name, &total, &offline);
}

void fty_kpi_power_metric_pull(zsock_t* pipe, void* args)
{
    if (!args) {
        log_error("args is NULL");
        return;
    }

    fty_kpi_power_uptime_server_t* server = static_cast<fty_kpi_power_uptime_server_t*>(args);
    if (!server) {
        log_error("server is NULL");
        return;
    }

    zpoller_t* poller = zpoller_new(pipe, nullptr);
    if (!poller) {
        log_error("zpoller_new failed");
        return;
    }

    zsock_signal(pipe, 0);

    while (!zsys_interrupted) {
        int timeout = fty_get_polling_interval() * 1000;
        void* which = zpoller_wait(poller, timeout);

        if (which == NULL) {
            if (zpoller_terminated(poller) || zsys_interrupted) {
                break;
            }
            if (zpoller_expired(poller)) {
                fty::shm::shmMetrics result;
                fty::shm::read_metrics(".*", "^status\\.ups|^status", result);
                log_debug("metric reads : %d", result.size());

                for (const auto& element : result) {
                    s_handle_metric(server, element);
                }
            }
        }
        else if (which == pipe) {
            zmsg_t* msg = zmsg_recv(pipe);
            char* cmd = msg ? zmsg_popstr(msg) : NULL;
            bool term = (cmd && streq(cmd, "$TERM"));
            zstr_free(&cmd);
            zmsg_destroy(&msg);
            if (term) {
                break;
            }
        }
    }

    zpoller_destroy(&poller);
}

//  Server as an actor
void fty_kpi_power_uptime_server(zsock_t* pipe, void* args)
{
    if (!static_cast<char*>(args)) {
        log_error("name/args is NULL");
        return;
    }

    fty_kpi_power_uptime_server_t* server = fty_kpi_power_uptime_server_new(static_cast<char*>(args));
    if (!server) {
        log_error("fty_kpi_power_uptime_server_new failed");
        return;
    }

    mlm_client_t* client = mlm_client_new();
    if (!client) {
        log_error("%s: mlm_client_new failed", server->name);
        fty_kpi_power_uptime_server_destroy(&server);
        return;
    }

    zpoller_t* poller = zpoller_new(pipe, mlm_client_msgpipe(client), nullptr);
    if (!poller) {
        log_error("%s: zpoller_new failed", server->name);
        mlm_client_destroy(&client);
        fty_kpi_power_uptime_server_destroy(&server);
        return;
    }

    zsock_signal(pipe, 0);

    zactor_t* kpi_power_metric_pull = zactor_new(fty_kpi_power_metric_pull, server);
    if (!kpi_power_metric_pull) {
        log_error("%s: kpi_power_metric_pull failed", server->name);
        zpoller_destroy(&poller);
        mlm_client_destroy(&client);
        fty_kpi_power_uptime_server_destroy(&server);
        return;
    }

    while (!zsys_interrupted) {
        void* which = zpoller_wait(poller, 10000);

        if (which == NULL) {
            if (zpoller_terminated(poller) || zsys_interrupted) {
                break;
            }
        }
        else if (which == pipe) {
            zmsg_t* msg = zmsg_recv(pipe);
            char* cmd = msg ? zmsg_popstr(msg) : NULL;
            bool term = (cmd && streq(cmd, "$TERM"));

            log_debug("%s: command=%s", server->name, (cmd ? cmd : "NULL"));

            if (!cmd) {
                log_warning("missing command in pipe");
            }
            else if (streq(cmd, "CONNECT")) {
                char* endpoint = zmsg_popstr(msg);
                int r = endpoint ? mlm_client_connect(client, endpoint, 1000, server->name) : -100;
                if (r != 0) {
                    log_error("%s: can't connect to malamute endpoint '%s' (r: %d)", server->name, endpoint, r);
                }
                zstr_free(&endpoint);
                zsock_signal(pipe, 0);
            }
            else if (streq(cmd, "CONSUMER")) {
                char* stream  = zmsg_popstr(msg);
                char* pattern = zmsg_popstr(msg);
                int r = (stream && pattern) ? mlm_client_set_consumer(client, stream, pattern) : -100;
                if (r != 0) {
                    log_error("%s: can't set consumer on '%s/%s' (r: %d)", server->name, stream, pattern, r);
                }
                zstr_free(&stream);
                zstr_free(&pattern);
                zsock_signal(pipe, 0);
            }
            else if (streq(cmd, "CONFIG")) {
                char* dir = zmsg_popstr(msg);
                if (!dir) {
                    log_error("%s: CONFIG: directory is null", server->name);
                }
                else {
                    fty_kpi_power_uptime_server_set_dir(server, dir);
                    int r = fty_kpi_power_uptime_server_load_state(server);
                    upt_print(server->upt);
                    if (r != 0) {
                        log_error("%s: CONFIG: failed to load %s/state (r: %d)", server->name, dir, r);
                    }
                }
                zstr_free(&dir);
                zsock_signal(pipe, 0);
            }

            zstr_free(&cmd);
            zmsg_destroy(&msg);

            if (term) {
                break;
            }
        }
        else if (which == mlm_client_msgpipe(client)) {

            // auto save
            server->save_counter++;
            if (server->save_counter >= 100) {
                log_debug("%s: saving state", server->name);
                fty_kpi_power_uptime_server_save_state(server);
                server->save_counter = 0;
            }

            zmsg_t* msg = mlm_client_recv(client);
            const char* command = mlm_client_command(client);

            if (streq(command, "MAILBOX DELIVER")) {
                const char* sender = mlm_client_sender(client);
                const char* subject = mlm_client_subject(client);
                char* cmd = zmsg_popstr(msg);
                log_debug("%s: %s sender=%s, subject=%s, cmd=%s", server->name, command, sender, subject, cmd);

                if (cmd && streq(cmd, "UPTIME")) {
                    s_handle_uptime(server, client, msg);
                }
                else {
                    zmsg_t* reply = zmsg_new();
                    zmsg_addstr(reply, "ERROR");
                    zmsg_addstr(reply, "Unexpected command"); //reason
                    int r = mlm_client_sendto(client, sender, subject, NULL, 5000, &reply);
                    zmsg_destroy(&reply);
                    if (r != 0) {
                        log_error("mlm_client_sendto reply failed");
                    }
                }

                zstr_free(&cmd);
            }
            else if (streq(command, "STREAM DELIVER")) {
                log_debug("%s: %s", server->name, command);
                fty_proto_t* proto = fty_proto_decode(&msg);

                if (proto && (fty_proto_id(proto) == FTY_PROTO_ASSET)) {
                    const char* type = fty_proto_aux_string(proto, "type", "<null>");
                    if (streq(type, "datacenter")) {
                        s_set_dc_upses(server, proto);
                    }
                    else {
                        log_trace("%s: invalid asset type: %s", server->name, type);
                    }
                }
                else {
                    log_warning("%s: received unwanted stream message", server->name);
                }

                fty_proto_destroy(&proto);
            }

            zmsg_destroy(&msg);
        }
    }

    log_info("%s - ending", server->name);

    // save
    if (fty_kpi_power_uptime_server_save_state(server) != 0) {
        log_error("failed to save state");
    }

    zactor_destroy(&kpi_power_metric_pull);
    zpoller_destroy(&poller);
    mlm_client_destroy(&client);

    log_info("%s - ended", server->name);

    fty_kpi_power_uptime_server_destroy(&server);
}
