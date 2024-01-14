# CMakeList.txt : CMake project for Equivalence, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (equivalence WIN32 "Main.cpp" "App.cpp" "App.h" "FontRenderer.cpp" "FontRenderer.h" "font/JetBrainsMono-Regular.c" "EqualityTable.cpp" "EqualityTable.h" "Entry.cpp" "Entry.h" "Crc32.cpp" "Crc32.h" )
target_compile_definitions(equivalence PRIVATE "DEBUG=$<IF:$<CONFIG:Debug>,1,0>")

set_property(TARGET equivalence PROPERTY CXX_STANDARD 20)

# TODO: Add tests and install targets if needed.

target_include_directories(equivalence PRIVATE
    "."
    "${PROJECT_SOURCE_DIR}/SDL/include"
    "${PROJECT_SOURCE_DIR}/SDL_ttf"
    "${PROJECT_SOURCE_DIR}/SDL_net"
    )

target_link_libraries(equivalence SDL2-static SDL2main SDL2_ttf SDL2_net::SDL2_net-static)