cmake_minimum_required(VERSION 3.0)

PROJECT(PluginRoutingInterfacePulse VERSION 7.4.0)

find_package(PkgConfig)
PKG_CHECK_MODULES(DBUS "dbus-1 >= 1.4" REQUIRED)
PKG_CHECK_MODULES(PULSE_MODULE_DEV REQUIRED pulseaudio-module-devel)

# all source files go here
file(GLOB PLUGINPULSE_SRCS_C "*.c")

function(pkg_check_variable _pkg _name)
    string(TOUPPER ${_pkg} _pkg_upper)
    string(TOUPPER ${_name} _name_upper)
    string(REPLACE "-" "_" _pkg_upper ${_pkg_upper})
    string(REPLACE "-" "_" _name_upper ${_name_upper})
    set(_output_name "${_pkg_upper}_${_name_upper}")

    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=${_name} ${_pkg}
                    OUTPUT_VARIABLE _pkg_result
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    set("${_output_name}" "${_pkg_result}" CACHE STRING "pkg-config variable ${_name} of ${_pkg}")
endfunction()


ADD_DEFINITIONS(${dependencies_CFLAGS})
SET(include_dirs ${INCLUDE_DIRS}  ${PULSE_MODULE_DEV_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${DBUS_INCLUDE_DIRS})
SET(link_libraries ${LINK_LIBRARIES} ${PULSE_MODULE_DEV_LIBRARIES})
STRING(REGEX REPLACE ";" " " link_flags "${PULSE_MODULE_DEV_LDFLAGS}" "")
pkg_check_variable(pulseaudio-module-devel modlibexecdir)

SET(plugin_install_dir ${PULSEAUDIO_MODULE_DEVEL_MODLIBEXECDIR})


add_library(module-router MODULE ${PLUGINPULSE_SRCS_C})
INCLUDE_DIRECTORIES(${include_dirs})
TARGET_LINK_LIBRARIES(module-router ${link_libraries})

SET_TARGET_PROPERTIES(module-router PROPERTIES PREFIX ""
                    LINK_FLAGS "${link_flags} -Wl,-rpath=${plugin_install_dir}")

INSTALL(TARGETS module-router
        LIBRARY DESTINATION ${plugin_install_dir})
