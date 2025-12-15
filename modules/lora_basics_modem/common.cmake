# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

#-----------------------------------------------------------------------------
# Regions
#-----------------------------------------------------------------------------

include(${CMAKE_CURRENT_LIST_DIR}/regions.cmake)

#-----------------------------------------------------------------------------
# Relay
#-----------------------------------------------------------------------------

include(${CMAKE_CURRENT_LIST_DIR}/relay.cmake)

#-----------------------------------------------------------------------------
# Compile Definitions Interface
#-----------------------------------------------------------------------------

# Create an INTERFACE library that contains all LBM compile definitions.
# This library can be linked by applications that need the same definitions as LBM.
# Applications can use: target_link_libraries(app PRIVATE lbm_compile_definitions)
add_library(lbm_compile_definitions INTERFACE)

# Core definitions
target_compile_definitions(lbm_compile_definitions INTERFACE
  NUMBER_OF_STACKS=${CONFIG_LORA_BASICS_MODEM_NUMBER_OF_STACKS}
)

# Conditional definitions using generator expressions
target_compile_definitions(lbm_compile_definitions INTERFACE
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_CLASS_B}>:ADD_CLASS_B>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_CLASS_C}>:ADD_CLASS_C>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_CSMA}>:ADD_CSMA>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_CSMA_BY_DEFAULT}>:ENABLE_CSMA_BY_DEFAULT>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_ALMANAC}>:ADD_ALMANAC>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_ALC_SYNC}>:ADD_SMTC_ALC_SYNC>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_GEOLOCATION}>:ADD_LBM_GEOLOCATION>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_STREAM}>:ADD_SMTC_STREAM>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_LFU}>:ADD_SMTC_LFU>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_DEVICE_MANAGEMENT}>:ADD_SMTC_CLOUD_DEVICE_MANAGEMENT>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_STORE_AND_FORWARD}>:ADD_SMTC_STORE_AND_FORWARD>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_BEACON_TX}>:MODEM_BEACON_APP>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_MULTICAST}>:SMTC_MULTICAST>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_RELAY_RX}>:ADD_RELAY_RX>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_RELAY_TX}>:ADD_RELAY_TX>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_RELAY_TX}>:USE_RELAY_TX>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_PERF_TEST}>:PERF_TEST_ENABLED>
  $<$<BOOL:${CONFIG_LORA_BASICS_MODEM_DISABLE_JOIN_DUTY_CYCLE}>:TEST_BYPASS_JOIN_DUTY_CYCLE>
)

# FUOTA definitions with values (cannot use generator expressions for value assignment)
if(CONFIG_LORA_BASICS_MODEM_FUOTA)
  target_compile_definitions(lbm_compile_definitions INTERFACE
    ADD_FUOTA=${CONFIG_LORA_BASICS_MODEM_FUOTA_VERSION}
  )
endif()

if(CONFIG_LORA_BASICS_MODEM_FUOTA_FMP)
  target_compile_definitions(lbm_compile_definitions INTERFACE
    ENABLE_FUOTA_FMP
  )
endif()

if(CONFIG_LORA_BASICS_MODEM_FUOTA_MPA)
  target_compile_definitions(lbm_compile_definitions INTERFACE
    ENABLE_FUOTA_MPA
  )
endif()

if(CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_NB_OF_FRAGMENTS_CONFIG)
  target_compile_definitions(lbm_compile_definitions INTERFACE
    FRAG_MAX_NB=${CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_NB_OF_FRAGMENTS}
  )
endif()

if(CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_SIZE_OF_FRAGMENTS)
  target_compile_definitions(lbm_compile_definitions INTERFACE
    FRAG_MAX_SIZE=${CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_SIZE_OF_FRAGMENTS}
  )
endif()

if(CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_FRAGMENTS_REDUNDANCY)
  target_compile_definitions(lbm_compile_definitions INTERFACE
    FRAG_MAX_REDUNDANCY=${CONFIG_LORA_BASICS_MODEM_FUOTA_MAX_FRAGMENTS_REDUNDANCY}
  )
endif()

# Create an alias with namespace (best practice)
add_library(lbm::definitions ALIAS lbm_compile_definitions)

#-----------------------------------------------------------------------------
# Common
#-----------------------------------------------------------------------------

# Note: The LBM library receives all compile definitions via linkage against
# lbm_compile_definitions in CMakeLists.txt. No need to redefine them here.

# SMTC_MODEM_CORE_C_SOURCES
zephyr_library_sources(
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_api/lorawan_api.c
  ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem.c
  ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_test.c
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_utilities/modem_event_utilities.c
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_utilities/fifo_ctrl.c
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_utilities/modem_core.c
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_supervisor/modem_supervisor_light.c
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_supervisor/modem_tx_protocol_manager.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/lorawan_certification/lorawan_certification.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_manager/lorawan_join_management.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_manager/lorawan_send_management.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_manager/lorawan_cid_request_management.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_manager/lorawan_dwn_ack_management.c
)

# LR1MAC_C_SOURCES
zephyr_library_sources(
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/lr1_stack_mac_layer.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/lr1mac_core.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/lr1mac_utilities.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/smtc_real/src/smtc_real.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/services/smtc_duty_cycle.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/services/smtc_lbt.c
)

# SMTC_MODEM_CRYPTO_C_SOURCES
zephyr_library_sources(
  ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto/smtc_modem_crypto.c
)

# RADIO_PLANNER_C_SOURCES & RAL/RALF SOURCES
if(NOT CONFIG_USP_LORA_BASICS_MODEM)
  zephyr_library_sources(
    ${LBM_SMTC_MODEM_CORE_DIR}/radio_planner/src/radio_planner.c
  )
  zephyr_include_directories(
    # NOTE: it is only used by the samples, could be cleaned up.
    ${LBM_LIB_DIR}/smtc_modem_hal
  )
  zephyr_library_include_directories(
    ${LBM_SMTC_MODEM_CORE_DIR}/radio_planner/src
    ${LBM_SMTC_MODEM_CORE_DIR}/smtc_ral/src
    ${LBM_SMTC_MODEM_CORE_DIR}/smtc_ralf/src
  )
endif() # if(NOT CONFIG_USP_LORA_BASICS_MODEM)

zephyr_include_directories(
  ${LBM_LIB_DIR}/smtc_modem_api
)

zephyr_library_include_directories(
  ${LBM_LIB_DIR}
  ${LBM_SMTC_MODEM_CORE_DIR}
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_api
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_manager
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/lorawan_certification
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/services
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/smtc_real/src
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_supervisor
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_utilities
  ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto
  ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto/smtc_secure_element
)

#-----------------------------------------------------------------------------
# ALCSync
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_ALC_SYNC_V1
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/application_layer_clock_synchronization/v1.0.0/lorawan_alcsync_v1.0.0.c
)

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_ALC_SYNC_V2
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/application_layer_clock_synchronization/v2.0.0/lorawan_alcsync_v2.0.0.c
)

zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_ALC_SYNC
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/application_layer_clock_synchronization
)

#-----------------------------------------------------------------------------
# FUOTA
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_FUOTA_V1
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/fragmented_data_block_transport/v1.0.0/fragmentation_helper_v1.0.0.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/fragmented_data_block_transport/v1.0.0/lorawan_fragmentation_package_v1.0.0.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/remote_multicast_setup/v1.0.0/lorawan_remote_multicast_setup_package_v1.0.0.c
)
zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_FUOTA_V2
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/fragmented_data_block_transport/v2.0.0/fragmentation_helper_v2.0.0.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/fragmented_data_block_transport/v2.0.0/lorawan_fragmentation_package_v2.0.0.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/remote_multicast_setup/v2.0.0/lorawan_remote_multicast_setup_package_v2.0.0.c
)
zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_FUOTA_FMP
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/firmware_management_protocol/lorawan_fmp_package.c
)
zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_FUOTA_MPA
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/multi_package_access/lorawan_mpa_package.c
)

zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_FUOTA
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/application_layer_clock_synchronization
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/fragmented_data_block_transport
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/remote_multicast_setup
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/firmware_management_protocol/
)
zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_FUOTA_V1
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/fragmented_data_block_transport/v1.0.0
)
zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_FUOTA_V2
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/fragmented_data_block_transport/v2.0.0
)
zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_FUOTA_FMP
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/firmware_management_protocol
)
zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_FUOTA_MPA
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_packages/multi_package_access
)

#-----------------------------------------------------------------------------
# Crypto soft
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_SOFT
  ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto/soft_secure_element/aes.c
  ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto/soft_secure_element/cmac.c
  ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto/soft_secure_element/soft_se.c
)
zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_SOFT
  ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto/soft_secure_element
)

#-----------------------------------------------------------------------------
# Class B
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_CLASS_B
  ${LBM_SMTC_MODEM_CORE_DIR}/lorawan_manager/lorawan_class_b_management.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/lr1mac_class_b/smtc_beacon_sniff.c
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/lr1mac_class_b/smtc_ping_slot.c
)

zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_CLASS_B
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/lr1mac_class_b
)

#-----------------------------------------------------------------------------
# Class C
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_CLASS_C
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/lr1mac_class_c/lr1mac_class_c.c
)

zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_CLASS_C
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/lr1mac_class_c
)

#-----------------------------------------------------------------------------
# Multicast
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_MULTICAST
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/services/smtc_multicast/smtc_multicast.c
)

zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_MULTICAST
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/services/smtc_multicast
)

#-----------------------------------------------------------------------------
# CSMA
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_CSMA
  ${LBM_SMTC_MODEM_CORE_DIR}/lr1mac/src/services/smtc_lora_cad_bt.c
)

#-----------------------------------------------------------------------------
# Almanac
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_ALMANAC
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/almanac_packages/almanac.c
)

zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_ALMANAC
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/almanac_packages
)

#-----------------------------------------------------------------------------
# Stream
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_STREAM
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/stream_packages/stream.c
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/stream_packages/rose.c
)

zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_STREAM
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/stream_packages
)

#-----------------------------------------------------------------------------
# Large File Upload
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_LFU
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/lfu_service/file_upload.c
)
zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_LFU
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/lfu_service
)

#-----------------------------------------------------------------------------
# Device Management
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_DEVICE_MANAGEMENT
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/cloud_dm_package/cloud_dm_package.c
)
zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_DEVICE_MANAGEMENT
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/cloud_dm_package
)

#-----------------------------------------------------------------------------
# Geolocation
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_GEOLOCATION
  ${LBM_SMTC_MODEM_CORE_DIR}/geolocation_services/mw_common.c
  ${LBM_SMTC_MODEM_CORE_DIR}/geolocation_services/mw_gnss_scan.c
  ${LBM_SMTC_MODEM_CORE_DIR}/geolocation_services/mw_gnss_send.c
  ${LBM_SMTC_MODEM_CORE_DIR}/geolocation_services/mw_gnss_almanac.c
  ${LBM_SMTC_MODEM_CORE_DIR}/geolocation_services/gnss_helpers.c
  ${LBM_SMTC_MODEM_CORE_DIR}/geolocation_services/mw_wifi_scan.c
  ${LBM_SMTC_MODEM_CORE_DIR}/geolocation_services/mw_wifi_send.c
  ${LBM_SMTC_MODEM_CORE_DIR}/geolocation_services/wifi_helpers.c
)

# Used in publicly-included headers
zephyr_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_GEOLOCATION
  ${LBM_SMTC_MODEM_CORE_DIR}/geolocation_services
)

#-----------------------------------------------------------------------------
# Store and Forward
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_STORE_AND_FORWARD
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_utilities/circularfs.c
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/store_and_forward/store_and_forward_flash.c
)
zephyr_library_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_STORE_AND_FORWARD
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/store_and_forward
)

#-----------------------------------------------------------------------------
# Beacon TX
#-----------------------------------------------------------------------------

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_BEACON_TX
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/beacon_tx_service/lorawan_beacon_tx_service_example.c
)

zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_MULTICAST
  ${LBM_SMTC_MODEM_CORE_DIR}/modem_services/beacon_tx_service
)

#-----------------------------------------------------------------------------
# Misc
#-----------------------------------------------------------------------------

if (CONFIG_LORA_BASICS_MODEM_DISABLE_JOIN_DUTY_CYCLE)
  message(WARNING "
    Warning: LORA_BASICS_MODEM_DISABLE_JOIN_DUTY_CYCLE should only be enabled
    when testing in an isolated setup (e.g with coaxial cables between gateway
    and device), and should absolutely not be enabled when emitting in open air.")
endif()
