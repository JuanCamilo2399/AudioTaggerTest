#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cmath>
#include <vector>

class PannsAudioFifo
{
public:
    static constexpr int capacity = 1 << 21;

    PannsAudioFifo() : m_fifo(capacity)
    {
        m_buffer.resize(static_cast<size_t>(capacity));
    }

    void setSampleRate(double sampleRate) { m_sampleRate.store(sampleRate); }
    double getSampleRate() const { return m_sampleRate.load(); }

    juce::int64 getStartMs() const { return m_startMs.load(); }
    juce::int64 getDroppedSamples() const { return m_dropped.load(); }
    float getAndResetPeak() { return m_peak.exchange(0.0f); }

    void writeMono(const float* const* inputs, int numInputs, int numSamples)
    {
        int numValid = 0;

        for (int ch = 0; ch < numInputs; ++ch)
            if (inputs[ch] != nullptr)
                ++numValid;

        if (numValid == 0 || numSamples <= 0)
            return;

        juce::int64 notStarted = -1;
        m_startMs.compare_exchange_strong(notStarted, juce::Time::currentTimeMillis());

        const float gain = 1.0f / static_cast<float>(numValid);
        int written = 0;
        float peak = 0.0f;

        auto fill = [&](int startIndex, int blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                float sum = 0.0f;

                for (int ch = 0; ch < numInputs; ++ch)
                    if (inputs[ch] != nullptr)
                        sum += inputs[ch][written + i];

                const float mono = sum * gain;
                peak = juce::jmax(peak, std::abs(mono));
                m_buffer[static_cast<size_t>(startIndex + i)] = mono;
            }

            written += blockSize;
        };

        {
            const auto scope = m_fifo.write(numSamples);
            fill(scope.startIndex1, scope.blockSize1);
            fill(scope.startIndex2, scope.blockSize2);
        }

        if (written < numSamples)
            m_dropped.fetch_add(numSamples - written);

        float previous = m_peak.load();
        while (peak > previous && !m_peak.compare_exchange_weak(previous, peak)) {}
    }

    int read(float* dest, int maxSamples)
    {
        const auto scope = m_fifo.read(juce::jmin(maxSamples, m_fifo.getNumReady()));

        if (scope.blockSize1 > 0)
            std::copy_n(m_buffer.data() + scope.startIndex1, scope.blockSize1, dest);

        if (scope.blockSize2 > 0)
            std::copy_n(m_buffer.data() + scope.startIndex2, scope.blockSize2, dest + scope.blockSize1);

        return scope.blockSize1 + scope.blockSize2;
    }

private:
    juce::AbstractFifo m_fifo;
    std::vector<float> m_buffer;
    std::atomic<double> m_sampleRate{ 0.0 };
    std::atomic<juce::int64> m_startMs{ -1 };
    std::atomic<juce::int64> m_dropped{ 0 };
    std::atomic<float> m_peak{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PannsAudioFifo)
};
