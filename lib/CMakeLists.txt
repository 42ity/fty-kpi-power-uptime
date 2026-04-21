cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

##############################################################################################################

etn_target(static ${PROJECT_NAME}-lib
    SOURCES
        src/dc.h
        src/dc.cc
        src/upt.h
        src/upt.cc
        src/fty_kpi_power_uptime_server.h
        src/fty_kpi_power_uptime_server.cc
    USES
        czmq
        mlm
        fty_common_logging
        fty_proto
        fty_shm
    PRIVATE
)

##############################################################################################################

etn_test_target(${PROJECT_NAME}-lib
    SOURCES
        tests/main.cpp
        tests/dc.cpp
        tests/upt.cpp
        tests/kpi_power_uptime_server.cpp
    PREPROCESSOR
        -DCATCH_CONFIG_FAST_COMPILE
    SUBDIR
        tests
)

##############################################################################################################
