# cmake -B build . && cmake --build build -j
cmake_minimum_required(VERSION 3.24)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(DoLServer)

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
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.18.0
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

set(INJA_USE_EMBEDDED_JSON FALSE)
set(INJA_BUILD_TESTS FALSE)
set(SPDLOG_USE_STD_FORMAT ON)
FetchContent_MakeAvailable(libmw ryml spdlog cxxopts httplib json inja)

add_custom_command(
  COMMAND bash make-static.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS make-static.sh "main.html"
  OUTPUT src/statics.hpp
  COMMENT "Generating code for HTML embed"
)

set(SOURCE_FILES
  src/app.cpp
  src/app.hpp
  src/config.cpp
  src/config.hpp
  src/data.cpp
  src/data.hpp
  src/main.cpp
  src/statics.hpp
)

set(LIBS
  cxxopts
  httplib
  mw
  ryml::ryml
  spdlog::spdlog
)

set(INCLUDES
  ${libmw_SOURCE_DIR}/includes
  ${json_SOURCE_DIR}/single_include
  ${inja_SOURCE_DIR}/single_include/inja
)

add_executable(dol-server ${SOURCE_FILES} src/main.cpp)
set_property(TARGET dol-server PROPERTY CXX_STANDARD 23)

set_property(TARGET dol-server PROPERTY COMPILE_WARNING_AS_ERROR TRUE)
target_compile_options(dol-server PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(dol-server PRIVATE ${INCLUDES})
target_link_libraries(dol-server PRIVATE ${LIBS})
