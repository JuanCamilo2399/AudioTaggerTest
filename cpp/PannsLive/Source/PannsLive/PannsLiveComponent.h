#pragma once

#include <JuceHeader.h>
#include "../AudioEngine/AudioEngine.h"
#include "../AudioEngine/DeviceManager.h"
#include "PannsWorker.h"
#include <memory>
#include <vector>

class PannsLiveComponent : public juce::Component,
                           private juce::Timer
{
public:
    PannsLiveComponent(DeviceManager& deviceManager, AudioEngine& engine);
    ~PannsLiveComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void resetUIState();

private:
    void timerCallback() override;

    void startAnalysis();
    void stopAnalysis();
    bool ensureModelLoaded();

    void refreshCameraList();
    void openCamera(int deviceIndex);
    void closeCamera();

    void appendEvents(const std::vector<PannsEvent>& events);
    void appendLogLine(const juce::String& line);
    void openCsvLog();

    juce::File getProjectRootFolder();
    static juce::String formatTime(juce::int64 ms, bool withDate);

    DeviceManager& m_manager;
    AudioEngine& m_engine;

    PannsWorker m_worker;
    std::shared_ptr<PannsAudioFifo> m_fifo;

    std::unique_ptr<juce::CameraDevice> m_camera;
    std::unique_ptr<juce::Component> m_cameraViewer;
    juce::Rectangle<int> m_cameraArea;

    juce::Label m_cameraLabel;
    juce::ComboBox m_cameraBox;
    juce::TextButton m_refreshCamerasButton;
    juce::TextButton m_startButton;
    juce::TextButton m_clearLogButton;
    juce::Label m_thresholdLabel;
    juce::Slider m_thresholdSlider;
    juce::Label m_statusLabel;
    juce::Label m_topLabelsLabel;
    juce::TextEditor m_log;

    std::unique_ptr<juce::FileOutputStream> m_csv;
    bool m_running = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PannsLiveComponent)
};
