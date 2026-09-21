cmake_minimum_required(VERSION 3.21)
project(trout VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# -Wno-missing-field-initializers: phlex's product_selector is designed for
# partial designated initialization; the warning would fire on every use.
add_compile_options(-Wall -Wextra -Wno-missing-field-initializers)

# Ensure plugin modules retain DT_NEEDED entries for all linked libraries,
# even if no symbols are directly referenced at link time (e.g. ROOT
# dictionary-only dependencies like SHiPDataModel's collection proxies).
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-as-needed")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-as-needed")

# Dependencies
find_package(SHiPGeometry CONFIG REQUIRED)
find_package(SHiPGeometryService 0.3 CONFIG REQUIRED)
find_package(SHiPDataModel REQUIRED)
find_package(SHiPFieldService REQUIRED)
find_package(Acts CONFIG REQUIRED)

# Phlex's cmake config does not call find_dependency for its transitive deps
find_package(Boost REQUIRED COMPONENTS json)
find_package(TBB REQUIRED)
find_package(spdlog REQUIRED)
find_package(phlex REQUIRED)
find_package(
    ROOT
    REQUIRED
    COMPONENTS Core RIO Hist MathCore ROOTNTuple ROOTHistUtil
)

# Random123 is header-only and ships no CMake config of its own.
find_path(
    Random123_INCLUDE_DIR
    NAMES Random123/philox.h
    DOC "Random123 header directory"
    REQUIRED
)

# Digitised (flat, record-per-hit) file reader plugin
add_library(read_digitised_hits MODULE src/io/read_digitised_hits.cpp)
target_link_libraries(
    read_digitised_hits
    PRIVATE phlex::module SHiP::SHiPDataModel ROOT::RIO ROOT::ROOTNTuple
)

# ACTS tracking-geometry provider plugin (builds Acts::TrackingGeometry for
# the straw-tube stations from the real SHiP geometry via SHiPGeometryService)
add_library(acts_geometry_provider MODULE src/acts_geometry_provider.cpp)
target_link_libraries(
    acts_geometry_provider
    PRIVATE phlex::module SHiPGeometryService::SHiPGeometryService Acts::Core
)
target_include_directories(
    acts_geometry_provider
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ACTS magnetic-field provider plugin (loads the spectrometer's covfie field
# map once and shares it as an Acts::MagneticFieldProvider product, the same
# "build once on layer job" pattern as acts_geometry_provider above)
add_library(acts_field_provider MODULE src/acts_field_provider.cpp)
target_link_libraries(
    acts_field_provider
    PRIVATE
        phlex::module
        SHiPFieldService::SHiPFieldService
        Acts::Core
        spdlog::spdlog
)
target_include_directories(
    acts_field_provider
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Passthrough plugin
add_library(passthrough MODULE src/passthrough.cpp)
target_link_libraries(passthrough PRIVATE phlex::module SHiP::SHiPDataModel)

# IP selection
add_library(ip_selector MODULE src/ip_selector.cpp)
target_link_libraries(ip_selector PRIVATE phlex::module SHiP::SHiPDataModel)

# Pattern recognition + track finding, split into phlex-native
# prepare/unfold/fit stages so per-seed fitting is parallelized by the
# framework. One file per pipeline stage — spectrometer_tracking.cpp just
# wires prepare_measurements/generate_seeds/fit_seed together; see its
# header comment for the stage breakdown.
add_library(
    spectrometer_tracking
    MODULE
    src/detectors/spectrometer/spectrometer_tracking.cpp
    src/detectors/spectrometer/prepare_measurements.cpp
    include/detectors/spectrometer/prepare_measurements.hpp
    src/detectors/spectrometer/generate_seeds.cpp
    include/detectors/spectrometer/generate_seeds.hpp
    src/detectors/spectrometer/fit_seed.cpp
    include/detectors/spectrometer/fit_seed.hpp
    include/PhlexModuleProxy.hpp
    include/detectors/spectrometer/SpillContext.hpp
    include/detectors/spectrometer/SpectrometerCkf.hpp
    include/detectors/spectrometer/SpectrometerMeasurements.hpp
    include/IndexSourceLink.hpp
    include/GeometryContainers.hpp
    include/acts_geometry_provider.hpp
    include/acts_field_provider.hpp
    src/detectors/spectrometer/HitUncertainty.cpp
    include/detectors/spectrometer/HitUncertainty.hpp
)
target_link_libraries(
    spectrometer_tracking
    PRIVATE
        phlex::module
        SHiP::SHiPDataModel
        Acts::Core
        SHiPFieldService::SHiPFieldService
        SHiPFieldService::G4Adapter
        SHiPGeometryService::SHiPGeometryService
        spdlog::spdlog
)
target_include_directories(
    spectrometer_tracking
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Placeholder per-subdetector reconstruction plugins, all instantiating the
# shared identity_reco.hpp template with their hit type.
foreach(
    detector
    IN
    ITEMS upstream_tagger surround_tagger calorimeter timing_detector
)
    add_library(
        ${detector}_reco
        MODULE
        src/detectors/${detector}/${detector}_reco.cpp
        include/detectors/identity_reco.hpp
    )
    target_link_libraries(
        ${detector}_reco
        PRIVATE phlex::module SHiP::SHiPDataModel
    )
    target_include_directories(
        ${detector}_reco
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
endforeach()

# Reconstruction output module plugin
add_library(
    rec_output_module
    MODULE
    src/io/rec_output_module.cpp
    include/HistoFileService.hpp
    include/detectors/multiplicity_histogrammer.hpp
    include/detectors/calorimeter/calorimeter_histogrammer.hpp
    include/detectors/spectrometer/SpectrometerHistogrammer.hpp
    include/detectors/surround_tagger/surround_tagger_histogrammer.hpp
    include/detectors/timing_detector/timing_detector_histogrammer.hpp
    include/detectors/upstream_tagger/upstream_tagger_histogrammer.hpp
    include/io/typed_rntuple_writer.hpp
)
target_include_directories(
    rec_output_module
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/io"
)
target_link_libraries(
    rec_output_module
    PRIVATE
        phlex::module
        SHiP::SHiPDataModel
        ROOT::Hist
        ROOT::RIO
        ROOT::ROOTHistUtil
        ROOT::ROOTNTuple
        Boost::headers
        spdlog::spdlog
)

# Smoke-test input generator (build-only, not installed)
add_executable(make_test_input tools/make_test_input.cpp)
target_include_directories(
    make_test_input
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(
    make_test_input
    SYSTEM
    PRIVATE "${Random123_INCLUDE_DIR}"
)
target_link_libraries(
    make_test_input
    PRIVATE SHiP::SHiPDataModel ROOT::RIO ROOT::ROOTNTuple TBB::tbb
)

# Install
include(GNUInstallDirs)

set(PLUGIN_TARGETS
    read_digitised_hits
    acts_geometry_provider
    acts_field_provider
    passthrough
    ip_selector
    spectrometer_tracking
    upstream_tagger_reco
    surround_tagger_reco
    calorimeter_reco
    timing_detector_reco
    rec_output_module
)

install(TARGETS ${PLUGIN_TARGETS} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY workflows/ DESTINATION share/trout/workflows)
