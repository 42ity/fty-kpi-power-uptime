cmake_minimum_required(VERSION 3.13)
cmake_policy(VERSION 3.13)

##############################################################################################################

project(fty-kpi-power-uptime
    VERSION 1.0.0
    DESCRIPTION "Compute Data Center uptime"
)

########################################################################################################################

find_package(fty-cmake PATHS ${CMAKE_BINARY_DIR})

########################################################################################################################

add_subdirectory(lib)
add_subdirectory(agent)

#install resources files
set(KPI_SETTINGS_DIR "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/fty/${PROJECT_NAME}")
set(KPI_USER "bios")

configure_file("${PROJECT_SOURCE_DIR}/resources/fty-kpi-power-uptime.cfg.in"    "${PROJECT_BINARY_DIR}/resources/fty-kpi-power-uptime.cfg" @ONLY)
install(FILES "${PROJECT_BINARY_DIR}/resources/fty-kpi-power-uptime.cfg" DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT_NAME})

configure_file("${PROJECT_SOURCE_DIR}/resources/fty-kpi-power-uptime.service.in"    "${PROJECT_BINARY_DIR}/resources/fty-kpi-power-uptime.service" @ONLY)
install(FILES "${PROJECT_BINARY_DIR}/resources/fty-kpi-power-uptime.service" DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system/)