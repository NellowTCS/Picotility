cmake_minimum_required(VERSION 3.20)

# Include ESP-IDF and Tactility SDK
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(TACTILITY_SDK_PATH $ENV{TACTILITY_SDK_PATH})
include("${TACTILITY_SDK_PATH}/TactilitySDK.cmake")
set(EXTRA_COMPONENT_DIRS ${TACTILITY_SDK_PATH})

# Project definition
project(Picotility)
tactility_project(Picotility)
