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

    void initialise (const juce::String& commandLine) override
    {
        // The theme applies to the whole app, including menus and pop-ups.
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        mainWindow = std::make_unique<MainWindow> (getApplicationName());

        // Double-clicking a .chimedes file: the system passes its path as an argument.
        const auto path = commandLine.unquoted().trim();
        if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
            if (auto* content = mainContent())
                content->openFile (juce::File (path));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        // With unsaved changes, ask first.
        if (auto* content = mainContent())
            content->confirmDiscardChanges ([] { juce::JUCEApplication::getInstance()->quit(); });
        else
            quit();
    }

    chimedes::ui::MainComponent* mainContent() const
    {
        return mainWindow != nullptr ? dynamic_cast<chimedes::ui::MainComponent*> (mainWindow->getContentComponent())
                                     : nullptr;
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
            chimedes::ui::applyDarkTitleBar (*this);  // after setVisible: needs the native window
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    chimedes::ui::ChimedesLookAndFeel lookAndFeel;  // must outlive the window
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (ChimedesApplication)
