cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

########################################################################################################################

etn_target(exe ${PROJECT_NAME}
    SOURCES
        src/fty_kpi_power_uptime.cc
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/../lib/src
    USES_PRIVATE
        ${PROJECT_NAME}-lib
)

########################################################################################################################

etn_target(exe ${PROJECT_NAME}-convert
    SOURCES
        src/fty_kpi_power_uptime_convert.cc
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/../lib/src
    USES_PRIVATE
        ${PROJECT_NAME}-lib
)

########################################################################################################################
