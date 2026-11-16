cmake_minimum_required(VERSION 3.16)
project(zero_day_market CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(JC_DB_CONFIG_PATH "/etc/jesterschest/db.conf"
    CACHE STRING "Path of the database configuration file read by the CGI")

# Hardening flags (see the security architecture document).
add_compile_options(-Wall -Wextra -Wpedantic -Werror -fstack-protector-strong -fPIE)
add_link_options(-pie -Wl,-z,relro,-z,now)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # Sanitizers catch memory errors while testing. _FORTIFY_SOURCE needs
  # optimization, so it is only enabled outside Debug.
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
else()
  # Undefine first: some compilers already define it with another level.
  add_compile_options(-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED IMPORTED_TARGET libmariadb)

# --- Domain: entities and business rules, no external dependencies ----------
add_library(domain STATIC
  src/domain/users/user.cpp
  src/domain/objects/object.cpp
  src/domain/locals/local.cpp
)
target_include_directories(domain PUBLIC src)

# --- Application: use cases and the ports they need -------------------------
add_library(application STATIC
  src/application/catalog/list_catalog.cpp
)
target_link_libraries(application PUBLIC domain)

# --- Infrastructure: MariaDB, configuration, (later) mail, files, crypto -----
add_library(infrastructure STATIC
  src/infrastructure/db/db_config.cpp
  src/infrastructure/db/mariadb_connection.cpp
  src/infrastructure/db/mariadb_catalog_repository.cpp
)
target_link_libraries(infrastructure PUBLIC application PkgConfig::MARIADB)

# --- HTML templates, embedded into the binary at build time ------------------
file(GLOB TEMPLATE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/templates/*.html")
set(EMBEDDED_TEMPLATES "${CMAKE_BINARY_DIR}/generated/template_sources.cpp")
add_custom_command(
  OUTPUT "${EMBEDDED_TEMPLATES}"
  COMMAND "${CMAKE_COMMAND}"
          -DTEMPLATE_DIR=${CMAKE_SOURCE_DIR}/templates
          -DOUTPUT=${EMBEDDED_TEMPLATES}
          -P "${CMAKE_SOURCE_DIR}/cmake/embed_templates.cmake"
  DEPENDS ${TEMPLATE_FILES} "${CMAKE_SOURCE_DIR}/cmake/embed_templates.cmake"
  COMMENT "Embedding HTML templates"
)

# --- Presentation: CGI request/response, routing and HTML -------------------
add_library(presentation STATIC
  src/presentation/templates/template_engine.cpp
  "${EMBEDDED_TEMPLATES}"
  src/presentation/cgi/cgi_request.cpp
  src/presentation/cgi/cgi_response.cpp
  src/presentation/pages/html.cpp
  src/presentation/pages/layout.cpp
  src/presentation/pages/catalog_page.cpp
  src/presentation/router.cpp
)
target_link_libraries(presentation PUBLIC application)

# --- CGI executable: composition root ---------------------------------------
add_executable(webapp src/main.cpp)
set_target_properties(webapp PROPERTIES OUTPUT_NAME "app.cgi")
target_compile_definitions(webapp PRIVATE JC_DB_CONFIG_PATH="${JC_DB_CONFIG_PATH}")
target_link_libraries(webapp PRIVATE presentation infrastructure)

# --- Tests (plain CTest, no third-party framework) --------------------------
enable_testing()
add_executable(unit_tests tests/unit_tests.cpp)
target_link_libraries(unit_tests PRIVATE presentation application)
add_test(NAME unit_tests COMMAND unit_tests)
