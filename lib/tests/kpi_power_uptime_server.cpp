#include "src/fty_kpi_power_uptime_server.h"
#include <catch2/catch.hpp>
#include <fty_shm.h>
#include <malamute.h>

TEST_CASE("kpi power uptime server test")
{
    fty_shm_set_test_dir(".");
    fty_shm_set_default_polling_interval(10);

    static const char* endpoint = "inproc://upt-server-test";
    zactor_t*          broker   = zactor_new(mlm_server, const_cast<char*>("Malamute"));
    zstr_sendx(broker, "BIND", endpoint, nullptr);

    mlm_client_t* ui_metr = mlm_client_new();
    mlm_client_connect(ui_metr, endpoint, 1000, "UI-M");

    //    mlm_client_t *ups = mlm_client_new ();
    //    mlm_client_connect (ups, endpoint, 1000, "UPS");
    //    mlm_client_set_producer (ups, "METRICS");

    mlm_client_t* ups_dc = mlm_client_new();
    mlm_client_connect(ups_dc, endpoint, 1000, "UPS_DC");
    mlm_client_set_producer(ups_dc, "ASSETS");

    fty_kpi_power_uptime_server_t* kpi = fty_kpi_power_uptime_server_new();

    zactor_t* server = zactor_new(fty_kpi_power_uptime_server, const_cast<char*>("uptime"));

    zstr_sendx(server, "CONFIG", ".", nullptr);
    zsock_wait(server);
    zstr_sendx(server, "CONNECT", endpoint, nullptr);
    zsock_wait(server);
    //    zstr_sendx (server, "CONSUMER", "METRICS", "status.ups.*", nullptr);
    //    zsock_wait (server);
    zstr_sendx(server, "CONSUMER", "ASSETS", "datacenter.unknown@.*", nullptr);
    zsock_wait(server);
    zclock_sleep(500); // THIS IS A HACK TO SETTLE DOWN THINGS

    // ---------- test of the new fn ------------------------
    zhash_t* aux = zhash_new();
    zhash_autofree(aux);
    zhash_insert(aux, "ups1", const_cast<char*>("roz.ups33"));
    zhash_insert(aux, "ups2", const_cast<char*>("roz.ups36"));
    zhash_insert(aux, "ups3", const_cast<char*>("roz.ups38"));
    zhash_insert(aux, "type", const_cast<char*>("datacenter"));
    zhash_insert(aux, "test", const_cast<char*>("test"));
    zmsg_t* msg = fty_proto_encode_asset(aux, "my-dc", "inventory", nullptr);
    REQUIRE(msg);
    fty_proto_t* fmsg = fty_proto_decode(&msg);
    REQUIRE(fmsg);

    s_set_dc_upses(kpi, fmsg);
    zclock_sleep(500);

    zhash_destroy(&aux);
    fty_proto_destroy(&fmsg);
    fty_kpi_power_uptime_server_destroy(&kpi);

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

    int rv = mlm_client_send(ups_dc, subject, &msg2);
    REQUIRE(rv == 0);
    zclock_sleep(500);
    zhash_destroy(&aux2);

    // set ups to on battery
    //    zmsg_t *metric = fty_proto_encode_metric (nullptr,
    //                                              time (nullptr),
    //                                              100,
    //                                              "status.ups",
    //                                              "roz.ups33",
    //                                              "16",
    //                                              ""
    //                                              );

    //    mlm_client_send (ups, "status.ups@roz.ups33", &metric);
    fty::shm::write_metric("roz.ups33", "status.ups", "16", "", 100);

    char *subject2, *command, *total, *offline;
    zclock_sleep(10000);
    zmsg_t* req = zmsg_new();
    zmsg_addstrf(req, "%s", "UPTIME");
    zmsg_addstrf(req, "%s", "my-dc");
    mlm_client_sendto(ui_metr, "uptime", "UPTIME", nullptr, 5000, &req);

    zclock_sleep(3000);
    int r = mlm_client_recvx(ui_metr, &subject2, &command, &total, &offline, nullptr);
    REQUIRE(r != -1);
    CHECK(streq(subject2, "UPTIME"));
    CHECK(streq(command, "UPTIME"));
    CHECK(atoi(total) > 0);
    CHECK(atoi(offline) > 0);

    // zmsg_destroy (&metric);
    zstr_free(&subject2);
    zstr_free(&command);
    zstr_free(&total);
    zstr_free(&offline);

    mlm_client_destroy(&ups_dc);
    //    mlm_client_destroy (&ups);
    mlm_client_destroy(&ui_metr);
    zactor_destroy(&server);
    zactor_destroy(&broker);

    // test for private function only!! UGLY REDONE DO NOT READ!!
    fty_kpi_power_uptime_server_t* s = fty_kpi_power_uptime_server_new();

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
    fty_shm_delete_test_dir();
}
