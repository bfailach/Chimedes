#pragma once

#include <functional>
#include <set>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace chimedes::ui
{

// Teclado de piano de dos octavas para elegir las notas de un nodo.
//
// Tocar una tecla la hace sonar y la deja como propuesta; Enter la guarda en el
// nodo y Esc la descarta. Así se puede probar antes de decidir.
//   clic ..................... propone esa nota sola
//   Mayús + clic ............. añade o quita la nota del acorde propuesto
//   letras del teclado ....... A W S E D F T G Y H U J K O L P, de do a re# de la
//                              octava siguiente, empezando en el primer do visible.
//                              Varias teclas a la vez forman un acorde; Mayús + letra
//                              añade o quita la nota, como con el ratón.
//   Z / X, < > ............... octava abajo / arriba
//   Enter / Esc .............. guardar / descartar la propuesta
class PianoPicker : public juce::Component
{
public:
    PianoPicker();

    // Se llama al guardar (Enter) con las notas, ordenadas de grave a agudo.
    std::function<void (std::vector<int>)> onCommit;

    // Una tecla empieza (true) o deja de sonar (false).
    std::function<void (int pitch, bool isDown)> onAudition;

    // La propuesta cambió (para mostrar sus nombres y la ayuda de Enter / Esc).
    std::function<void()> onPendingChanged;

    // Notas guardadas del nodo. Si cambian, la propuesta se descarta; si son las
    // mismas (la pista se refrescó por otra edición), la propuesta se conserva.
    // Si la más grave queda fuera de vista, mueve el teclado a ella.
    void setPitches (const std::vector<int>& pitches);

    // La propuesta actual (igual a las guardadas si no hay ninguna).
    const std::vector<int>& getPendingPitches() const noexcept { return pending; }
    bool hasPendingChange() const noexcept { return pending != saved; }

    void commitPending();
    void discardPending();

    // Teclado de la computadora. La ventana se las pasa mientras el panel muestra un
    // nodo; devuelven true si la tecla era suya.
    bool handleKeyPress (const juce::KeyPress& key);
    bool handleKeyStateChange();

    // Apaga las notas que estén sonando (al perder el foco, al cambiar de nodo...).
    void releaseAllKeys();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    static constexpr int whiteKeysShown = 14;  // dos octavas
    static constexpr int headerHeight = 24;

    int lowestPitch() const noexcept { return (baseOctave + 1) * 12; }
    int pitchAt (juce::Point<float> position) const;
    juce::Rectangle<float> keyBounds (int pitch) const;
    juce::Rectangle<float> keyboardArea() const;
    void setBaseOctave (int octave);
    void updateRangeLabel();

    // Una tecla nueva se toca: suena y cambia la propuesta.
    //   asChord: se suma al acorde (otras teclas sostenidas);  toggle: Mayús.
    void playKey (int pitch, bool asChord, bool toggle);
    void setPending (std::vector<int> next);

    static bool isBlack (int pitch) noexcept;

    std::vector<int> saved;     // las notas del nodo
    std::vector<int> pending;   // la propuesta (lo que se ve y se guarda con Enter)
    int baseOctave = 3;         // el teclado empieza en C3

    int mousePitch = -1;            // tecla sostenida con el ratón
    std::set<int> heldKeyCodes;     // letras sostenidas
    std::set<int> soundingPitches;  // notas de prueba sonando, para apagarlas

    juce::TextButton downButton { "<" }, upButton { ">" };
    juce::Label rangeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoPicker)
};

} // namespace chimedes::ui
