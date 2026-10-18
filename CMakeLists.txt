cmake_minimum_required(VERSION 3.22)

project(Chimedes VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# El código fuente está en UTF-8 (textos con acentos). Visual Studio necesita saberlo.
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

option(CHIMEDES_BUILD_APP   "Compilar la aplicación de escritorio (requiere JUCE)" ON)
option(CHIMEDES_BUILD_TESTS "Compilar las pruebas del motor" ON)

# ---------------------------------------------------------------------------
# Núcleo: modelo y motor en C++ puro, sin dependencias de JUCE.
# Se compila aparte para poder probarlo sin interfaz ni audio.
# ---------------------------------------------------------------------------
add_library(chimedes_core STATIC
    Source/model/Track.cpp
    Source/model/TrackEditing.cpp
    Source/model/NoteNames.cpp
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
# Pruebas del motor
# ---------------------------------------------------------------------------
if(CHIMEDES_BUILD_TESTS)
    enable_testing()
    add_executable(chimedes_tests
        Tests/TestMain.cpp
        Tests/ValidatorTests.cpp
        Tests/TreeBuilderTests.cpp
        Tests/SchedulerTests.cpp
        Tests/SequencerTests.cpp
        Tests/EditingTests.cpp
    )
    target_link_libraries(chimedes_tests PRIVATE chimedes_core)
    target_include_directories(chimedes_tests PRIVATE Tests)
    add_test(NAME chimedes_tests COMMAND chimedes_tests)
endif()

# ---------------------------------------------------------------------------
# Aplicación de escritorio
# ---------------------------------------------------------------------------
if(CHIMEDES_BUILD_APP)
    add_subdirectory(JUCE)

    juce_add_gui_app(Chimedes
        PRODUCT_NAME "Chimedes"
        COMPANY_NAME "bfailach"
        VERSION ${PROJECT_VERSION}
    )

    target_sources(Chimedes PRIVATE
        Source/Main.cpp
        Source/audio/AudioEngine.cpp
        Source/ui/ChimedesLookAndFeel.cpp
        Source/ui/GraphView.cpp
        Source/ui/Inspector.cpp
        Source/ui/MainComponent.cpp
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
            juce::juce_gui_extra
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    # Herramienta de desarrollo: renderiza el grafo de demostración a WAV sin tarjeta
    # de sonido, con el mismo motor de audio que la aplicación.
    juce_add_console_app(ChimedesRender PRODUCT_NAME "ChimedesRender")

    target_sources(ChimedesRender PRIVATE
        Tools/RenderDemo.cpp
        Source/audio/AudioEngine.cpp
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
