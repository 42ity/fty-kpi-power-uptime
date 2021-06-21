#include "src/upt.h"
#include <catch2/catch.hpp>

TEST_CASE("upt test")
{
    const char* SELFTEST_DIR_RW = ".";
    char*       state_file      = zsys_sprintf("%s/state-upt", SELFTEST_DIR_RW);
    REQUIRE(state_file != nullptr);

    //  @selftest
    upt_t* uptime = upt_new();
    REQUIRE(uptime);
    upt_destroy(&uptime);
    CHECK(!uptime);

    uptime = upt_new();
    uint64_t total, offline;
    int      r;

    r = upt_uptime(uptime, "TEST", &total, &offline);
    REQUIRE(r == -1);
    CHECK(total == 0);
    CHECK(offline == 0);

    zlistx_t* ups = nullptr;
    r             = upt_add(uptime, "UPS007", ups);
    REQUIRE(r == 0);
    zlistx_destroy(&ups);

    r = upt_uptime(uptime, "TEST", &total, &offline);
    REQUIRE(r == -1);
    CHECK(total == 0);
    CHECK(offline == 0);

    const char* dc_name;
    dc_name = upt_dc_name(uptime, "UPS007");
    CHECK(!dc_name);

    ups = zlistx_new();
    zlistx_add_end(ups, const_cast<char*>("UPS001"));
    zlistx_add_end(ups, const_cast<char*>("UPS007"));

    r = upt_add(uptime, "DC007", ups);
    REQUIRE(r == 0);
    zlistx_destroy(&ups);

    dc_name = upt_dc_name(uptime, "UPS007");
    CHECK(streq(dc_name, "DC007"));

    dc_name = upt_dc_name(uptime, "UPS042");
    CHECK(!dc_name);

    upt_set_online(uptime, "UPS001");
    CHECK(!upt_is_offline(uptime, "DC007"));

    upt_set_offline(uptime, "UPS007");
    CHECK(upt_is_offline(uptime, "DC007"));

    upt_set_online(uptime, "DC042");
    CHECK(upt_is_offline(uptime, "DC007"));

    // uptime works with 1sec precision, so let sleep
    zclock_sleep(2000);

    r = upt_uptime(uptime, "DC007", &total, &offline);
    REQUIRE(r == 0);
    CHECK(total > 1);
    CHECK(offline > 1);

    upt_set_online(uptime, "UPS007");
    CHECK(!upt_is_offline(uptime, "DC007"));

    zclock_sleep(1000);
    r = upt_uptime(uptime, "DC007", &total, &offline);
    REQUIRE(r == 0);
    CHECK(total > 2);
    CHECK(offline > 1);

    // test UPS removal
    ups = zlistx_new();
    zlistx_add_end(ups, const_cast<char*>("UPS007"));
    r = upt_add(uptime, "DC007", ups);

    REQUIRE(r == 0);
    zlistx_destroy(&ups);

    dc_name = upt_dc_name(uptime, "UPS001");
    CHECK(!dc_name);

    upt_t* uptime2 = upt_new();
    //    upt_t *uptime3 = upt_new ();
    zlistx_t* ups2 = zlistx_new();
    ups            = zlistx_new();

    zlistx_add_end(ups, const_cast<char*>("UPS007"));
    zlistx_add_end(ups, const_cast<char*>("UPS006"));
    zlistx_add_end(ups, const_cast<char*>("UPS005"));
    zlistx_add_end(ups2, const_cast<char*>("UPS001"));
    zlistx_add_end(ups2, const_cast<char*>("UPS002"));
    zlistx_add_end(ups2, const_cast<char*>("UPS003"));

    r = upt_add(uptime2, "DC007", ups);
    REQUIRE(r == 0);
    r = upt_add(uptime2, "DC006", ups2);
    REQUIRE(r == 0);

    zlistx_add_end(ups2, const_cast<char*>("UPS033"));
    r = upt_add(uptime2, "DC006", ups2);
    REQUIRE(r == 0);

    upt_set_offline(uptime2, "UPS007");
    CHECK(upt_is_offline(uptime2, "DC007"));

    // uptime works with 1sec precision, so let sleep
    zclock_sleep(2000);

    total   = 0;
    offline = 0;
    r       = upt_uptime(uptime, "DC007", &total, &offline);
    REQUIRE(r == 0);
    CHECK(total > 1);
    CHECK(offline > 1);

    r = upt_save(uptime2, state_file);
    REQUIRE(r == 0);

    upt_t* uptime3 = upt_load(state_file);
    CHECK(zhashx_size(uptime3->ups2dc) == (zlistx_size(ups) + zlistx_size(ups2)));
    CHECK(zhashx_size(uptime3->dc) == 2);

    total   = 0;
    offline = 0;
    r       = upt_uptime(uptime, "DC007", &total, &offline);
    REQUIRE(r == 0);
    CHECK(total > 1);
    CHECK(offline > 1);

    zstr_free(&state_file);

    zlistx_destroy(&ups);
    zlistx_destroy(&ups2);
    upt_destroy(&uptime);
    upt_destroy(&uptime2);
    upt_destroy(&uptime3);
}
