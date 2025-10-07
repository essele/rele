# cross-compile for ARM hard-float
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR arm)

# specify the cross compiler
SET(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# optionally, sysroot if needed
# SET(CMAKE_SYSROOT /path/to/arm/sysroot)

# flags
SET(CMAKE_C_FLAGS   "-O2 -mfloat-abi=hard")
SET(CMAKE_CXX_FLAGS "-O2 -std=c++17 -mfloat-abi=hard")

# where to install libraries
#SET(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/install)

