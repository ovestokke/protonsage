cmake_minimum_required(VERSION 3.16)
project(ProtonSage VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql Widgets Network)
find_package(ZLIB REQUIRED)

set(SOURCES
    src/main.cpp
    src/core/models.cpp
    src/storage/database.cpp
    src/protondb/import.cpp
    src/protondb/snapshots.cpp
    src/steam/library.cpp
    src/steam/vdf.cpp
    src/steam/paths.cpp
    src/system/profile.cpp
    src/advisor/scoring.cpp
    src/advisor/helpers.cpp
    src/advisor/engine.cpp
    src/ui/mainwindow.cpp
)

set(HEADERS
    src/core/models.h
    src/storage/database.h
    src/protondb/import.h
    src/protondb/snapshots.h
    src/steam/library.h
    src/steam/vdf.h
    src/steam/paths.h
    src/system/profile.h
    src/advisor/advisor.h
    src/advisor/helpers.h
    src/ui/mainwindow.h
)

add_executable(protonsage ${SOURCES} ${HEADERS})

target_include_directories(protonsage PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(protonsage PRIVATE Qt6::Core Qt6::Sql Qt6::Widgets Qt6::Network ${ZLIB_LIBRARIES})

# Embed schema.sql as a Qt resource
qt_add_resources(protonsage schema
    FILES src/storage/schema.sql
)

install(TARGETS protonsage RUNTIME DESTINATION bin)
