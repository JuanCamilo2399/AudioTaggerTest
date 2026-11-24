#pragma once

#include "../AudioEngine/AudioEngine.h"
#include "PannsAudioFifo.h"
#include <memory>

class PannsLiveInput : public IOAudioProcessor
{
public:
    explicit PannsLiveInput(std::shared_ptr<PannsAudioFifo> fifo)
        : m_fifo(std::move(fifo))
    {
    }

    void prepare(double sampleRate, int blockSize) override
    {
        juce::ignoreUnused(blockSize);

        if (m_fifo != nullptr)
            m_fifo->setSampleRate(sampleRate);
    }

    void process(const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples) override
    {
        juce::ignoreUnused(outputChannelData, numOutputChannels);

        if (m_fifo != nullptr)
            m_fifo->writeMono(inputChannelData, numInputChannels, numSamples);
    }

private:
    std::shared_ptr<PannsAudioFifo> m_fifo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PannsLiveInput)
};
