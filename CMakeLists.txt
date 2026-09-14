cmake_minimum_required(VERSION 3.20)
project(PeriodTracker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # lets VS Code's C++ extension find headers

set(APP_NAME "Period Tracker")

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

# Add new .cpp files to this list, then re-run the cmake configure step.
add_executable(app MACOSX_BUNDLE
    src/main.cpp
    src/date.cpp
    src/profile.cpp
    src/symptoms.cpp
    src/tracker.cpp
    src/dayeditor.cpp
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

# ---- macOS app bundle -------------------------------------------------------
# Builds "Period Tracker.app" rather than a bare executable, so it can be opened
# from Finder, Spotlight and the Dock like any other app.

set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/AppIcon.icns")

set_target_properties(app PROPERTIES
    OUTPUT_NAME "${APP_NAME}"
    MACOSX_BUNDLE_INFO_PLIST          "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME         "${APP_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER      "com.nicole-li7.periodtracker"
    MACOSX_BUNDLE_ICON_FILE           "AppIcon"
    MACOSX_BUNDLE_BUNDLE_VERSION      "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"

    # Where the executable looks for the libraries copied in below.
    INSTALL_RPATH "@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Assemble the bundle. Order matters: the icon has to be in place before the
# libraries are bundled, because that step signs the app last and any file added
# afterwards invalidates the signature. This is why the icon is copied here
# rather than with MACOSX_PACKAGE_LOCATION, which CMake runs after POST_BUILD.
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_CONTENT_DIR:app>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${APP_ICON}" "$<TARGET_BUNDLE_CONTENT_DIR:app>/Resources/AppIcon.icns"
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/bundle_libs.sh"
            "$<TARGET_BUNDLE_DIR:app>" "${APP_NAME}"
    COMMENT "Assembling ${APP_NAME}.app"
    VERBATIM)

# `cmake --install build` puts the finished app in /Applications.
install(TARGETS app BUNDLE DESTINATION "/Applications")
