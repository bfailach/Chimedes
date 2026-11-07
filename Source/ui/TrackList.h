#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "model/Track.h"
#include "ui/IconButton.h"

namespace chimedes::ui
{

// Panel izquierdo: las pistas del proyecto. Todas suenan a la vez; la activa es la
// que se ve entera y se edita en el lienzo y en el panel derecho.
//   clic ............... activar la pista
//   doble clic ......... renombrar
//   altavoz ............ silenciar / volver a oír
//   clic derecho ....... renombrar, duplicar, eliminar
//   + .................. pista nueva
class TrackList : public juce::Component
{
public:
    static constexpr int maxTracks = 16;   // una por ranura del motor de audio

    TrackList();
    ~TrackList() override;

    std::function<void (std::size_t)> onSelect;
    std::function<void (std::size_t)> onToggleMute;
    std::function<void (std::size_t)> onDuplicate;
    std::function<void (std::size_t)> onDelete;
    std::function<void (std::size_t, juce::String)> onRename;
    std::function<void()> onAdd;

    void refresh (const model::Project& project, std::size_t activeIndex);

    // Alto que necesita para mostrar todas las pistas.
    int getIdealHeight() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class Row;
    class MuteButton;

    static constexpr int headerHeight = 34;
    static constexpr int rowHeight = 34;
    static constexpr int padding = 10;

    void showRowMenu (std::size_t index);

    juce::Label header;
    IconButton addButton { "Pista nueva", IconButton::Icon::Add };
    std::vector<std::unique_ptr<Row>> rows;
    std::size_t active = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackList)
};

} // namespace chimedes::ui
