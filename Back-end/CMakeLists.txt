cmake_minimum_required(VERSION 3.20)
project(cplusplusproject2022fall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch fmt
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        12.2.0
    GIT_SHALLOW    TRUE
)

# Fetch glaze
FetchContent_Declare(
    glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG        v9.0.0
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(fmt glaze)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(.)
include_directories(./usr/local/include)

find_package(OpenMP REQUIRED)

add_executable(cplusplusproject2022fall server_unencrypted.cpp)
target_link_libraries(cplusplusproject2022fall PRIVATE
    fmt::fmt-header-only
    glaze::glaze
    sqlite3
    ssl
    crypto
    OpenMP::OpenMP_CXX
)

add_executable(client_unencrypted client_unencrypted.cpp)
target_link_libraries(client_unencrypted PRIVATE
    fmt::fmt-header-only
    glaze::glaze
)

add_executable(server_ssl server.cpp)
target_link_libraries(server_ssl PRIVATE
    fmt::fmt-header-only
    glaze::glaze
    sqlite3
    ssl
    crypto
    OpenMP::OpenMP_CXX
)

add_executable(client_ssl client.cpp)
target_link_libraries(client_ssl PRIVATE
    fmt::fmt-header-only
    glaze::glaze
    ssl
    crypto
)