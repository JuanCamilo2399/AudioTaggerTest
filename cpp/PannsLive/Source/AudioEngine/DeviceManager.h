/*
  ==============================================================================

    DeviceManager.h
    Created: 6 May 2026 3:34:29pm
    Author:  JNUNEZ

  ==============================================================================
*/

#pragma once
#include<JuceHeader.h>
namespace ChannelBits
{
    static constexpr int maxChannelsToCheck = 32;
    
    //==============================================================================
    static bool isChannelActive(const juce::BigInteger& bits, int channelIndex)
    {
        if (channelIndex < 0 || channelIndex >= maxChannelsToCheck)
            return false;

        return bits[channelIndex];
    }

    //==============================================================================
    static int getFirstActiveChannel(const juce::BigInteger& bits,
        int maxChannels = maxChannelsToCheck)
    {
        maxChannels = juce::jlimit(0, maxChannelsToCheck, maxChannels);

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            if (bits[ch])
                return ch;
        }

        return -1;
    }

    //==============================================================================
    static int getLastActiveChannel(const juce::BigInteger& bits,
        int maxChannels = maxChannelsToCheck)
    {
        maxChannels = juce::jlimit(0, maxChannelsToCheck, maxChannels);

        for (int ch = maxChannels - 1; ch >= 0; --ch)
        {
            if (bits[ch])
                return ch;
        }

        return -1;
    }

    //==============================================================================
    static int getNumActiveChannels(const juce::BigInteger& bits,
        int maxChannels = maxChannelsToCheck)
    {
        maxChannels = juce::jlimit(0, maxChannelsToCheck, maxChannels);

        int count = 0;

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            if (bits[ch])
                ++count;
        }

        return count;
    }

    //==============================================================================
    static juce::Array<int> getActiveChannels(const juce::BigInteger& bits,
        int maxChannels = maxChannelsToCheck)
    {
        maxChannels = juce::jlimit(0, maxChannelsToCheck, maxChannels);

        juce::Array<int> activeChannels;

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            if (bits[ch])
                activeChannels.add(ch);
        }

        return activeChannels;
    }

    //==============================================================================
    static juce::String getListOfActiveBits(const juce::BigInteger& b)
    {
        juce::StringArray bits;

        for (auto i = 0; i <= b.getHighestBit(); ++i)
            if (b[i])
                bits.add(juce::String(i));

        return bits.joinIntoString(", ");
    }

    //==============================================================================
    static bool hasAnyActiveChannel(const juce::BigInteger& bits,
        int maxChannels = maxChannelsToCheck)
    {
        return getFirstActiveChannel(bits, maxChannels) != -1;
    }

    //==============================================================================
    static bool hasOnlyOneActiveChannel(const juce::BigInteger& bits,
        int maxChannels = maxChannelsToCheck)
    {
        return getNumActiveChannels(bits, maxChannels) == 1;
    }

}


class DeviceManager
{
public:
    DeviceManager()
    {
    }

    void initialise(int inputChannels, int outputChannels)
    {
        auto audioError = m_manager.initialise(inputChannels, outputChannels, nullptr, true, {}, nullptr);
        jassert(audioError.isEmpty());
    }

    struct DeviceInfo
    {
        juce::String deviceName;
        juce::String currentFs;
        juce::String blockSize;
        juce::String bitDepth;
        juce::String inputChannels;
        juce::String ActiveInputChannels;
        juce::String outputChannels;
        juce::String ActiveOutputChannels;
    };

    struct DeviceInOutInfo
    {
        int maxInputChannels = 0;
        int maxOutputChannels = 0;
        juce::BigInteger activeInputChannels{0};
        juce::BigInteger activeOutputChannels{0};
    };

    void addChangeListener(juce::ChangeListener* listener)
    {
        m_manager.addChangeListener(listener);
    }
    void removeChangeListener(juce::ChangeListener* listener)
    {
        m_manager.removeChangeListener(listener);
    }

    void addAudioCallBack(juce::AudioIODeviceCallback& newCallback)
    {
        m_manager.addAudioCallback(&newCallback);
    }

    void removeAudioCallback(juce::AudioIODeviceCallback& callback)
    {
        m_manager.removeAudioCallback(&callback);
    }

    void addAudioCallBack(juce::AudioIODeviceCallback* newCallback)
    {
        m_manager.addAudioCallback(newCallback);
    }

    void removeAudioCallback(juce::AudioIODeviceCallback* callback)
    {
        m_manager.removeAudioCallback(callback);
    }

    DeviceInOutInfo requestDeviceInOutInfo()
    {
        DeviceInOutInfo info;

        if (auto* device = m_manager.getCurrentAudioDevice())
        {
            info.activeInputChannels = device->getActiveInputChannels();
            info.activeOutputChannels = device->getActiveOutputChannels();

            info.maxInputChannels = info.activeInputChannels.countNumberOfSetBits();
            info.maxOutputChannels = info.activeOutputChannels.countNumberOfSetBits();
        }

        return info;
    }

    DeviceInfo requestDeviceInfo()
    {
        DeviceInfo info;

        if (auto* device = m_manager.getCurrentAudioDevice())
        {
            info.deviceName = device->getName().quoted();
            info.currentFs = juce::String(device->getCurrentSampleRate()) + " Hz";
            info.blockSize = juce::String(device->getCurrentBufferSizeSamples()) + " samples";
            info.bitDepth = juce::String(device->getCurrentBitDepth());
            info.inputChannels = device->getInputChannelNames().joinIntoString(", ");
            info.ActiveInputChannels = ChannelBits::getListOfActiveBits(device->getActiveInputChannels());
            info.outputChannels = device->getOutputChannelNames().joinIntoString(", ");
            info.ActiveOutputChannels = ChannelBits::getListOfActiveBits(device->getActiveOutputChannels());
           
        }
        
        return info;
    }

    double getCpuUsage()
    {
        return m_manager.getCpuUsage() * 100;
    }

    juce::AudioDeviceManager& getAudioDeviceManager() { return m_manager; }

private:
    juce::AudioDeviceManager m_manager;
};