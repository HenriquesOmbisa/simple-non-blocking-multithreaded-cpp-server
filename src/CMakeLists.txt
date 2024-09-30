cmake_minimum_required(VERSION 3.15)

project(httpServer)

add_executable(httpServer)


# Verifica se a pasta 'static' existe no diretório de destino
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/static")
    message(STATUS "Removendo o diretório 'static' existente...")
    file(REMOVE_RECURSE "${CMAKE_CURRENT_BINARY_DIR}/static")
endif()

# Copia a pasta 'static' para o diretório de destino
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/static" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

target_sources(
    httpServer
    PRIVATE
    main.cc
    http/Server.cc
    http/Server.hpp
)