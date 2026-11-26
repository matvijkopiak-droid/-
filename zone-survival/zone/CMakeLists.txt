cmake_minimum_required(VERSION 3.14)
project(ZoneSurvival CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(EMSCRIPTEN)
  set(PLATFORM Web CACHE STRING "" FORCE)
endif()

# --- raylib: спершу шукаємо встановлену, інакше завантажуємо 5.0 ---
if(NOT EMSCRIPTEN)
  find_package(raylib 5.0 QUIET)
endif()
if(EMSCRIPTEN OR NOT raylib_FOUND)
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.0 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(raylib)
endif()

# --- ядро гри (без raylib, тестується окремо) ---
add_library(zone_core STATIC src/world.cpp src/items.cpp src/game.cpp src/zombie.cpp src/colony.cpp src/save.cpp)
target_include_directories(zone_core PUBLIC src)
if(MSVC)
  target_compile_options(zone_core PRIVATE /utf-8)
else()
  target_compile_options(zone_core PRIVATE -Wall -Wextra)
endif()

# --- гра ---
add_executable(zone src/main.cpp src/render.cpp src/ui.cpp src/audio.cpp)
target_link_libraries(zone PRIVATE zone_core raylib)
if(MSVC)
  target_compile_options(zone PRIVATE /utf-8)
endif()

if(EMSCRIPTEN)
  # браузерна збірка: zone.html + zone.js + zone.wasm + zone.data (шрифт)
  set_target_properties(zone PROPERTIES SUFFIX ".html")
  target_link_options(zone PRIVATE -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=1048576
    -sEXPORTED_RUNTIME_METHODS=FS,IDBFS -lidbfs.js --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets)
else()
  # шрифт із кирилицею поруч з виконуваним файлом
  add_custom_command(TARGET zone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:zone>/assets)
endif()

# --- тести без вікна: cmake -DZONE_TESTS=ON ---
option(ZONE_TESTS "Збирати тести симуляції" OFF)
if(ZONE_TESTS)
  add_executable(test_world tools/test_world.cpp)
  target_link_libraries(test_world PRIVATE zone_core)
  add_executable(test_sim tools/test_sim.cpp)
  target_link_libraries(test_sim PRIVATE zone_core)
  add_executable(test_sim2 tools/test_sim2.cpp)
  target_link_libraries(test_sim2 PRIVATE zone_core)
endif()
