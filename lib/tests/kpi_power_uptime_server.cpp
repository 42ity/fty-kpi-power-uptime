/*  ========================================================================
    Copyright (C) 2020 Eaton
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
    ========================================================================
*/

#include <catch2/catch.hpp>

#include "src/fty_kpi_power_uptime_server.h"
#include <fty_shm.h>
#include <fty_log.h>
#include <malamute.h>

TEST_CASE("kpi power uptime server test")
{
    fty_shm_set_test_dir(".");
    fty_shm_set_default_polling_interval(10);

    static const char* ENDPOINT = "inproc://upt-server-test";
    const char* SERVER_ADDRESS = "kpi-power-uptime-test"; //upt address name

    zactor_t* connector = zactor_new(mlm_server, const_cast<char*>("Malamute"));
    REQUIRE(connector);
    zstr_sendx(connector , "BIND", ENDPOINT, nullptr);

    // msg sender
    mlm_client_t* ui_metr = mlm_client_new();
    REQUIRE(ui_metr);
    mlm_client_connect(ui_metr, ENDPOINT, 1000, "UI-M");

    // ASSET publisher
    mlm_client_t* ups_dc = mlm_client_new();
    REQUIRE(ups_dc);
    mlm_client_connect(ups_dc, ENDPOINT, 1000, "UPS_DC");
    mlm_client_set_producer(ups_dc, "ASSETS");

    zactor_t* upt_server = zactor_new(fty_kpi_power_uptime_server, const_cast<char*>(SERVER_ADDRESS));
    REQUIRE(upt_server);
    zstr_sendx(upt_server, "CONFIG", ".", nullptr);
    zsock_wait(upt_server);
    zstr_sendx(upt_server, "CONNECT", ENDPOINT, nullptr);
    zsock_wait(upt_server);
    zstr_sendx(upt_server, "CONSUMER", "ASSETS", "datacenter.unknown@.*", nullptr);
    zsock_wait(upt_server);

    zclock_sleep(500); //sync

    {
        // ---------- test of the new fn ------------------------
        zhash_t* aux = zhash_new();
        zhash_autofree(aux);
        zhash_insert(aux, "ups1", const_cast<char*>("roz.ups33"));
        zhash_insert(aux, "ups2", const_cast<char*>("roz.ups36"));
        zhash_insert(aux, "ups3", const_cast<char*>("roz.ups38"));
        zhash_insert(aux, "type", const_cast<char*>("datacenter"));
        zhash_insert(aux, "test", const_cast<char*>("test"));
        zmsg_t* msg = fty_proto_encode_asset(aux, "my-dc", "inventory", nullptr);
        zhash_destroy(&aux);
        REQUIRE(msg);
        fty_proto_t* fmsg = fty_proto_decode(&msg);
        REQUIRE(fmsg);

        fty_kpi_power_uptime_server_t* kpi = fty_kpi_power_uptime_server_new(NULL);
        REQUIRE(kpi);
        s_set_dc_upses(kpi, fmsg);
        fty_proto_destroy(&fmsg);
        fty_kpi_power_uptime_server_destroy(&kpi);
    }

    // -------------- test of the whole component ----------
    const char* subject = "datacenter.unknown@my-dc";

    zhash_t* aux2 = zhash_new();
    zhash_autofree(aux2);
    zhash_insert(aux2, "ups1", const_cast<char*>("roz.ups33"));
    zhash_insert(aux2, "ups2", const_cast<char*>("roz.ups36"));
    zhash_insert(aux2, "ups3", const_cast<char*>("roz.ups38"));
    zhash_insert(aux2, "test", const_cast<char*>("test"));
    zhash_insert(aux2, "type", const_cast<char*>("datacenter"));

    zmsg_t* msg2 = fty_proto_encode_asset(aux2, "my-dc", "inventory", nullptr);
    zhash_destroy(&aux2);
    int rv = mlm_client_send(ups_dc, subject, &msg2);
    REQUIRE(rv == 0);

    zclock_sleep(500); //sync

    // set ups to on battery
    fty::shm::write_metric("roz.ups33", "status.ups", "16", "", 100);
    zclock_sleep(10000);

    char *subject2, *command, *total, *offline;
    zmsg_t* req = zmsg_new();
    zmsg_addstrf(req, "%s", "UPTIME");
    zmsg_addstrf(req, "%s", "my-dc");
    mlm_client_sendto(ui_metr, SERVER_ADDRESS, "UPTIME", nullptr, 5000, &req);
    zmsg_destroy(&req);

    zclock_sleep(3000);
    int r = mlm_client_recvx(ui_metr, &subject2, &command, &total, &offline, nullptr);
    REQUIRE(r != -1);
    CHECK(streq(subject2, "UPTIME"));
    CHECK(streq(command, "UPTIME"));
    CHECK(atoi(total) > 0);
    CHECK(atoi(offline) > 0);

    zstr_free(&subject2);
    zstr_free(&command);
    zstr_free(&total);
    zstr_free(&offline);

    // test for private function only!! UGLY REDONE DO NOT READ!!
    log_debug("test for private function");
    {
        fty_kpi_power_uptime_server_t* s = fty_kpi_power_uptime_server_new(NULL);

        upt_t*    upt  = upt_new();
        zlistx_t* upsl = zlistx_new();

        zlistx_add_end(upsl, const_cast<char*>("UPS007"));
        zlistx_add_end(upsl, const_cast<char*>("UPS006"));
        r = upt_add(upt, "DC007", upsl);
        REQUIRE(r == 0);

        fty_kpi_power_uptime_server_set_dir(s, ".");
        zclock_sleep(1000);
        r = fty_kpi_power_uptime_server_save_state(s);
        REQUIRE(r == 0);
        r = fty_kpi_power_uptime_server_load_state(s);
        REQUIRE(r == 0);

        zlistx_destroy(&upsl);
        upt_destroy(&upt);
        fty_kpi_power_uptime_server_destroy(&s);
    }

    log_debug("cleanup");
    mlm_client_destroy(&ups_dc);
    mlm_client_destroy(&ui_metr);
    zactor_destroy(&upt_server);
    zactor_destroy(&connector);
    fty_shm_delete_test_dir();
}
