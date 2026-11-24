#pragma once

#include <JuceHeader.h>
#include "PannsAudioFifo.h"
#include "PannsSedModel.h"
#include "SedEventTracker.h"
#include <memory>
#include <vector>

struct PannsEvent
{
    juce::String label;
    juce::int64 startMs = 0;
    juce::int64 endMs = 0;
    float score = 0.0f;
};

class PannsWorker : private juce::Thread
{
public:
    static constexpr int windowSamples = 2 * PannsSedModel::sampleRate;
    static constexpr int hopSamples = PannsSedModel::sampleRate / 2;
    static constexpr int delaySamples = PannsSedModel::sampleRate / 2;
    static constexpr int ringSamples = windowSamples + 2 * hopSamples;
    static constexpr int firstFrame = (windowSamples - hopSamples - delaySamples) / PannsSedModel::hopSize;
    static constexpr int framesPerHop = hopSamples / PannsSedModel::hopSize;

    static constexpr float defaultThreshold = 0.15f;
    static constexpr float minEventSeconds = 0.10f;
    static constexpr float mergeGapSeconds = 0.05f;

    PannsWorker();
    ~PannsWorker() override;

    bool loadModel(const juce::File& modelFile, const juce::File& labelsFile, juce::String& errorMessage);
    bool isModelLoaded() const { return m_model.isLoaded(); }

    bool start(std::shared_ptr<PannsAudioFifo> fifo, juce::String& errorMessage);
    void stop();
    bool isRunning() const { return isThreadRunning(); }

    void setThreshold(float threshold) { m_tracker.setThreshold(threshold); }

    void popEvents(std::vector<PannsEvent>& dest);
    juce::String getCurrentTopLabels() const;
    juce::String getLastError() const;
    float getLastInferenceMs() const { return m_lastInferenceMs.load(); }

private:
    void run() override;
    void configureResampler(double inputRate);
    void processInput(float* data, int numSamples);
    void pushResampled(const float* data, int numSamples);
    void runWindow(juce::int64 windowEnd);
    void publish(const std::vector<SedEvent>& closed);
    void setError(const juce::String& message);

    PannsSedModel m_model;
    juce::StringArray m_labels;
    SedEventTracker m_tracker;
    std::shared_ptr<PannsAudioFifo> m_fifo;

    double m_inputRate = 0.0;
    double m_ratio = 1.0;
    std::vector<juce::dsp::IIR::Filter<float>> m_lowpass;
    juce::WindowedSincInterpolator m_interpolator;
    std::vector<float> m_pending;
    std::vector<float> m_resampled;

    std::vector<float> m_ring;
    std::vector<float> m_window;
    std::vector<float> m_framewise;
    juce::int64 m_totalResampled = 0;
    juce::int64 m_nextWindowEnd = windowSamples;
    juce::int64 m_nextFrame = 0;

    mutable juce::CriticalSection m_outLock;
    std::vector<PannsEvent> m_events;
    juce::String m_topLabels;
    juce::String m_lastError;
    std::atomic<float> m_lastInferenceMs{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PannsWorker)
};
