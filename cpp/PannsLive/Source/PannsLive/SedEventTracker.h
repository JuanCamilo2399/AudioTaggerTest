#pragma once

#include <JuceHeader.h>
#include <vector>

struct SedEvent
{
    int classIndex = -1;
    juce::int64 startFrame = 0;
    juce::int64 endFrame = 0;
    float score = 0.0f;
};

class SedEventTracker
{
public:
    void configure(int numClasses, float threshold, int minDurationFrames, int mergeGapFrames)
    {
        m_threshold = threshold;
        m_minDurationFrames = minDurationFrames;
        m_mergeGapFrames = mergeGapFrames;
        m_states.assign(static_cast<size_t>(numClasses), ClassState{});
    }

    void setThreshold(float threshold) { m_threshold = threshold; }

    void reset()
    {
        for (auto& s : m_states)
            s = ClassState{};
    }

    void pushFrame(const float* probs, juce::int64 frame, std::vector<SedEvent>& closed)
    {
        const float threshold = m_threshold;

        for (int c = 0; c < static_cast<int>(m_states.size()); ++c)
        {
            auto& s = m_states[static_cast<size_t>(c)];
            const float p = probs[c];

            if (p >= threshold)
            {
                if (!s.active)
                {
                    s.active = true;
                    s.start = frame;
                    s.peak = p;
                }
                else
                {
                    s.peak = juce::jmax(s.peak, p);
                }
            }
            else if (s.active)
            {
                s.active = false;
                closeSegment(c, frame, closed);
            }

            if (s.hasPending)
            {
                const bool canStillMerge = s.active
                    ? (s.start - s.pending.endFrame <= m_mergeGapFrames)
                    : (frame + 1 - s.pending.endFrame <= m_mergeGapFrames);

                if (!canStillMerge)
                {
                    closed.push_back(s.pending);
                    s.hasPending = false;
                }
            }
        }
    }

    void flush(juce::int64 frame, std::vector<SedEvent>& closed)
    {
        for (int c = 0; c < static_cast<int>(m_states.size()); ++c)
        {
            auto& s = m_states[static_cast<size_t>(c)];

            if (s.active)
            {
                s.active = false;
                closeSegment(c, frame, closed);
            }

            if (s.hasPending)
            {
                closed.push_back(s.pending);
                s.hasPending = false;
            }
        }
    }

private:
    struct ClassState
    {
        bool active = false;
        juce::int64 start = 0;
        float peak = 0.0f;
        bool hasPending = false;
        SedEvent pending;
    };

    void closeSegment(int classIndex, juce::int64 endFrame, std::vector<SedEvent>& closed)
    {
        auto& s = m_states[static_cast<size_t>(classIndex)];

        if (endFrame - s.start < m_minDurationFrames)
            return;

        if (s.hasPending && s.start - s.pending.endFrame <= m_mergeGapFrames)
        {
            s.pending.endFrame = juce::jmax(s.pending.endFrame, endFrame);
            s.pending.score = juce::jmax(s.pending.score, s.peak);
            return;
        }

        if (s.hasPending)
            closed.push_back(s.pending);

        s.pending = { classIndex, s.start, endFrame, s.peak };
        s.hasPending = true;
    }

    std::vector<ClassState> m_states;
    std::atomic<float> m_threshold{ 0.15f };
    int m_minDurationFrames = 10;
    int m_mergeGapFrames = 5;
};
