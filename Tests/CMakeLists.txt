add_executable(cosmic_tests oscillator_tests.cpp)
target_link_libraries(cosmic_tests PRIVATE cosmic_dsp)
target_include_directories(cosmic_tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)

add_executable(quad_tests quadrature_tests.cpp)
target_link_libraries(quad_tests PRIVATE cosmic_dsp)
target_include_directories(quad_tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)

add_executable(pll_tests pll_tests.cpp)
target_link_libraries(pll_tests PRIVATE cosmic_dsp)
target_include_directories(pll_tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)

add_executable(tuning_tests tuning_tests.cpp)
target_link_libraries(tuning_tests PRIVATE cosmic_dsp)
target_include_directories(tuning_tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)

add_executable(drift_tests drift_tests.cpp)
target_link_libraries(drift_tests PRIVATE cosmic_dsp)
target_include_directories(drift_tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)

add_executable(divider_tests divider_tests.cpp)
target_link_libraries(divider_tests PRIVATE cosmic_dsp)
target_include_directories(divider_tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)

add_executable(spectral_tests spectral_tests.cpp)
target_link_libraries(spectral_tests PRIVATE cosmic_dsp)

add_test(NAME Core COMMAND cosmic_tests)
add_test(NAME Quad COMMAND quad_tests)
add_test(NAME PLL COMMAND pll_tests)
add_test(NAME Tuning COMMAND tuning_tests)
add_test(NAME Drift COMMAND drift_tests)
add_test(NAME Divider COMMAND divider_tests)
add_test(NAME Spectral COMMAND spectral_tests)

add_executable(carrierfield_tests carrierfield_tests.cpp)
target_link_libraries(carrierfield_tests PRIVATE cosmic_dsp)
target_include_directories(carrierfield_tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME CarrierField COMMAND carrierfield_tests)

add_executable(render_drone ${CMAKE_SOURCE_DIR}/Tools/render_drone.cpp)
target_link_libraries(render_drone PRIVATE cosmic_dsp)
target_include_directories(render_drone PRIVATE ${CMAKE_SOURCE_DIR}/Source)

add_executable(carrierfield_plugin_tests carrierfield_plugin_tests.cpp)
target_link_libraries(carrierfield_plugin_tests PRIVATE cosmic_dsp)
target_include_directories(carrierfield_plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME CarrierFieldPlugin COMMAND carrierfield_plugin_tests)

add_executable(carrierfield_vst3_bridge_tests carrierfield_vst3_bridge_tests.cpp)
target_include_directories(carrierfield_vst3_bridge_tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)
add_test(NAME CarrierFieldVst3Bridge COMMAND carrierfield_vst3_bridge_tests)
