idf_component_register(
    SRCS main.c pet.c watch_clock.c
    INCLUDE_DIRS .)

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
