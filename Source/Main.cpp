#include <juce_gui_extra/juce_gui_extra.h>

#include "ui/ChimedesLookAndFeel.h"
#include "ui/MainComponent.h"
#include "ui/Theme.h"
#include "ui/WindowStyle.h"

class ChimedesApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        // El tema se aplica a toda la app, también a menús y ventanas emergentes.
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& name)
            : DocumentWindow (name, chimedes::ui::theme::background, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new chimedes::ui::MainComponent(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
            chimedes::ui::applyDarkTitleBar (*this);  // tras setVisible: necesita la ventana nativa
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    chimedes::ui::ChimedesLookAndFeel lookAndFeel;  // debe vivir más que la ventana
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (ChimedesApplication)
