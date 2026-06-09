cmake_minimum_required(VERSION 3.16)
project(ProtonSage VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql Widgets Network)
find_package(ZLIB REQUIRED)

# ── Core library (all non-UI logic) ────────────────────────────────────

set(CORE_SOURCES
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
)

set(CORE_HEADERS
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
)

add_library(protonsage_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_include_directories(protonsage_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(protonsage_core PUBLIC Qt6::Core Qt6::Sql Qt6::Network ${ZLIB_LIBRARIES})

# ── Main GUI application ────────────────────────────────────────────────

add_executable(protonsage
    src/main.cpp
    src/ui/mainwindow.cpp src/ui/mainwindow.h
    src/ui/imagecache.cpp src/ui/imagecache.h
)
qt_add_resources(protonsage schema FILES src/storage/schema.sql)
target_link_libraries(protonsage PRIVATE protonsage_core Qt6::Widgets)

# ── Smoke test ──────────────────────────────────────────────────────────

add_executable(smoke_test src/smoke_test.cpp)
target_include_directories(smoke_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(smoke_test PRIVATE protonsage_core)

install(TARGETS protonsage RUNTIME DESTINATION bin)
