cmake_minimum_required(VERSION 3.15)

include(cmake/HunterGate.cmake)

HunterGate(
        URL "https://github.com/cpp-pm/hunter/archive/v0.23.268.tar.gz"
        SHA1 "40ae51ce014380289bad5ec6b6e207660f69e804"
)

project(Chatter)

hunter_add_package(Boost COMPONENTS system)
hunter_add_package(OpenSSL)

find_package(Boost CONFIG COMPONENTS system)
find_package(OpenSSL)
find_package(Threads)

file(GLOB_RECURSE src_files CONFIGURE_DEPENDS "src/*.cpp" "src/*.hpp")
add_executable(chatter ${src_files})
target_include_directories(chatter PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(chatter PRIVATE Boost::system OpenSSL::Crypto OpenSSL::SSL Threads::Threads)
target_compile_definitions(chatter PRIVATE BOOST_ASIO_DISABLE_CONCEPTS)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(chatter PRIVATE -Werror -Wall -Wextra -pedantic)
endif()