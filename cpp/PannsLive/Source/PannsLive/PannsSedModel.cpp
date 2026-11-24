#include "PannsSedModel.h"
#include <onnxruntime_cxx_api.h>
#include <array>

PannsSedModel::PannsSedModel() = default;

PannsSedModel::~PannsSedModel()
{
    m_session.reset();
    m_env.reset();
}

bool PannsSedModel::load(const juce::File& modelFile, juce::String& errorMessage)
{
    m_session.reset();

    if (!modelFile.existsAsFile())
    {
        errorMessage = "Model not found: " + modelFile.getFullPathName();
        return false;
    }

    try
    {
        if (m_env == nullptr)
            m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "PannsLive");

        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(juce::jmax(1, juce::SystemStats::getNumPhysicalCpus() / 2));
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        const juce::String fullPath = modelFile.getFullPathName();

#if JUCE_WINDOWS
        m_session = std::make_unique<Ort::Session>(*m_env, fullPath.toWideCharPointer(), options);
#else
        m_session = std::make_unique<Ort::Session>(*m_env, fullPath.toRawUTF8(), options);
#endif
    }
    catch (const Ort::Exception& e)
    {
        errorMessage = e.what();
        m_session.reset();
        return false;
    }

    return true;
}

bool PannsSedModel::run(const float* audio,
    int numSamples,
    std::vector<float>& framewise,
    int& numFrames,
    juce::String& errorMessage)
{
    numFrames = 0;

    if (m_session == nullptr)
    {
        errorMessage = "Model not loaded";
        return false;
    }

    try
    {
        const std::array<int64_t, 2> shape{ 1, static_cast<int64_t>(numSamples) };
        auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        auto input = Ort::Value::CreateTensor<float>(memoryInfo,
            const_cast<float*>(audio),
            static_cast<size_t>(numSamples),
            shape.data(),
            shape.size());

        const char* inputNames[] = { "waveform" };
        const char* outputNames[] = { "framewise_output" };

        auto outputs = m_session->Run(Ort::RunOptions{ nullptr }, inputNames, &input, 1, outputNames, 1);

        const auto outShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();

        if (outShape.size() != 3 || outShape[2] != numClasses)
        {
            errorMessage = "Unexpected model output shape";
            return false;
        }

        numFrames = static_cast<int>(outShape[1]);
        const float* data = outputs[0].GetTensorData<float>();
        framewise.assign(data, data + static_cast<size_t>(numFrames) * numClasses);
    }
    catch (const Ort::Exception& e)
    {
        errorMessage = e.what();
        return false;
    }

    return true;
}
