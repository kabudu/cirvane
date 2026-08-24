cmake_minimum_required(VERSION 3.16)

# Reuse the same vendored ESP-IDF console command component as v2. The v1
# application source and sdkconfig defaults below are exact recoveries from
# the Hermes request archive that produced the recorded 0xe0100-byte image.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../nucleus/components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nucleus)
