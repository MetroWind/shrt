# cmake -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE=include-what-you-use -B build . && cmake --build build -j
cmake_minimum_required(VERSION 3.24)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(Shrt)
option(SHRT_BUILD_TESTS "Build unit tests" OFF)

include(FetchContent)
FetchContent_Declare(
  libmw
  GIT_REPOSITORY https://github.com/MetroWind/libmw.git
)

FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG v3.1.1
)

FetchContent_Declare(
  ryml
  GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
  GIT_TAG
  GIT_SHALLOW FALSE  # ensure submodules are checked out
)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.12.0
)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_Declare(
  inja
  GIT_REPOSITORY https://github.com/pantor/inja.git
  GIT_TAG main
)

set(SPDLOG_USE_STD_FORMAT ON)
set(LIBMW_BUILD_URL ON)
set(LIBMW_BUILD_SQLITE ON)
set(LIBMW_BUILD_HTTP_SERVER ON)
set(LIBMW_BUILD_CRYPTO ON)
set(INJA_USE_EMBEDDED_JSON FALSE)
set(INJA_BUILD_TESTS FALSE)
FetchContent_MakeAvailable(libmw ryml spdlog cxxopts json inja)

if(SHRT_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
  )
  FetchContent_MakeAvailable(googletest)
endif()

set(SOURCE_FILES
  src/app.cpp
  src/app.hpp
  src/config.cpp
  src/config.hpp
  src/data.cpp
  src/data.hpp
)

set(LIBS
  cxxopts
  mw::mw
  mw::url
  mw::sqlite
  mw::http-server
  mw::crypto
  ryml::ryml
  spdlog::spdlog
)

set(INCLUDES
  ${libmw_SOURCE_DIR}/includes
  ${json_SOURCE_DIR}/single_include
  ${inja_SOURCE_DIR}/single_include/inja
)

add_executable(shrt ${SOURCE_FILES} src/main.cpp)
set_property(TARGET shrt PROPERTY CXX_STANDARD 23)
set_property(TARGET shrt PROPERTY CXX_EXTENSIONS FALSE)

set_property(TARGET shrt PROPERTY COMPILE_WARNING_AS_ERROR TRUE)
target_compile_options(shrt PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(shrt PRIVATE ${INCLUDES})
target_link_libraries(shrt PRIVATE ${LIBS})

if(SHRT_BUILD_TESTS)
  set(TEST_FILES
    src/data_mock.hpp
    src/data_test.cpp
    src/app_test.cpp
  )

  # ctest --test-dir build
  add_executable(shrt_test ${SOURCE_FILES} ${TEST_FILES})
  set_property(TARGET shrt_test PROPERTY CXX_STANDARD 23)
  set_property(TARGET shrt_test PROPERTY COMPILE_WARNING_AS_ERROR TRUE)
  target_compile_options(shrt_test PRIVATE -Wall -Wextra -Wpedantic)
  target_include_directories(shrt_test PRIVATE
    ${INCLUDES}
    ${googletest_SOURCE_DIR}/googletest/include
    ${googletest_SOURCE_DIR}/googlemock/include
  )

  target_link_libraries(shrt_test PRIVATE
    ${LIBS}
    GTest::gtest_main
    GTest::gmock_main
  )

  enable_testing()
  include(GoogleTest)
  gtest_discover_tests(shrt_test
    # Need this so that the unit tests can find the templates.
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
