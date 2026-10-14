idf_component_register(
    SRCS main.c pet.c watch_clock.c watch_home_clock.c watch_cartoon_68.c watch_wifi.c watch_settings.c watch_font_24.c watch_time_sync.c watch_power.c watch_battery_log.c watch_panel.c watch_idle_diag.c
    INCLUDE_DIRS .)

option(WATCH_ENABLE_POWER_MANAGEMENT "Use watch power policy (OFF only for timed A/B comparison)" ON)
option(WATCH_BATTERY_LOG_ENABLED "Low-rate rolling battery history for day-long observation" ON)
target_compile_definitions(${COMPONENT_LIB} PRIVATE WATCH_BATTERY_LOG_ENABLED=$<BOOL:${WATCH_BATTERY_LOG_ENABLED}>)
option(WATCH_OPT_DFS "Allow idle CPU frequency scaling" ON)
option(WATCH_OPT_TICK "Replace periodic LVGL tick with monotonic clock" ON)
option(WATCH_OPT_SCENE "Pause hidden cloud and wind work" ON)
option(WATCH_OPT_SLEEP "Allow automatic light sleep while screen is black" ON)
foreach(WATCH_OPT DFS TICK SCENE SLEEP)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WATCH_OPT_${WATCH_OPT}=$<BOOL:${WATCH_OPT_${WATCH_OPT}}>)
endforeach()
set(WATCH_CPU_MIN_MHZ 80 CACHE STRING "Idle CPU frequency: 40 or 80 MHz")
set(WATCH_CPU_MAX_MHZ 240 CACHE STRING "Busy CPU frequency: 80, 160 or 240 MHz")
if(NOT WATCH_CPU_MIN_MHZ MATCHES "^(40|80)$" OR NOT WATCH_CPU_MAX_MHZ MATCHES "^(80|160|240)$")
    message(FATAL_ERROR "Unsupported watch CPU frequency range")
endif()
target_compile_definitions(${COMPONENT_LIB} PRIVATE WATCH_CPU_MIN_MHZ=${WATCH_CPU_MIN_MHZ}
    WATCH_CPU_MAX_MHZ=${WATCH_CPU_MAX_MHZ})
if(WATCH_ENABLE_POWER_MANAGEMENT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WATCH_POWER_ENABLED=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WATCH_POWER_ENABLED=0)
endif()

# Raw images remain editable inputs; the lossless overlay is a build artifact.
# Disable with `idf.py -D PET_PACK_RESOURCES=OFF build` for a raw A/B build.
option(PET_PACK_RESOURCES "Losslessly compress firmware image resources" ON)
if(PET_PACK_RESOURCES)
    set(PET_PACK_DIR "${CMAKE_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${PET_PACK_DIR}")
    file(GLOB PET_RAW_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.c")
    set(PET_PACK_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/../tools/pack_resources.py")
    add_custom_command(
        OUTPUT "${PET_PACK_DIR}/pet_resources.stamp"
        BYPRODUCTS "${PET_PACK_DIR}/pet_packed_assets.c" "${PET_PACK_DIR}/resource_manifest.json"
                   "${PET_PACK_DIR}/pet_resource_test_entries.h"
        COMMAND "${PYTHON}" "${PET_PACK_SCRIPT}" --source "${CMAKE_CURRENT_LIST_DIR}" --output "${PET_PACK_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${PET_PACK_DIR}/pet_resources.stamp"
        DEPENDS ${PET_RAW_ASSETS} "${PET_PACK_SCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/../tools/lz4_host_memory.c"
                "${CMAKE_CURRENT_LIST_DIR}/../../lvgl-simulator/lvgl/src/libs/lz4/lz4.c"
                "${CMAKE_CURRENT_LIST_DIR}/../../lvgl-simulator/lvgl/src/libs/lz4/lz4.h"
                "${CMAKE_CURRENT_LIST_DIR}/../../lvgl-simulator/lv_conf.h"
        VERBATIM)
    add_custom_target(pet_resource_pack DEPENDS "${PET_PACK_DIR}/pet_resources.stamp")
    add_dependencies(${COMPONENT_LIB} pet_resource_pack)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${PET_PACK_DIR}")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PET_USE_PACKED_ASSETS=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PET_USE_PACKED_ASSETS=0)
endif()

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(${LVGL_LIB} PRIVATE -DLV_LVGL_H_INCLUDE_SIMPLE)

option(WATCH_IDLE_DIAGNOSTICS "Two read-only peripheral snapshots per boot" ON)
target_compile_definitions(${COMPONENT_LIB} PRIVATE WATCH_IDLE_DIAGNOSTICS=$<BOOL:${WATCH_IDLE_DIAGNOSTICS}>)
