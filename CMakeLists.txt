cmake_minimum_required(VERSION 3.16)
project(mercado CXX)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Wextra -Werror -fstack-protector-strong
                    -D_FORTIFY_SOURCE=2 -fPIE)
add_link_options(-pie -Wl,-z,relro,-z,now)

add_library(domain STATIC ...)
add_library(application STATIC ...)
target_link_libraries(application PUBLIC domain)

add_library(infrastructure STATIC ...)
target_link_libraries(infrastructure PUBLIC application pq ssl crypto magic)

add_executable(app.cgi src/main.cpp ...)
target_link_libraries(app.cgi PRIVATE infrastructure)