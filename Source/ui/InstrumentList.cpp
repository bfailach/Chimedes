#include "InstrumentList.h"

#include "model/Instruments.h"
#include "ui/InstrumentIcons.h"
#include "ui/Text.h"
#include "ui/Theme.h"

namespace chimedes::ui
{

InstrumentList::InstrumentList()
{
    setRepaintsOnMouseActivity (false);
    rebuild();
}

void InstrumentList::setSamples (std::vector<std::pair<model::InstrumentId, juce::String>> newSamples, bool canLoadMore)
{
    if (newSamples == samples && canLoadMore == canLoadSample)
        return;

    samples = std::move (newSamples);
    canLoadSample = canLoadMore;
    rebuild();
}

void InstrumentList::setSelected (model::InstrumentId id)
{
    if (id != selected)
    {
        selected = id;
        repaint();
    }
}

void InstrumentList::rebuild()
{
    rows.clear();
    int y = 0;

    const auto add = [&] (Kind kind, model::InstrumentId id, const juce::String& label)
    {
        const auto height = kind == Kind::Heading ? headingHeight : rowHeight;
        rows.push_back ({ kind, id, label, y, height });
        y += height;
    };

    for (std::size_t c = 0; c < model::instruments::categoryNames.size(); ++c)
    {
        add (Kind::Heading, 0, text (model::instruments::categoryNames[c]));

        for (const auto& info : model::instruments::builtIns)
            if (info.category == static_cast<model::instruments::Category> (c))
                add (Kind::BuiltIn, info.id, text (info.name));
    }

    add (Kind::Heading, 0, text ("Samples"));
    for (const auto& [id, name] : samples)
        add (Kind::Sample, id, name);
    add (Kind::LoadSample, 0, canLoadSample ? text ("Load sample…") : text ("Load sample… (10 max)"));

    hovered = -1;
    setSize (getWidth(), y);
    repaint();
}

int InstrumentList::getContentHeight() const
{
    return rows.empty() ? 0 : rows.back().top + rows.back().height;
}

juce::Range<int> InstrumentList::rowRange (model::InstrumentId id) const
{
    for (const auto& r : rows)
        if (r.kind != Kind::Heading && r.kind != Kind::LoadSample && r.id == id)
            return { r.top, r.top + r.height };
    return {};
}

int InstrumentList::rowAt (int y) const
{
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (y >= rows[i].top && y < rows[i].top + rows[i].height)
            return rows[i].kind == Kind::Heading ? -1 : static_cast<int> (i);
    return -1;
}

void InstrumentList::paint (juce::Graphics& g)
{
    const auto width = static_cast<float> (getWidth());

    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows[i];
        const auto bounds = juce::Rectangle<float> (0.0f, static_cast<float> (r.top), width, static_cast<float> (r.height));

        if (r.kind == Kind::Heading)
        {
            g.setColour (theme::textSecondary);
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (r.label, bounds.withTrimmedLeft (4.0f).withTrimmedTop (6.0f), juce::Justification::centredLeft);
            continue;
        }

        const bool enabled = r.kind != Kind::LoadSample || canLoadSample;
        const bool isSelected = r.kind != Kind::LoadSample && r.id == selected;
        const bool isHovered = enabled && static_cast<int> (i) == hovered;
        const auto row = bounds.reduced (0.0f, 1.0f);

        if (isSelected)
        {
            g.setColour (theme::controlPressed);
            g.fillRoundedRectangle (row, theme::controlRadius);
            g.setColour (theme::accent.withAlpha (0.35f));
            g.drawRoundedRectangle (row.reduced (0.5f), theme::controlRadius, 1.0f);
        }
        else if (isHovered)
        {
            g.setColour (theme::controlHover);
            g.fillRoundedRectangle (row, theme::controlRadius);
        }

        const auto colour = ! enabled ? theme::textDisabled
                          : isSelected ? theme::accent
                          : isHovered ? theme::textPrimary
                                      : theme::textSecondary;

        const auto icon = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ row.getX() + 17.0f, row.getCentreY() });
        g.setColour (colour);
        if (r.kind == Kind::BuiltIn)
            instrumentIcons::draw (g, icon, r.id);
        else
            instrumentIcons::draw (g, icon, r.kind == Kind::Sample ? instrumentIcons::Special::Sample
                                                                  : instrumentIcons::Special::LoadSample);

        g.setColour (! enabled ? theme::textDisabled : isSelected || isHovered ? theme::textPrimary : theme::textPrimary.withAlpha (0.82f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (r.label, row.withTrimmedLeft (36.0f).withTrimmedRight (8.0f), juce::Justification::centredLeft, true);
    }
}

void InstrumentList::mouseMove (const juce::MouseEvent& e)
{
    const auto row = rowAt (e.y);
    if (row != hovered)
    {
        hovered = row;
        setMouseCursor (row >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void InstrumentList::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1)
    {
        hovered = -1;
        repaint();
    }
}

void InstrumentList::mouseDown (const juce::MouseEvent& e)
{
    const auto index = rowAt (e.y);
    if (index < 0)
        return;

    const auto& r = rows[static_cast<std::size_t> (index)];

    if (r.kind == Kind::LoadSample)
    {
        if (canLoadSample && onLoadSample)
            onLoadSample();
        return;
    }

    if (r.id != selected)
    {
        setSelected (r.id);
        if (onSelect)
            onSelect (r.id);
    }
}

} // namespace chimedes::ui
