#pragma once

#include <functional>
#include <utility>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "model/Track.h"

namespace chimedes::ui
{

// Always-open list of instruments: "Load sample…", the project's samples and
// then the built-ins under their family headings. Each row has its icon; the
// chosen one is highlighted. It is as tall as its content: put it in a Viewport.
class InstrumentList : public juce::Component
{
public:
    InstrumentList();

    // The project's samples (id and name) and whether another one fits.
    void setSamples (std::vector<std::pair<model::InstrumentId, juce::String>> samples, bool canLoadMore);

    // Highlights an instrument (it does not call onSelect).
    void setSelected (model::InstrumentId id);
    model::InstrumentId getSelected() const noexcept { return selected; }

    // Top and bottom of an instrument's row (to scroll it into view).
    juce::Range<int> rowRange (model::InstrumentId id) const;

    int getContentHeight() const;

    std::function<void (model::InstrumentId)> onSelect;
    std::function<void()> onLoadSample;

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;

    static constexpr int rowHeight = 28;
    static constexpr int headingHeight = 24;

private:
    enum class Kind { Heading, BuiltIn, Sample, LoadSample };

    struct Row
    {
        Kind kind;
        model::InstrumentId id = 0;
        juce::String label;
        int top = 0, height = 0;
    };

    void rebuild();
    int rowAt (int y) const;

    std::vector<Row> rows;
    std::vector<std::pair<model::InstrumentId, juce::String>> samples;
    bool canLoadSample = true;
    model::InstrumentId selected = 0;
    int hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InstrumentList)
};

} // namespace chimedes::ui
