add_test(
  NAME emu.support.benchmark_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_benchmark_contract.py"
)

add_test(
  NAME emu.support.state_promotion_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_state_promotion_contract.py"
)

add_test(
  NAME emu.support.batch_lifecycle_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_batch_lifecycle_contract.py"
)

add_test(
  NAME emu.support.interactive_load_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_interactive_load_contract.py"
)

add_test(
  NAME emu.support.interactive_lifecycle_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_interactive_lifecycle_contract.py"
)

add_test(
  NAME emu.support.adapter_config_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_adapter_config_contract.py"
)

add_test(
  NAME emu.support.runtime_paths_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_runtime_paths_contract.py"
)

add_test(
  NAME emu.support.savestate_bridge_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_savestate_bridge_contract.py"
)

add_test(
  NAME emu.support.regression_runner_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_regression_runner_contract.py"
)

add_test(
  NAME emu.support.regression_compare_modes_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_regression_compare_modes_contract.py"
)

add_test(
  NAME emu.support.conformance_roms_contract
  COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_conformance_roms_contract.py"
)

set(EMU_BEHAVIOR_SUPPORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/support")

add_test(
  NAME emu.support.paper_mario_fixture_verification_contract
  COMMAND bash "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_paper_mario_fixture_verification_contract.sh"
)

add_test(
  NAME emu.support.paper_mario_memory_decoder_contract
  COMMAND python3 "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_paper_mario_memory_decoder_contract.py"
)

add_test(
  NAME emu.support.kmr03_import_remint_publication_contract
  COMMAND python3 "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_kmr03_import_remint_publication_contract.py"
)

add_test(
  NAME emu.support.paper_mario_authority_evidence_contract
  COMMAND python3 "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_paper_mario_authority_evidence_contract.py"
)

add_test(
  NAME emu.support.hires_capability_runtime_contract
  COMMAND bash "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_hires_capability_runtime_contract.sh"
)

add_test(
  NAME emu.support.hts2phrb_smoke
  COMMAND bash "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_hts2phrb_smoke.sh"
)

add_test(
  NAME emu.support.hts2phrb_round_trip
  COMMAND bash "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_hts2phrb_round_trip.sh"
)

add_test(
  NAME emu.support.hts2phrb_directory_input
  COMMAND bash "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_hts2phrb_directory_input.sh"
)

add_test(
  NAME emu.support.hts2phrb_loose_inputs_contract
  COMMAND python3 "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_hts2phrb_loose_inputs_contract.py"
)

add_test(
  NAME emu.support.hts2phrb_jabo_contract
  COMMAND python3 "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_hts2phrb_jabo_contract.py"
)

add_test(
  NAME emu.support.hts2phrb_bounded_memory_contract
  COMMAND python3 "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_hts2phrb_bounded_memory_contract.py"
)

add_test(
  NAME emu.support.hts2phrb_all_families
  COMMAND bash "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_hts2phrb_all_families.sh"
)

add_test(
  NAME emu.support.hts2phrb_gates
  COMMAND bash "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_hts2phrb_gates.sh"
)

add_test(
  NAME emu.support.hts2phrb_paper_mario_full_cache_contract
  COMMAND bash "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_hts2phrb_paper_mario_full_cache_contract.sh"
)
set_tests_properties(emu.support.hts2phrb_paper_mario_full_cache_contract PROPERTIES SKIP_RETURN_CODE 77)

add_test(
  NAME emu.support.gliden64_reference_guardrails
  COMMAND bash "${EMU_BEHAVIOR_SUPPORT_DIR}/emu_gliden64_reference_guardrails.sh"
)

add_executable(emu_unit_smoke_test
  emu_unit_smoke_test.cpp
  "${EMU_BEHAVIOR_SUPPORT_DIR}/rdp_memory_fixture.cpp"
)
target_include_directories(emu_unit_smoke_test PRIVATE
  "${EMU_BEHAVIOR_SUPPORT_DIR}"
  "${CMAKE_SOURCE_DIR}"
)
target_compile_features(emu_unit_smoke_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.smoke COMMAND emu_unit_smoke_test)

add_executable(emu_unit_seed_policy_test
  emu_unit_seed_policy_test.cpp
)
target_include_directories(emu_unit_seed_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
)
target_compile_features(emu_unit_seed_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.seed_policy COMMAND emu_unit_seed_policy_test)

add_executable(emu_unit_input_analog_test
  emu_unit_input_analog_test.c
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/plugin/emulate_game_controller_via_libretro.c"
)
target_include_directories(emu_unit_input_analog_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_features(emu_unit_input_analog_test PRIVATE c_std_99)
if(UNIX)
  target_link_libraries(emu_unit_input_analog_test PRIVATE m)
endif()
add_test(NAME emu.unit.input_analog COMMAND emu_unit_input_analog_test)

add_executable(emu_unit_vi_refresh_rate_test
  emu_unit_vi_refresh_rate_test.c
)
target_include_directories(emu_unit_vi_refresh_rate_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src"
)
target_compile_features(emu_unit_vi_refresh_rate_test PRIVATE c_std_99)
add_test(NAME emu.unit.vi_refresh_rate COMMAND emu_unit_vi_refresh_rate_test)

add_executable(emu_unit_cp0_exception_cause_test
  emu_unit_cp0_exception_cause_test.c
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/r4300/exception.c"
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/r4300/interrupt.c"
)
target_include_directories(emu_unit_cp0_exception_cause_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src"
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/api"
  "${CMAKE_SOURCE_DIR}/libretro"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_definitions(emu_unit_cp0_exception_cause_test PRIVATE M64P_CORE_PROTOTYPES)
target_compile_features(emu_unit_cp0_exception_cause_test PRIVATE c_std_99)
add_test(NAME emu.unit.cp0_exception_cause COMMAND emu_unit_cp0_exception_cause_test)

# The same sources configured as the ARM/AArch64 new_dynarec builds compile
# them, which takes interrupt.c's new_dynarec delivery branch.
add_executable(emu_unit_cp0_exception_cause_new_dynarec_test
  emu_unit_cp0_exception_cause_test.c
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/r4300/exception.c"
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/r4300/interrupt.c"
)
target_include_directories(emu_unit_cp0_exception_cause_new_dynarec_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src"
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/api"
  "${CMAKE_SOURCE_DIR}/libretro"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_definitions(emu_unit_cp0_exception_cause_new_dynarec_test PRIVATE
  M64P_CORE_PROTOTYPES NEW_DYNAREC=4)
target_compile_features(emu_unit_cp0_exception_cause_new_dynarec_test PRIVATE c_std_99)
add_test(NAME emu.unit.cp0_exception_cause_new_dynarec
  COMMAND emu_unit_cp0_exception_cause_new_dynarec_test)

add_executable(emu_unit_rdp_hires_compat_crc_override_test
  emu_unit_rdp_hires_compat_crc_override_test.cpp
)
target_include_directories(emu_unit_rdp_hires_compat_crc_override_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_rdp_hires_compat_crc_override_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_hires_compat_crc_override COMMAND emu_unit_rdp_hires_compat_crc_override_test)

add_executable(emu_unit_rdp_hires_summary_interval_test emu_unit_rdp_hires_summary_interval_test.cpp)
target_include_directories(emu_unit_rdp_hires_summary_interval_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_rdp_hires_summary_interval_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_hires_summary_interval COMMAND emu_unit_rdp_hires_summary_interval_test)

add_executable(emu_unit_hires_cache_path_test emu_unit_hires_cache_path_test.cpp)
target_include_directories(emu_unit_hires_cache_path_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_hires_cache_path_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_cache_path COMMAND emu_unit_hires_cache_path_test)

add_executable(emu_unit_texture_replacement_provider_test
  emu_unit_texture_replacement_provider_test.cpp
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/texture_replacement.cpp"
)
target_include_directories(emu_unit_texture_replacement_provider_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/util"
)
# Exercise the renderer's resident-image key without constructing a Vulkan device.
target_include_directories(emu_unit_texture_replacement_provider_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/volk"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan-headers/include"
)
target_compile_options(emu_unit_texture_replacement_provider_test PRIVATE -fno-access-control)
target_compile_features(emu_unit_texture_replacement_provider_test PRIVATE cxx_std_17)
target_link_libraries(emu_unit_texture_replacement_provider_test PRIVATE z)
add_test(NAME emu.unit.texture_replacement_provider COMMAND emu_unit_texture_replacement_provider_test)

add_executable(emu_unit_hires_registry_budget_test
  emu_unit_hires_registry_budget_test.cpp
)
target_include_directories(emu_unit_hires_registry_budget_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_hires_registry_budget_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_registry_budget COMMAND emu_unit_hires_registry_budget_test)

add_executable(emu_unit_hires_mip_filter_test
  emu_unit_hires_mip_filter_test.cpp
)
target_include_directories(emu_unit_hires_mip_filter_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_hires_mip_filter_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_mip_filter COMMAND emu_unit_hires_mip_filter_test)

add_executable(emu_unit_hires_jabo_key_test
  emu_unit_hires_jabo_key_test.cpp
)
target_include_directories(emu_unit_hires_jabo_key_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_hires_jabo_key_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_jabo_key COMMAND emu_unit_hires_jabo_key_test)

add_executable(emu_unit_hires_addressing_test
  emu_unit_hires_addressing_test.cpp
)
target_include_directories(emu_unit_hires_addressing_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_hires_addressing_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_addressing COMMAND emu_unit_hires_addressing_test)

add_executable(emu_unit_hires_cutout_policy_test
  emu_unit_hires_cutout_policy_test.cpp
)
target_include_directories(emu_unit_hires_cutout_policy_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_hires_cutout_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_cutout_policy COMMAND emu_unit_hires_cutout_policy_test)

add_executable(emu_unit_hires_pass_policy_test
  emu_unit_hires_pass_policy_test.cpp
)
target_include_directories(emu_unit_hires_pass_policy_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_hires_pass_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_pass_policy COMMAND emu_unit_hires_pass_policy_test)

add_executable(emu_unit_hires_ci_palette_policy_test
  emu_unit_hires_ci_palette_policy_test.cpp
)
target_include_directories(emu_unit_hires_ci_palette_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_hires_ci_palette_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_ci_palette_policy COMMAND emu_unit_hires_ci_palette_policy_test)

add_executable(emu_unit_hires_ci_binding_test emu_unit_hires_ci_binding_test.cpp)
target_include_directories(emu_unit_hires_ci_binding_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_hires_ci_binding_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_ci_binding COMMAND emu_unit_hires_ci_binding_test)

add_executable(emu_unit_hires_compat_view_test emu_unit_hires_compat_view_test.cpp)
target_include_directories(emu_unit_hires_compat_view_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_hires_compat_view_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_compat_view COMMAND emu_unit_hires_compat_view_test)

add_executable(emu_unit_rdp_command_ingest_test
  emu_unit_rdp_command_ingest_test.cpp
)
target_include_directories(emu_unit_rdp_command_ingest_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/api"
)
target_compile_features(emu_unit_rdp_command_ingest_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_command_ingest COMMAND emu_unit_rdp_command_ingest_test)

add_executable(emu_unit_rdp_frame_mapping_test
  emu_unit_rdp_frame_mapping_test.cpp
  "${EMU_BEHAVIOR_SUPPORT_DIR}/rdp_memory_fixture.cpp"
)
target_include_directories(emu_unit_rdp_frame_mapping_test PRIVATE
  "${EMU_BEHAVIOR_SUPPORT_DIR}"
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/api"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/util"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/volk"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan-headers/include"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_definitions(emu_unit_rdp_frame_mapping_test PRIVATE GRANITE_VULKAN_MT)
target_compile_features(emu_unit_rdp_frame_mapping_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_frame_mapping COMMAND emu_unit_rdp_frame_mapping_test)

add_library(emu_obj_rdp_renderer_compile OBJECT EXCLUDE_FROM_ALL
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_renderer.cpp"
)
target_include_directories(emu_obj_rdp_renderer_compile PRIVATE
  "${EMU_BEHAVIOR_SUPPORT_DIR}"
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/api"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/util"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/volk"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan-headers/include"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_definitions(emu_obj_rdp_renderer_compile PRIVATE GRANITE_VULKAN_MT)
target_compile_features(emu_obj_rdp_renderer_compile PRIVATE cxx_std_17)
add_custom_target(emu_compile_rdp_renderer DEPENDS emu_obj_rdp_renderer_compile)

add_executable(emu_unit_rdp_frame_fallback_policy_test
  emu_unit_rdp_frame_fallback_policy_test.cpp
)
target_include_directories(emu_unit_rdp_frame_fallback_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
)
target_compile_features(emu_unit_rdp_frame_fallback_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_frame_fallback_policy COMMAND emu_unit_rdp_frame_fallback_policy_test)

add_executable(emu_unit_rdp_scanout_fallback_test
  emu_unit_rdp_scanout_fallback_test.cpp
)
target_include_directories(emu_unit_rdp_scanout_fallback_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/util"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/volk"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan-headers/include"
)
target_compile_definitions(emu_unit_rdp_scanout_fallback_test PRIVATE GRANITE_VULKAN_MT)
target_compile_features(emu_unit_rdp_scanout_fallback_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_scanout_fallback COMMAND emu_unit_rdp_scanout_fallback_test)

add_executable(emu_unit_rdp_init_policy_test
  emu_unit_rdp_init_policy_test.cpp
)
target_include_directories(emu_unit_rdp_init_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
)
target_compile_features(emu_unit_rdp_init_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_init_policy COMMAND emu_unit_rdp_init_policy_test)

add_executable(emu_unit_rdp_vulkan_glue_test
  emu_unit_rdp_vulkan_glue_test.cpp
)
target_include_directories(emu_unit_rdp_vulkan_glue_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/api"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan-headers/include"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_features(emu_unit_rdp_vulkan_glue_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_vulkan_glue COMMAND emu_unit_rdp_vulkan_glue_test)

add_executable(emu_unit_rdp_retro_image_mapping_test
  emu_unit_rdp_retro_image_mapping_test.cpp
)
target_include_directories(emu_unit_rdp_retro_image_mapping_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/api"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan-headers/include"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_features(emu_unit_rdp_retro_image_mapping_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_retro_image_mapping COMMAND emu_unit_rdp_retro_image_mapping_test)

add_executable(emu_unit_z64_constants_test
  emu_unit_z64_constants_test.cpp
)
target_include_directories(emu_unit_z64_constants_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_features(emu_unit_z64_constants_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.z64_constants COMMAND emu_unit_z64_constants_test)

add_executable(emu_unit_rdp_common_test
  emu_unit_rdp_common_test.cpp
)
target_include_directories(emu_unit_rdp_common_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_rdp_common_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_common COMMAND emu_unit_rdp_common_test)

add_executable(emu_unit_rdp_device_capability_policy_test
  emu_unit_rdp_device_capability_policy_test.cpp
)
target_include_directories(emu_unit_rdp_device_capability_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/vulkan-headers/include"
)
target_compile_features(emu_unit_rdp_device_capability_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_device_capability_policy COMMAND emu_unit_rdp_device_capability_policy_test)

add_executable(emu_unit_rdp_memory_path_policy_test
  emu_unit_rdp_memory_path_policy_test.cpp
)
target_include_directories(emu_unit_rdp_memory_path_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
)
target_compile_features(emu_unit_rdp_memory_path_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_memory_path_policy COMMAND emu_unit_rdp_memory_path_policy_test)

add_executable(emu_unit_rdp_scanout_coherency_policy_test
  emu_unit_rdp_scanout_coherency_policy_test.cpp
)
target_include_directories(emu_unit_rdp_scanout_coherency_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
)
target_compile_features(emu_unit_rdp_scanout_coherency_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_scanout_coherency_policy COMMAND emu_unit_rdp_scanout_coherency_policy_test)

add_executable(emu_unit_rdp_data_structures_test
  emu_unit_rdp_data_structures_test.cpp
)
target_include_directories(emu_unit_rdp_data_structures_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_rdp_data_structures_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_data_structures COMMAND emu_unit_rdp_data_structures_test)

add_executable(emu_unit_rdp_triangle_setup_policy_test
  emu_unit_rdp_triangle_setup_policy_test.cpp
)
target_include_directories(emu_unit_rdp_triangle_setup_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_rdp_triangle_setup_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_triangle_setup_policy COMMAND emu_unit_rdp_triangle_setup_policy_test)

add_executable(emu_unit_rdp_other_modes_policy_test
  emu_unit_rdp_other_modes_policy_test.cpp
)
target_include_directories(emu_unit_rdp_other_modes_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_rdp_other_modes_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_other_modes_policy COMMAND emu_unit_rdp_other_modes_policy_test)

add_executable(emu_unit_rdp_scissor_policy_test
  emu_unit_rdp_scissor_policy_test.cpp
)
target_include_directories(emu_unit_rdp_scissor_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_rdp_scissor_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_scissor_policy COMMAND emu_unit_rdp_scissor_policy_test)

add_executable(emu_unit_rdp_rect_setup_policy_test
  emu_unit_rdp_rect_setup_policy_test.cpp
)
target_include_directories(emu_unit_rdp_rect_setup_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_rdp_rect_setup_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_rect_setup_policy COMMAND emu_unit_rdp_rect_setup_policy_test)

add_executable(emu_unit_rdp_tex_rect_policy_test
  emu_unit_rdp_tex_rect_policy_test.cpp
)
target_include_directories(emu_unit_rdp_tex_rect_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_rdp_tex_rect_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.rdp_tex_rect_policy COMMAND emu_unit_rdp_tex_rect_policy_test)

add_executable(emu_unit_command_ring_policy_test
  emu_unit_command_ring_policy_test.cpp
)
target_include_directories(emu_unit_command_ring_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_command_ring_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.command_ring_policy COMMAND emu_unit_command_ring_policy_test)

add_executable(emu_unit_luts_hash_test
  emu_unit_luts_hash_test.cpp
)
target_include_directories(emu_unit_luts_hash_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_luts_hash_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.luts_hash COMMAND emu_unit_luts_hash_test)

add_executable(emu_unit_worker_thread_test
  emu_unit_worker_thread_test.cpp
)
target_include_directories(emu_unit_worker_thread_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_worker_thread_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.worker_thread COMMAND emu_unit_worker_thread_test)

add_executable(emu_conformance_vi_register_contract_test
  emu_conformance_vi_register_contract_test.cpp
)
target_include_directories(emu_conformance_vi_register_contract_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_conformance_vi_register_contract_test PRIVATE cxx_std_17)
add_test(NAME emu.conformance.vi_register_contract COMMAND emu_conformance_vi_register_contract_test)

add_executable(emu_conformance_vi_scanout_range_test
  emu_conformance_vi_scanout_range_test.cpp
)
target_include_directories(emu_conformance_vi_scanout_range_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_conformance_vi_scanout_range_test PRIVATE cxx_std_17)
add_test(NAME emu.conformance.vi_scanout_range COMMAND emu_conformance_vi_scanout_range_test)

add_executable(emu_conformance_vi_scaling_crop_test
  emu_conformance_vi_scaling_crop_test.cpp
)
target_include_directories(emu_conformance_vi_scaling_crop_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_conformance_vi_scaling_crop_test PRIVATE cxx_std_17)
add_test(NAME emu.conformance.vi_scaling_crop COMMAND emu_conformance_vi_scaling_crop_test)

add_executable(emu_unit_vi_register_interaction_matrix_test
  emu_unit_vi_register_interaction_matrix_test.cpp
)
target_include_directories(emu_unit_vi_register_interaction_matrix_test PRIVATE
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_vi_register_interaction_matrix_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.vi_register_interaction_matrix COMMAND emu_unit_vi_register_interaction_matrix_test)

add_executable(emu_unit_vi_scale_policy_test
  emu_unit_vi_scale_policy_test.cpp
)
target_include_directories(emu_unit_vi_scale_policy_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp"
)
target_compile_features(emu_unit_vi_scale_policy_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.vi_scale_policy COMMAND emu_unit_vi_scale_policy_test)

add_executable(emu_conformance_rdp_command_fields_test
  emu_conformance_rdp_command_fields_test.cpp
)
target_include_directories(emu_conformance_rdp_command_fields_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_features(emu_conformance_rdp_command_fields_test PRIVATE cxx_std_17)
add_test(NAME emu.conformance.rdp_command_fields COMMAND emu_conformance_rdp_command_fields_test)

add_executable(emu_conformance_rdp_command_lengths_test
  emu_conformance_rdp_command_lengths_test.cpp
)
target_include_directories(emu_conformance_rdp_command_lengths_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_features(emu_conformance_rdp_command_lengths_test PRIVATE cxx_std_17)
add_test(NAME emu.conformance.rdp_command_lengths COMMAND emu_conformance_rdp_command_lengths_test)

add_executable(emu_conformance_rdp_texture_load_sequence_test
  emu_conformance_rdp_texture_load_sequence_test.cpp
)
target_include_directories(emu_conformance_rdp_texture_load_sequence_test PRIVATE
  "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/libretro-common/include"
)
target_compile_features(emu_conformance_rdp_texture_load_sequence_test PRIVATE cxx_std_17)
add_test(NAME emu.conformance.rdp_texture_load_sequence COMMAND emu_conformance_rdp_texture_load_sequence_test)

add_test(
  NAME emu.conformance.runtime_smoke_lavapipe
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_conformance_runtime_smoke.sh"
)
set_tests_properties(emu.conformance.runtime_smoke_lavapipe PROPERTIES SKIP_RETURN_CODE 77)

add_test(
  NAME emu.conformance.paper_mario_full_cache_phrb_authorities
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_conformance_paper_mario_full_cache_phrb_authorities.sh"
)
set_tests_properties(emu.conformance.paper_mario_full_cache_phrb_authorities PROPERTIES SKIP_RETURN_CODE 77)

add_test(
  NAME emu.conformance.sm64_hires_boot
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_conformance_sm64_hires_boot.sh"
)
set_tests_properties(emu.conformance.sm64_hires_boot PROPERTIES SKIP_RETURN_CODE 77)

add_test(
  NAME emu.conformance.sm64_hires_title_fixture
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_conformance_sm64_hires_title_fixture.sh"
)
set_tests_properties(emu.conformance.sm64_hires_title_fixture PROPERTIES SKIP_RETURN_CODE 77)

add_test(
  NAME emu.conformance.oot_hires_boot
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_conformance_oot_hires_boot.sh"
)
set_tests_properties(emu.conformance.oot_hires_boot PROPERTIES SKIP_RETURN_CODE 77)

add_test(
  NAME emu.conformance.oot_hires_title_fixture
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_conformance_oot_hires_title_fixture.sh"
)
set_tests_properties(emu.conformance.oot_hires_title_fixture PROPERTIES SKIP_RETURN_CODE 77)

# Public conformance test ROMs (tools/conformance), one test per suite. Opt-in
# runtime lanes: they skip unless EMU_ENABLE_RUNTIME_CONFORMANCE=1 and the
# suite's ROMs are staged, and run every CPU engine serially.
foreach(suite krom dillon systemtest)
  add_test(
    NAME emu.conformance.test_roms_${suite}
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/support/emu_conformance_test_roms.sh" ${suite}
  )
  set_tests_properties(emu.conformance.test_roms_${suite} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 7200 RUN_SERIAL TRUE)
endforeach()

add_executable(emu_unit_hires_strong_hash_test emu_unit_hires_strong_hash_test.cpp)
target_include_directories(emu_unit_hires_strong_hash_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_hires_strong_hash_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.hires_strong_hash COMMAND emu_unit_hires_strong_hash_test)

add_executable(emu_unit_encoding_crc32_test emu_unit_encoding_crc32_test.c
  "${CMAKE_SOURCE_DIR}/libretro-common/encodings/encoding_crc32.c")
target_include_directories(emu_unit_encoding_crc32_test PRIVATE
  "${CMAKE_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/libretro-common/include")
target_link_libraries(emu_unit_encoding_crc32_test PRIVATE z)
add_test(NAME emu.unit.encoding_crc32 COMMAND emu_unit_encoding_crc32_test)

add_executable(emu_unit_compat_checksum_test emu_unit_compat_checksum_test.cpp)
target_include_directories(emu_unit_compat_checksum_test PRIVATE "${CMAKE_SOURCE_DIR}")
target_compile_features(emu_unit_compat_checksum_test PRIVATE cxx_std_17)
add_test(NAME emu.unit.compat_checksum COMMAND emu_unit_compat_checksum_test)

# Isolate the production TLBWrite from the other cached-interpreter instructions.
# GNU/LLVM ELF section GC lets this test link only the code it exercises.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_executable(emu_unit_tlb_code_hash_test
    emu_unit_tlb_code_hash_test.c
    "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/r4300/tlb.c"
  )
  target_include_directories(emu_unit_tlb_code_hash_test PRIVATE
    "${CMAKE_SOURCE_DIR}/mupen64plus-core/src"
    "${CMAKE_SOURCE_DIR}/mupen64plus-core/src/api"
    "${CMAKE_SOURCE_DIR}/libretro"
    "${CMAKE_SOURCE_DIR}/libretro-common/include"
  )
  target_compile_definitions(emu_unit_tlb_code_hash_test PRIVATE M64P_CORE_PROTOTYPES)
  target_compile_features(emu_unit_tlb_code_hash_test PRIVATE c_std_99)
  target_compile_options(emu_unit_tlb_code_hash_test PRIVATE -ffunction-sections -fdata-sections)
  target_link_options(emu_unit_tlb_code_hash_test PRIVATE -Wl,--gc-sections)
  add_test(NAME emu.unit.tlb_code_hash COMMAND emu_unit_tlb_code_hash_test)
endif()
