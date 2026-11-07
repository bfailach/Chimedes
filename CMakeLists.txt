cmake_minimum_required(VERSION 3.22)

# Link the MSVC runtime statically, so Chimedes.exe runs on any Windows
# machine without installing the Visual C++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Chimedes VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Source code is UTF-8 (text with accents). Visual Studio needs to know.
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

option(CHIMEDES_BUILD_APP   "Build the desktop application (requires JUCE)" ON)
option(CHIMEDES_BUILD_TESTS "Build the engine tests" ON)

# ---------------------------------------------------------------------------
# Core: model and engine in plain C++, with no JUCE dependency.
# Built separately so it can be tested without UI or audio.
# ---------------------------------------------------------------------------
add_library(chimedes_core STATIC
    Source/model/Track.cpp
    Source/model/TrackEditing.cpp
    Source/model/NoteNames.cpp
    Source/model/Randomizer.cpp
    Source/model/ProjectFile.cpp
    Source/engine/Export.cpp
    Source/engine/Adjacency.cpp
    Source/engine/Validator.cpp
    Source/engine/TreeBuilder.cpp
    Source/engine/Traversal.cpp
    Source/engine/Scheduler.cpp
)
target_include_directories(chimedes_core PUBLIC Source)

if(MSVC)
    target_compile_options(chimedes_core PRIVATE /W4)
else()
    target_compile_options(chimedes_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Engine tests
# ---------------------------------------------------------------------------
if(CHIMEDES_BUILD_TESTS)
    enable_testing()
    add_executable(chimedes_tests
        Tests/TestMain.cpp
        Tests/ValidatorTests.cpp
        Tests/TreeBuilderTests.cpp
        Tests/SchedulerTests.cpp
        Tests/ProjectFileTests.cpp
        Tests/ExportTests.cpp
        Tests/SequencerTests.cpp
        Tests/EditingTests.cpp
        Tests/RandomizerTests.cpp
    )
    target_link_libraries(chimedes_tests PRIVATE chimedes_core)
    target_include_directories(chimedes_tests PRIVATE Tests)
    add_test(NAME chimedes_tests COMMAND chimedes_tests)
endif()

# ---------------------------------------------------------------------------
# Desktop application
# ---------------------------------------------------------------------------
if(CHIMEDES_BUILD_APP)
    add_subdirectory(JUCE)

    juce_add_gui_app(Chimedes
        PRODUCT_NAME "Chimedes"
        COMPANY_NAME "bfailach"
        VERSION ${PROJECT_VERSION}
        # App icon (window, taskbar, executable). Source: Resources/Logo/chimedes-icon.svg
        ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Logo/chimedes-icon-1024.png"
        ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Logo/chimedes-icon-256.png"
    )

    target_sources(Chimedes PRIVATE
        Source/Main.cpp
        Source/audio/AudioEngine.cpp
        Source/audio/OfflineRender.cpp
        Source/ui/ChimedesLookAndFeel.cpp
        Source/ui/Fonts.cpp
        Source/ui/GraphView.cpp
        Source/ui/Inspector.cpp
        Source/ui/InstrumentList.cpp
        Source/ui/MainComponent.cpp
        Source/ui/PianoPicker.cpp
        Source/ui/TrackList.cpp
        Source/ui/TrackGlance.cpp
        Source/ui/WindowStyle.cpp
    )

    # Theme fonts (Sora and DM Mono, SIL OFL license) and the
    # window icon used where the platform does not take it from the executable.
    juce_add_binary_data(ChimedesResources
        HEADER_NAME BinaryData.h
        NAMESPACE BinaryData
        SOURCES
            Resources/Fonts/Sora.ttf
            Resources/Fonts/DMMono.ttf
            Resources/Logo/chimedes-icon-256.png
    )

    target_compile_definitions(Chimedes PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Chimedes,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Chimedes,JUCE_VERSION>"
    )

    target_link_libraries(Chimedes
        PRIVATE
            chimedes_core
            ChimedesResources
            juce::juce_gui_extra
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    # Dark title bar on Windows (DwmSetWindowAttribute).
    if(WIN32)
        target_link_libraries(Chimedes PRIVATE dwmapi)
    endif()

    # Development tool: renders the demo graph to WAV without a sound
    # card, using the same audio engine as the application.
    juce_add_console_app(ChimedesRender PRODUCT_NAME "ChimedesRender")

    target_sources(ChimedesRender PRIVATE
        Tools/RenderDemo.cpp
        Source/audio/AudioEngine.cpp
        Source/audio/OfflineRender.cpp
    )

    target_compile_definitions(ChimedesRender PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )

    target_link_libraries(ChimedesRender
        PRIVATE
            chimedes_core
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endif()
