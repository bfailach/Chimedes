#pragma once

#include <functional>
#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "audio/AudioEngine.h"
#include "engine/Schedule.h"
#include "model/Track.h"
#include "ui/GraphView.h"
#include "ui/IconButton.h"
#include "ui/Inspector.h"
#include "ui/TransportButton.h"

namespace chimedes::ui
{

// Ventana principal: transporte arriba, lienzo en el centro y propiedades a la
// derecha. Es la dueña de la pista: aplica cada cambio, lo registra para deshacer,
// recalcula el horario y lo publica al audio.
class MainComponent : public juce::Component,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType cause) override;
    void parentHierarchyChanged() override;

    // --- Archivos -------------------------------------------------------------
    // Si hay cambios sin guardar, pregunta (Guardar / No guardar / Cancelar) y solo
    // llama a `proceed` si se puede seguir. Lo usa también la app al cerrarse.
    void confirmDiscardChanges (std::function<void()> proceed);

    // Abre un proyecto (también desde la línea de comandos, al hacer doble clic).
    void openFile (const juce::File& file);

private:
    class TrackChange;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;  // el historial cambió

    // Cambios a la pista
    void applyEdit (const EditCallbacks::Change& change);
    void beginGesture();
    void updateGesture (const EditCallbacks::Change& change);
    void endGesture();
    void setTrack (const model::Track& newTrack);  // usado por deshacer y rehacer
    void trackChanged();

    void rebuildSchedule();
    void togglePlayPause();
    void updateUndoButtons();

    // Archivos
    void newProject();
    void openProject();
    void saveProject (std::function<void (bool saved)> done = {});
    void saveProjectAs (std::function<void (bool saved)> done = {});
    bool writeFile (const juce::File& file);
    void markSaved();
    bool hasUnsavedChanges() const;
    void updateTitle();
    juce::File defaultFolder() const;
    juce::String projectDisplayName() const;

    // Zona del lienzo que no tapan los paneles (para encuadrar el grafo).
    juce::Rectangle<int> freeCanvasArea() const;

    // Audio
    audio::AudioEngine engine;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer audioPlayer;

    // Datos
    model::Track track;
    std::shared_ptr<const engine::Schedule> schedule;
    juce::UndoManager undoManager;
    std::optional<model::Track> gestureStart;

    // Archivo abierto (vacío = sin título) y lo último guardado, para saber si hay cambios.
    juce::File currentFile;
    model::Track savedTrack;
    double savedBpm = 120.0;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Fondo redondeado de la barra inferior flotante.
    struct FloatingPanel : public juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            // Forma de píldora: el radio es la mitad de la altura.
            const auto radius = static_cast<float> (getHeight()) * 0.5f;
            g.setColour (theme::panel);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), radius);
            g.setColour (theme::panelOutline);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), radius, 1.0f);
        }
    };

    // Interfaz
    FloatingPanel topBar;      // archivo: nuevo, abrir, guardar y nombre del proyecto
    FloatingPanel bottomBar;
    FloatingPanel statusBar;   // avisos (nodos inalcanzables, audio): solo si hay alguno
    TransportButton stopButton { "Detener", TransportButton::Icon::Stop };
    TransportButton playButton { "Reproducir", TransportButton::Icon::Play };
    TransportButton pauseButton { "Pausa", TransportButton::Icon::Pause };
    juce::Label bpmLabel { {}, "BPM" };
    juce::Slider bpmSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    IconButton randomButton { "Aleatorio", IconButton::Icon::Random };
    IconButton newButton { "Nuevo (Ctrl+N)", IconButton::Icon::New };
    IconButton openButton { "Abrir (Ctrl+O)", IconButton::Icon::Open };
    IconButton saveButton { "Guardar (Ctrl+S)", IconButton::Icon::Save };
    IconButton undoButton { "Deshacer (Ctrl+Z)", IconButton::Icon::Undo };
    IconButton redoButton { "Rehacer (Ctrl+Y)", IconButton::Icon::Redo };
    juce::Label statusLabel;
    juce::Label projectLabel;
    GraphView graphView;
    Inspector inspector;
    juce::TooltipWindow tooltips { this, 600 };

    juce::String audioWarning;
    int timerTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace chimedes::ui
