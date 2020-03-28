cmake_minimum_required(VERSION 3.2)

project(bulk_server VERSION 0.0.$ENV{TRAVIS_BUILD_NUMBER})

find_package(Threads REQUIRED)
find_package(Boost 1.58 COMPONENTS system REQUIRED)
find_package(Boost 1.58 COMPONENTS unit_test_framework REQUIRED)

configure_file(version.h.in ${PROJECT_SOURCE_DIR}/version.h)

add_executable(test_version tests/test_version.cpp src/Bulkmt.h)

add_executable(bulk_client bulk_client.cpp)
add_executable(bulk_server bulk_server.cpp)

file(GLOB_RECURSE TARGET_SRC "src/*.cpp" "src/*.h")
add_library(async SHARED ${TARGET_SRC})
add_library(patch_version lib.cpp)

set_target_properties(async PROPERTIES PUBLIC_HEADER src/async.h)

set_target_properties(bulk_client bulk_server async patch_version test_version PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        COMPILE_OPTIONS "-Wpedantic;-Wall;-Wextra"
        )

set_target_properties(test_version PROPERTIES
        COMPILE_DEFINITIONS BOOST_TEST_DYN_LINK
        INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
        )

set(COMMON_LIBS
        async
        patch_version
        Threads::Threads
        ${Boost_LIBRARIES}
        boost_system)

target_link_libraries(bulk_client ${COMMON_LIBS})
target_link_libraries(bulk_server ${COMMON_LIBS})

target_link_libraries(test_version
        ${Boost_LIBRARIES}
        patch_version
        )

target_include_directories(async PRIVATE src)

target_include_directories(bulk_client PUBLIC src)
target_include_directories(bulk_server PUBLIC src)
target_include_directories(test_version PUBLIC src)

install(TARGETS async
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION lib)

install(TARGETS bulk_client RUNTIME DESTINATION bin)
install(TARGETS bulk_server RUNTIME DESTINATION bin)


set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_CONTACT a.shatalov@rj-games.com)

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)

enable_testing()

add_test(${PROJECT_NAME}_tests test_version)

