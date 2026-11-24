#include "PannsWorker.h"
#include <algorithm>
#include <cmath>
#include <numeric>

PannsWorker::PannsWorker()
    : juce::Thread("PANNs SED Worker")
{
    m_tracker.configure(PannsSedModel::numClasses,
        defaultThreshold,
        juce::roundToInt(minEventSeconds * PannsSedModel::framesPerSecond),
        juce::roundToInt(mergeGapSeconds * PannsSedModel::framesPerSecond));
}

PannsWorker::~PannsWorker()
{
    stop();
}

bool PannsWorker::loadModel(const juce::File& modelFile, const juce::File& labelsFile, juce::String& errorMessage)
{
    stop();

    if (!labelsFile.existsAsFile())
    {
        errorMessage = "Labels not found: " + labelsFile.getFullPathName();
        return false;
    }

    juce::StringArray labels;
    labelsFile.readLines(labels);
    labels.removeEmptyStrings();

    if (labels.size() != PannsSedModel::numClasses)
    {
        errorMessage = "Expected " + juce::String(PannsSedModel::numClasses)
            + " labels, found " + juce::String(labels.size());
        return false;
    }

    if (!m_model.load(modelFile, errorMessage))
        return false;

    m_labels = labels;
    return true;
}

bool PannsWorker::start(std::shared_ptr<PannsAudioFifo> fifo, juce::String& errorMessage)
{
    stop();

    if (!m_model.isLoaded())
    {
        errorMessage = "Model not loaded";
        return false;
    }

    if (fifo == nullptr)
    {
        errorMessage = "No audio input";
        return false;
    }

    m_fifo = std::move(fifo);

    m_inputRate = 0.0;
    m_ratio = 1.0;
    m_lowpass.clear();
    m_pending.clear();
    m_ring.assign(static_cast<size_t>(ringSamples), 0.0f);
    m_window.assign(static_cast<size_t>(windowSamples), 0.0f);
    m_totalResampled = 0;
    m_nextWindowEnd = windowSamples;
    m_nextFrame = 0;
    m_tracker.reset();
    m_lastInferenceMs.store(0.0f);

    {
        const juce::ScopedLock sl(m_outLock);
        m_events.clear();
        m_topLabels.clear();
        m_lastError.clear();
    }

    startThread();
    return true;
}

void PannsWorker::stop()
{
    if (isThreadRunning())
        stopThread(4000);

    m_fifo.reset();
}

void PannsWorker::popEvents(std::vector<PannsEvent>& dest)
{
    const juce::ScopedLock sl(m_outLock);
    dest.insert(dest.end(), m_events.begin(), m_events.end());
    m_events.clear();
}

juce::String PannsWorker::getCurrentTopLabels() const
{
    const juce::ScopedLock sl(m_outLock);
    return m_topLabels;
}

juce::String PannsWorker::getLastError() const
{
    const juce::ScopedLock sl(m_outLock);
    return m_lastError;
}

void PannsWorker::run()
{
    std::vector<float> block(4096);

    while (!threadShouldExit())
    {
        const double rate = m_fifo->getSampleRate();

        if (rate <= 0.0)
        {
            wait(10);
            continue;
        }

        if (rate != m_inputRate)
            configureResampler(rate);

        const int numRead = m_fifo->read(block.data(), static_cast<int>(block.size()));

        if (numRead == 0)
        {
            wait(5);
            continue;
        }

        processInput(block.data(), numRead);
    }

    std::vector<SedEvent> closed;
    m_tracker.flush(m_nextFrame, closed);
    publish(closed);
}

void PannsWorker::configureResampler(double inputRate)
{
    m_inputRate = inputRate;
    m_ratio = inputRate / PannsSedModel::sampleRate;
    m_interpolator.reset();
    m_pending.clear();
    m_lowpass.clear();

    if (inputRate > PannsSedModel::sampleRate * 1.001)
    {
        auto coefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
            0.45f * PannsSedModel::sampleRate, inputRate, 10);

        for (auto* c : coefficients)
            m_lowpass.emplace_back(c);
    }
}

void PannsWorker::processInput(float* data, int numSamples)
{
    for (auto& filter : m_lowpass)
        for (int i = 0; i < numSamples; ++i)
            data[i] = filter.processSample(data[i]);

    if (std::abs(m_ratio - 1.0) < 1.0e-9)
    {
        pushResampled(data, numSamples);
        return;
    }

    m_pending.insert(m_pending.end(), data, data + numSamples);

    const int available = static_cast<int>(m_pending.size());
    const int numOut = static_cast<int>(std::floor((available - 2) / m_ratio));

    if (numOut <= 0)
        return;

    m_resampled.resize(static_cast<size_t>(numOut));

    const int used = m_interpolator.process(m_ratio, m_pending.data(), m_resampled.data(), numOut);
    m_pending.erase(m_pending.begin(), m_pending.begin() + juce::jlimit(0, available, used));

    pushResampled(m_resampled.data(), numOut);
}

void PannsWorker::pushResampled(const float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        m_ring[static_cast<size_t>(m_totalResampled % ringSamples)] = data[i];
        ++m_totalResampled;

        if (m_totalResampled == m_nextWindowEnd)
        {
            runWindow(m_nextWindowEnd);
            m_nextWindowEnd += hopSamples;
        }
    }
}

void PannsWorker::runWindow(juce::int64 windowEnd)
{
    const juce::int64 windowStart = windowEnd - windowSamples;

    for (int i = 0; i < windowSamples; ++i)
        m_window[static_cast<size_t>(i)] = m_ring[static_cast<size_t>((windowStart + i) % ringSamples)];

    const double startTime = juce::Time::getMillisecondCounterHiRes();
    int numFrames = 0;
    juce::String error;

    if (!m_model.run(m_window.data(), windowSamples, m_framewise, numFrames, error))
    {
        setError(error);
        return;
    }

    m_lastInferenceMs.store(static_cast<float>(juce::Time::getMillisecondCounterHiRes() - startTime));

    if (numFrames < firstFrame + framesPerHop)
    {
        setError("Model returned " + juce::String(numFrames) + " frames");
        return;
    }

    const juce::int64 baseFrame = windowStart / PannsSedModel::hopSize;
    std::vector<SedEvent> closed;
    std::vector<float> peaks(static_cast<size_t>(PannsSedModel::numClasses), 0.0f);

    for (int k = firstFrame; k < firstFrame + framesPerHop; ++k)
    {
        const float* probs = m_framewise.data() + static_cast<size_t>(k) * PannsSedModel::numClasses;
        m_tracker.pushFrame(probs, baseFrame + k, closed);

        for (int c = 0; c < PannsSedModel::numClasses; ++c)
            peaks[static_cast<size_t>(c)] = juce::jmax(peaks[static_cast<size_t>(c)], probs[c]);
    }

    m_nextFrame = baseFrame + firstFrame + framesPerHop;

    std::vector<int> order(peaks.size());
    std::iota(order.begin(), order.end(), 0);
    std::partial_sort(order.begin(), order.begin() + 3, order.end(),
        [&peaks](int a, int b) { return peaks[static_cast<size_t>(a)] > peaks[static_cast<size_t>(b)]; });

    juce::StringArray top;
    for (int i = 0; i < 3; ++i)
        top.add(m_labels[order[static_cast<size_t>(i)]] + " " + juce::String(peaks[static_cast<size_t>(order[static_cast<size_t>(i)])], 2));

    {
        const juce::ScopedLock sl(m_outLock);
        m_topLabels = top.joinIntoString("  |  ");
    }

    publish(closed);
}

void PannsWorker::publish(const std::vector<SedEvent>& closed)
{
    if (closed.empty() || m_fifo == nullptr)
        return;

    const juce::int64 startMs = m_fifo->getStartMs();
    constexpr juce::int64 msPerFrame = 1000 / PannsSedModel::framesPerSecond;

    std::vector<PannsEvent> events;
    events.reserve(closed.size());

    for (const auto& e : closed)
    {
        PannsEvent ev;
        ev.label = juce::isPositiveAndBelow(e.classIndex, m_labels.size()) ? m_labels[e.classIndex]
                                                                           : juce::String(e.classIndex);
        ev.startMs = startMs + e.startFrame * msPerFrame;
        ev.endMs = startMs + e.endFrame * msPerFrame;
        ev.score = e.score;
        events.push_back(ev);
    }

    std::sort(events.begin(), events.end(),
        [](const PannsEvent& a, const PannsEvent& b) { return a.startMs < b.startMs; });

    const juce::ScopedLock sl(m_outLock);
    m_events.insert(m_events.end(), events.begin(), events.end());
}

void PannsWorker::setError(const juce::String& message)
{
    const juce::ScopedLock sl(m_outLock);
    m_lastError = message;
}
