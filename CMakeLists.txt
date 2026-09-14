cmake_minimum_required(VERSION 3.20)
project(PeriodTracker VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # lets VS Code's C++ extension find headers

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

# Add new .cpp files to this list, then re-run the cmake configure step.
add_executable(app
    src/main.cpp
    src/date.cpp
    src/profile.cpp
    src/tracker.cpp
    src/onboarding.cpp
    src/ui.cpp
)

target_link_libraries(app PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    SDL2_ttf::SDL2_ttf
)

if(MSVC)
    target_compile_options(app PRIVATE /W4)
else()
    target_compile_options(app PRIVATE -Wall -Wextra -Wpedantic)
endif()
