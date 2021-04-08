cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

project(fty-kpi-power-uptime
    VERSION 1.0.0
    DESCRIPTION "Compute Data Center uptime"
)

##############################################################################################################
find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR}/fty-cmake)
##############################################################################################################

etn_target(static ${PROJECT_NAME}-lib
    SOURCES
        src/dc.cc
        src/dc.h
        src/upt.cc
        src/upt.h
        src/fty_kpi_power_uptime_server.cc
    PUBLIC_INCLUDE_DIR
        include
    PUBLIC
        *.h
    USES_PRIVATE
        czmq
        fty_common_logging
        fty_proto
        fty_shm
        libmlm
    PRIVATE
)

if (BUILD_TESTING)

    etn_test(${PROJECT_NAME}-test
        SOURCES
            tests/main.cc
            src/fty_kpi_power_uptime_private_selftest.cc
            src/dc.cc
            src/dc.h
            src/upt.cc
            src/upt.h
            src/fty_kpi_power_uptime_server.cc
        PREPROCESSOR -DCATCH_CONFIG_FAST_COMPILE
        USES
            czmq
            fty_common_logging
            fty_proto
            fty_shm
            libmlm
            Catch2::Catch2
            stdc++fs
            log4cplus
    )

     #copy selftest-ro for test
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/selftest-ro DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(MAKE_DIRECTORY  ${CMAKE_CURRENT_BINARY_DIR}/selftest-rw)

    #enable coverage
    etn_coverage(${PROJECT_NAME}-test)

    #copy selftest-ro for test coverage
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tests/selftest-ro DESTINATION ${CMAKE_BINARY_DIR})
    file(MAKE_DIRECTORY  ${CMAKE_BINARY_DIR}/selftest-rw)

endif()