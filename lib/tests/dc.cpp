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

#include "src/dc.h"

TEST_CASE("dc test")
{
    dc_t* dc = dc_new();
    REQUIRE(dc);

    CHECK(!dc_is_offline(dc));

    dc_set_online(dc, const_cast<char*>("UPS007"));
    CHECK(!dc_is_offline(dc));

    dc_set_offline(dc, const_cast<char*>("UPS001"));
    CHECK(dc_is_offline(dc));

    zclock_sleep(3000);

    uint64_t total, offline;
    dc_uptime(dc, &total, &offline);
    printf("total:  %" PRIi64, total);
    printf("offline:  %" PRIi64, offline);

    CHECK(total > 1);
    CHECK(offline > 1);

    dc_set_online(dc, const_cast<char*>("UPS001"));
    CHECK(!dc_is_offline(dc));

    zclock_sleep(3000);

    dc_uptime(dc, &total, &offline);
    CHECK(total > 2);
    CHECK(offline > 1);

    dc_destroy(&dc);
    CHECK(!dc);

    // pack/unpack
    dc = dc_new();
    REQUIRE(dc);

    // XXX: dirty tricks for test - class intentionally don't allow to test those directly
    dc->last_update = 42;
    dc->total       = 1042;
    dc->offline     = 17;
    dc_set_offline(dc, const_cast<char*>("UPS001"));
    dc_set_offline(dc, const_cast<char*>("UPS002"));
    dc_set_offline(dc, const_cast<char*>("UPS003"));

    zframe_t* frame = dc_pack(dc);
    REQUIRE(frame);

    dc_t* dc2 = dc_unpack(frame);
    REQUIRE(dc2);
    CHECK(dc->last_update == dc2->last_update);
    CHECK(dc->total == dc2->total);
    CHECK(dc->offline == dc2->offline);
    CHECK(dc_is_offline(dc2));
    CHECK(zlistx_size(dc2->ups) == 3);
    CHECK(streq(reinterpret_cast<char*>(zlistx_first(dc2->ups)), "UPS001"));
    CHECK(streq(reinterpret_cast<char*>(zlistx_next(dc2->ups)), "UPS002"));
    CHECK(streq(reinterpret_cast<char*>(zlistx_next(dc2->ups)), "UPS003"));

    zframe_destroy(&frame);
    dc_destroy(&dc2);
    dc_destroy(&dc);
    CHECK(!dc2);
    CHECK(!dc);

    // pack/unpack of empty struct
    dc    = dc_new();
    REQUIRE(dc);
    frame = dc_pack(dc);
    REQUIRE(frame);

    dc2 = dc_unpack(frame);
    REQUIRE(dc2);
    CHECK(zlistx_size(dc2->ups) == 0);

    zframe_destroy(&frame);
    dc_destroy(&dc2);
    dc_destroy(&dc);
}
