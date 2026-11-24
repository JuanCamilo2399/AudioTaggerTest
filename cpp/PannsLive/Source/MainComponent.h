#pragma once

#include <JuceHeader.h>
#include "AudioEngine/AudioEngine.h"
#include "AudioEngine/DeviceManager.h"
#include "PannsLive/PannsLiveComponent.h"

class EngineTabbedComponent : public juce::TabbedComponent
{
public:
    EngineTabbedComponent()
        : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop)
    {
    }

    std::function<void(int)> onTabChanged;

private:
    void currentTabChanged(int newCurrentTabIndex,
        const juce::String& newCurrentTabName) override
    {
        juce::ignoreUnused(newCurrentTabName);

        if (onTabChanged)
            onTabChanged(newCurrentTabIndex);
    }
};

class IOComponent : public juce::Component,
                    private juce::Timer,
                    public juce::ChangeListener
{
public:
    IOComponent(DeviceManager& deviceManager);
    ~IOComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void dumpDeviceInfo();
private:
    void logMessage(const juce::String& m)
    {
        diagnosticsBox.moveCaretToEnd();
        diagnosticsBox.insertTextAtCaret(m + juce::newLine);
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        dumpDeviceInfo();
    }

    void timerCallback() override
    {
        auto cpu = m_manager.getCpuUsage() * 100;
        cpuUsageText.setText(juce::String(cpu, 6) + " %", juce::dontSendNotification);
    }
    DeviceManager& m_manager;
    juce::AudioDeviceSelectorComponent audioSetupComp;
    juce::Label cpuUsageLabel;
    juce::Label cpuUsageText;
    juce::TextEditor diagnosticsBox;
};

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::Component
{
    enum class TabComponents
    {
        settings = 0,
        pannsLive
    };

public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void handleTabChanged(int newTabIndex);
private:
    DeviceManager m_manager;
    AudioEngine m_engine;
   
    EngineTabbedComponent m_tabs;
    
    std::unique_ptr<IOComponent> m_settings;
    std::unique_ptr<PannsLiveComponent> m_pannsLivePage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
