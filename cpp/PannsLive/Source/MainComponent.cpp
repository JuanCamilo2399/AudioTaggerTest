#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent() :
    m_engine(m_manager)
{
    m_manager.initialise(2, 2);
    m_settings = std::make_unique<IOComponent>(m_manager);
    m_pannsLivePage = std::make_unique<PannsLiveComponent>(m_manager, m_engine);

    m_manager.addChangeListener(m_settings.get());
    
    addAndMakeVisible(m_tabs);

    m_tabs.addTab("Settings",
        juce::Colours::darkgrey,
        m_settings.get(),
        false);

    m_tabs.addTab("PANNs Live",
        juce::Colours::darkgrey,
        m_pannsLivePage.get(),
        false);

    m_tabs.onTabChanged = [this](int newTabIndex)
    {
        handleTabChanged(newTabIndex);
    };
    
    handleTabChanged(m_tabs.getCurrentTabIndex());
    setSize(1100, 760);
}

MainComponent::~MainComponent()
{
    m_engine.stop();
    m_engine.clearMode();

    m_manager.removeChangeListener(m_settings.get());

    m_tabs.clearTabs();

    m_pannsLivePage.reset();
    m_settings.reset();
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    m_tabs.setBounds(getLocalBounds());
}

void MainComponent::handleTabChanged(int newTabIndex)
{
    m_engine.clearMode();

    auto tabType = static_cast<TabComponents>(newTabIndex);

    if (tabType != TabComponents::pannsLive && m_pannsLivePage != nullptr)
        m_pannsLivePage->resetUIState();

    switch (tabType)
    {
    case MainComponent::TabComponents::settings:
        m_engine.stop();
        break;
    case MainComponent::TabComponents::pannsLive:
        m_engine.start();
        break;
    default:
        break;
    }
}

IOComponent::IOComponent(DeviceManager& deviceManager) :
    m_manager(deviceManager),
    audioSetupComp(m_manager.getAudioDeviceManager(),
        0,     // minimum input channels
        256,   // maximum input channels
        0,     // minimum output channels
        256,   // maximum output channels
        false, // ability to select midi inputs
        false, // ability to select midi output device
        false, // treat channels as stereo pairs
        false) // hide advanced options
{
    addAndMakeVisible(audioSetupComp);
    
    addAndMakeVisible(diagnosticsBox);

    diagnosticsBox.setMultiLine(true);
    diagnosticsBox.setReturnKeyStartsNewLine(true);
    diagnosticsBox.setReadOnly(true);
    diagnosticsBox.setScrollbarsShown(true);
    diagnosticsBox.setCaretVisible(false);
    diagnosticsBox.setPopupMenuEnabled(true);
    diagnosticsBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x32ffffff));
    diagnosticsBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0x1c000000));
    diagnosticsBox.setColour(juce::TextEditor::shadowColourId, juce::Colour(0x16000000));

}

IOComponent::~IOComponent()
{

}

void IOComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void IOComponent::resized()
{
    auto rect = getLocalBounds();

    auto buttonsArea = rect.removeFromTop(5).reduced(10, 5);

    audioSetupComp.setBounds(rect.removeFromLeft(proportionOfWidth(0.6f)));

    rect.reduce(10, 10);

    auto topLine(rect.removeFromTop(5));
    cpuUsageLabel.setBounds(topLine.removeFromLeft(topLine.getWidth() / 2));
    cpuUsageText.setBounds(topLine);
    rect.removeFromTop(20);

    diagnosticsBox.setBounds(rect);
}

void IOComponent::dumpDeviceInfo()
{
    auto& dManager = m_manager.getAudioDeviceManager();
    diagnosticsBox.clear();
    logMessage("--------------------------------------");
    logMessage("Current audio device type: " + (dManager.getCurrentDeviceTypeObject() != nullptr
        ? dManager.getCurrentDeviceTypeObject()->getTypeName()
        : "<none>"));

    if (auto* device = dManager.getCurrentAudioDevice())
    {
        logMessage("Current audio device: " + device->getName().quoted());
        logMessage("Sample rate: " + juce::String(device->getCurrentSampleRate()) + " Hz");
        logMessage("Block size: " + juce::String(device->getCurrentBufferSizeSamples()) + " samples");
        logMessage("Bit depth: " + juce::String(device->getCurrentBitDepth()));
        logMessage("Input channel names: " + device->getInputChannelNames().joinIntoString(", "));
        logMessage("Active input channels: " + ChannelBits::getListOfActiveBits(device->getActiveInputChannels()));
        logMessage("Output channel names: " + device->getOutputChannelNames().joinIntoString(", "));
        logMessage("Active output channels: " + ChannelBits::getListOfActiveBits(device->getActiveOutputChannels()));
    }
    else
    {
        logMessage("No audio device open");
    }
}
