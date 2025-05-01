cmake_minimum_required(VERSION 3.14)
project(file_server)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type default if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# Output directories for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# Enable folders in IDEs like VS or Xcode
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Find Boost
find_package(Boost REQUIRED COMPONENTS system thread)

# Add executable
add_executable(file_server
    main.cpp
    server.cpp
)

# Link Boost
target_link_libraries(file_server PRIVATE Boost::system Boost::thread)

# Set compile options
target_compile_options(file_server PRIVATE
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
)

# copy video files to bin folder
add_custom_command(
        TARGET  ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/video/*.mp4
                ${CMAKE_BINARY_DIR}/bin/)
