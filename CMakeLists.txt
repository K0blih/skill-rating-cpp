cmake_minimum_required(VERSION 3.16)

project(skill_rating_cpp VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(SKILL_RATING_BUILD_TESTS "Build Bayesian skill rating tests" ON)
option(SKILL_RATING_BUILD_CLI "Build Bayesian skill rating CLI" ON)
option(SKILL_RATING_BUILD_HTTP "Build Bayesian skill rating HTTP service" ON)

add_library(
    skill_rating
    src/detail/factor_graph.cpp
    src/detail/math.cpp
    src/skill_rating.cpp
)

add_library(skill_rating::skill_rating ALIAS skill_rating)
set_target_properties(
    skill_rating
    PROPERTIES
        EXPORT_NAME skill_rating
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_compile_features(skill_rating PUBLIC cxx_std_20)
target_include_directories(
    skill_rating
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(SKILL_RATING_BUILD_CLI OR SKILL_RATING_BUILD_HTTP)
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    set(JSON_Install OFF CACHE INTERNAL "")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
    )
    FetchContent_MakeAvailable(nlohmann_json)

    add_library(
        skill_rating_app
        src/app/commands.cpp
        src/app/json.cpp
    )
    target_compile_features(skill_rating_app PUBLIC cxx_std_20)
    target_include_directories(skill_rating_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(skill_rating_app PUBLIC skill_rating::skill_rating nlohmann_json::nlohmann_json)
endif()

install(
    TARGETS skill_rating
    EXPORT skill_rating_cppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/skill_rating_cppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/skill_rating_cppConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/skill_rating_cppConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/skill_rating_cpp
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/skill_rating_cppConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/skill_rating_cppConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/skill_rating_cpp
)

install(
    EXPORT skill_rating_cppTargets
    NAMESPACE skill_rating::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/skill_rating_cpp
)

if(SKILL_RATING_BUILD_CLI)
    add_executable(skill_rating_cli src/cli/main.cpp)
    target_include_directories(skill_rating_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(skill_rating_cli PRIVATE skill_rating_app)
endif()

if(SKILL_RATING_BUILD_HTTP)
    set(HTTPLIB_INSTALL OFF CACHE INTERNAL "")
    FetchContent_Declare(
        cpp_httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.26.0
    )
    FetchContent_MakeAvailable(cpp_httplib)

    add_executable(skill_rating_http src/http/main.cpp)
    target_include_directories(skill_rating_http PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(skill_rating_http PRIVATE skill_rating_app httplib::httplib)
endif()

if(SKILL_RATING_BUILD_TESTS)
    enable_testing()
    add_executable(skill_rating_tests tests/skill_rating_tests.cpp)
    target_link_libraries(skill_rating_tests PRIVATE skill_rating::skill_rating)
    add_test(NAME skill_rating_tests COMMAND skill_rating_tests)
    add_test(
        NAME skill_rating_package_install_smoke
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/package_install_smoke.sh ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
    )
    if(SKILL_RATING_BUILD_CLI)
        add_test(
            NAME skill_rating_cli_smoke
            COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_smoke.sh $<TARGET_FILE:skill_rating_cli>
        )
    endif()
    if(SKILL_RATING_BUILD_HTTP)
        add_test(
            NAME skill_rating_http_smoke
            COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/http_smoke.sh $<TARGET_FILE:skill_rating_http> 18080
        )
    endif()
endif()
