cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

##############################################################################################################

etn_target(static ${PROJECT_NAME}-lib
    SOURCES
        src/dc.cc
        src/dc.h
        src/fty_kpi_power_uptime_server.cc
        src/fty_kpi_power_uptime_server.h
        src/upt.cc
        src/upt.h
    USES
        czmq
        fty_common_logging
        fty_proto
        fty_shm
        mlm
    PRIVATE
)

##############################################################################################################

etn_test_target(${PROJECT_NAME}-lib
    SOURCES
        tests/dc.cpp
        tests/kpi_power_uptime_server.cpp
        tests/main.cpp
        tests/upt.cpp
    PREPROCESSOR
        -DCATCH_CONFIG_FAST_COMPILE
    SUBDIR
        tests
)

##############################################################################################################
