/*
  ==============================================================================

    AudioEngine.h
    Created: 8 May 2026 8:18:05am
    Author:  USER

  ==============================================================================
*/

#pragma once
#include "DeviceManager.h"
class IOAudioProcessor 
{
public:
    virtual ~IOAudioProcessor() = default;

    virtual void prepare(double sampleRate, int blockSize) = 0;

    virtual void process(const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples) = 0;

    virtual void reset() {}

    static void clearOutputs(float* const* outputChannelData,
        int numOutputChannels,
        int numSamples)
    {
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
        }
    }
};

class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    AudioEngine(DeviceManager& manager) :
        m_devManager(manager)
    {

    }

    ~AudioEngine() override
    {
        clearMode();
        stop();
    }

    void start()
    {
        if (!isRunning.exchange(true, std::memory_order_acq_rel))
            m_devManager.addAudioCallBack(this);
    }

    void stop()
    {
        if (isRunning.exchange(false, std::memory_order_acq_rel))
           m_devManager.removeAudioCallback(this);
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        sampleRate = device->getCurrentSampleRate();
        blockSize = device->getCurrentBufferSizeSamples();

        if (currentMode != nullptr)
            currentMode->prepare(sampleRate, blockSize);
    }

    void audioDeviceStopped() override
    {
        juce::ScopedLock lock(modeLock);

        if (currentMode != nullptr)
            currentMode->reset();
        currentMode.reset();
    }

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override
    {
        juce::ignoreUnused(context);
        clearOutputs(outputChannelData, numOutputChannels, numSamples);
        
        juce::ScopedLock lock(modeLock);
      
        if (currentMode)
            currentMode->process(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples);
    }

    void audioDeviceError(const juce::String& errorMessage) override
    {
        DBG(errorMessage);
    }

    void clearMode()
    {
        juce::ScopedLock lock(modeLock);

        if (currentMode != nullptr)
            currentMode->reset();

        currentMode.reset(nullptr);
    }

    template <typename ModeType, typename... Args>
    ModeType& setMode(Args&&... args)
    {
        
        juce::ScopedLock lock(modeLock);

        auto newMode = std::make_unique<ModeType>(std::forward<Args>(args)...);

        newMode->prepare(sampleRate, blockSize);

        auto& ref = *newMode;
        currentMode = std::move(newMode);

        return ref;
    }

    template <typename ModeType>
    ModeType* getMode()
    {
        return dynamic_cast<ModeType*>(currentMode.get());
    }

protected:
    static void clearOutputs(float* const* output,
        int numOutputs,
        int numSamples)
    {
        for (int ch = 0; ch < numOutputs; ++ch)
        {
            if (output[ch] != nullptr)
                juce::FloatVectorOperations::clear(output[ch], numSamples);
        }
    }

private:

    juce::CriticalSection modeLock;
    std::unique_ptr<IOAudioProcessor> currentMode{nullptr};
    DeviceManager& m_devManager;
    
    std::atomic<bool> isRunning{ false };
    double sampleRate = 44100.0;
    int blockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};