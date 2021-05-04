cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

########################################################################################################################

#Create the target
etn_target(exe ${PROJECT_NAME}
    SOURCES
        src/fty_kpi_power_uptime.cc
    USES_PRIVATE
        ${PROJECT_NAME}-lib
)

etn_target(exe ${PROJECT_NAME}-convert
    SOURCES
        src/fty_kpi_power_uptime_convert.cc
    USES_PRIVATE
        ${PROJECT_NAME}-lib
)
