#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>

namespace Ort
{
    struct Env;
    struct Session;
}

class PannsSedModel
{
public:
    static constexpr int sampleRate = 32000;
    static constexpr int hopSize = 320;
    static constexpr int framesPerSecond = sampleRate / hopSize;
    static constexpr int numClasses = 527;

    PannsSedModel();
    ~PannsSedModel();

    bool load(const juce::File& modelFile, juce::String& errorMessage);
    bool isLoaded() const { return m_session != nullptr; }

    bool run(const float* audio,
        int numSamples,
        std::vector<float>& framewise,
        int& numFrames,
        juce::String& errorMessage);

private:
    std::unique_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::Session> m_session;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PannsSedModel)
};
