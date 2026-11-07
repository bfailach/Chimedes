#include "TrackList.h"

#include "model/Instruments.h"
#include "ui/Text.h"
#include "ui/Theme.h"

namespace chimedes::ui
{

// Altavoz: con ondas si suena; tachado si la pista está silenciada.
class TrackList::MuteButton : public juce::Button
{
public:
    MuteButton() : juce::Button ("Silenciar")
    {
        setClickingTogglesState (false);
    }

    void paintButton (juce::Graphics& g, bool isMouseOver, bool isDown) override
    {
        const auto bounds = getLocalBounds().toFloat();
        if (isMouseOver || isDown)
        {
            g.setColour (isDown ? theme::controlPressed : theme::controlHover);
            g.fillRoundedRectangle (bounds.reduced (2.0f), theme::controlRadius);
        }

        const bool muted = getToggleState();
        const auto box = juce::Rectangle<float> (16.0f, 16.0f).withCentre (bounds.getCentre());
        const auto x = box.getX();
        const auto y = box.getY();

        juce::Path speaker;
        speaker.startNewSubPath (x + 1.5f, y + 6.0f);
        speaker.lineTo (x + 4.5f, y + 6.0f);
        speaker.lineTo (x + 8.5f, y + 2.5f);
        speaker.lineTo (x + 8.5f, y + 13.5f);
        speaker.lineTo (x + 4.5f, y + 10.0f);
        speaker.lineTo (x + 1.5f, y + 10.0f);
        speaker.closeSubPath();

        juce::Path extra;
        if (muted)
        {
            extra.startNewSubPath (x + 11.0f, y + 5.5f);
            extra.lineTo (x + 15.5f, y + 10.5f);
            extra.startNewSubPath (x + 15.5f, y + 5.5f);
            extra.lineTo (x + 11.0f, y + 10.5f);
        }
        else
        {
            extra.addCentredArc (x + 9.0f, y + 8.0f, 3.5f, 3.5f, 0.0f, 0.6f, 2.55f, true);
            extra.addCentredArc (x + 9.0f, y + 8.0f, 6.5f, 6.5f, 0.0f, 0.6f, 2.55f, true);
        }

        const juce::PathStrokeType stroke (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (muted ? theme::textDisabled : theme::textSecondary);
        g.strokePath (speaker, stroke);
        g.strokePath (extra, stroke);
    }
};

// Una fila: nombre de la pista, instrumento y botón de silencio.
class TrackList::Row : public juce::Component
{
public:
    explicit Row (TrackList& ownerToUse) : owner (ownerToUse)
    {
        name.setInterceptsMouseClicks (false, false);
        name.setFont (theme::sans (13.0f));
        name.setColour (juce::Label::textColourId, theme::textPrimary);
        name.setColour (juce::Label::textWhenEditingColourId, theme::textPrimary);
        name.setColour (juce::Label::backgroundWhenEditingColourId, theme::control);
        name.setColour (juce::Label::outlineWhenEditingColourId, theme::focus);
        name.onTextChange = [this]
        {
            if (owner.onRename)
                owner.onRename (index, name.getText().trim());
        };

        detail.setInterceptsMouseClicks (false, false);
        detail.setFont (theme::sans (11.0f));
        detail.setColour (juce::Label::textColourId, theme::textSecondary);
        detail.setJustificationType (juce::Justification::centredRight);

        mute.onClick = [this]
        {
            if (owner.onToggleMute)
                owner.onToggleMute (index);
        };

        addAndMakeVisible (name);
        addAndMakeVisible (detail);
        addAndMakeVisible (mute);
    }

    void update (std::size_t newIndex, const model::Track& track, bool isActive)
    {
        index = newIndex;
        active = isActive;

        const auto title = track.name.empty() ? text ("Pista ") + juce::String (static_cast<int> (index) + 1)
                                              : juce::String::fromUTF8 (track.name.c_str());
        name.setText (title, juce::dontSendNotification);
        name.setColour (juce::Label::textColourId, track.muted ? theme::textDisabled
                                                   : isActive  ? theme::textPrimary
                                                               : theme::textSecondary);

        // Instrumento de la pista, abreviado, para distinguirlas de un vistazo.
        detail.setText (text (model::instruments::name (track.instrument)), juce::dontSendNotification);

        mute.setToggleState (track.muted, juce::dontSendNotification);
        mute.setTooltip (track.muted ? text ("Volver a oír") : text ("Silenciar"));
        repaint();
    }

    void startRename() { name.showEditor(); }

    void paint (juce::Graphics& g) override
    {
        if (active || isMouseOver())
        {
            g.setColour (active ? theme::control : theme::control.withAlpha (0.5f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), theme::controlRadius);
        }

        // Punto blanco estelar en la pista activa.
        if (active)
        {
            g.setColour (theme::accent);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ 10.0f, getHeight() * 0.5f }));
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedLeft (18);
        mute.setBounds (area.removeFromRight (30));
        detail.setBounds (area.removeFromRight (76));
        name.setBounds (area);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            owner.showRowMenu (index);
        else if (owner.onSelect)
            owner.onSelect (index);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu())
            startRename();
    }

private:
    TrackList& owner;
    std::size_t index = 0;
    bool active = false;
    juce::Label name, detail;
    MuteButton mute;
};

TrackList::TrackList()
{
    header.setText ("Pistas", juce::dontSendNotification);
    header.setFont (theme::sans (13.0f));
    header.setColour (juce::Label::textColourId, theme::textSecondary);
    addAndMakeVisible (header);

    addButton.onClick = [this]
    {
        if (onAdd)
            onAdd();
    };
    addAndMakeVisible (addButton);
}

TrackList::~TrackList() = default;

void TrackList::refresh (const model::Project& project, std::size_t activeIndex)
{
    active = activeIndex;

    while (rows.size() < project.tracks.size())
    {
        rows.push_back (std::make_unique<Row> (*this));
        addAndMakeVisible (*rows.back());
    }
    while (rows.size() > project.tracks.size())
        rows.pop_back();

    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->update (i, project.tracks[i], i == activeIndex);

    addButton.setEnabled (project.tracks.size() < maxTracks);
    addButton.setTooltip (project.tracks.size() < maxTracks ? text ("Pista nueva")
                                                            : text ("Máximo 16 pistas"));
    resized();
}

int TrackList::getIdealHeight() const
{
    return padding + headerHeight + static_cast<int> (rows.size()) * rowHeight + padding;
}

void TrackList::paint (juce::Graphics& g)
{
    g.setColour (theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), theme::cornerRadius);
    g.setColour (theme::panelOutline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), theme::cornerRadius, 1.0f);
}

void TrackList::resized()
{
    auto area = getLocalBounds().reduced (padding);
    auto top = area.removeFromTop (headerHeight);
    addButton.setBounds (top.removeFromRight (34).withSizeKeepingCentre (34, 30));
    header.setBounds (top.withTrimmedLeft (4));

    for (auto& row : rows)
        row->setBounds (area.removeFromTop (rowHeight));
}

void TrackList::showRowMenu (std::size_t index)
{
    enum { rename = 1, duplicate, remove };

    juce::PopupMenu menu;
    menu.addItem (rename, "Renombrar");
    menu.addItem (duplicate, "Duplicar", rows.size() < maxTracks);
    menu.addItem (remove, "Eliminar", rows.size() > 1);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (rows[index].get()),
                        [safe = juce::Component::SafePointer<TrackList> (this), index] (int result)
    {
        if (safe == nullptr || index >= safe->rows.size())
            return;

        if (result == rename)
            safe->rows[index]->startRename();
        else if (result == duplicate && safe->onDuplicate)
            safe->onDuplicate (index);
        else if (result == remove && safe->onDelete)
            safe->onDelete (index);
    });
}

} // namespace chimedes::ui
