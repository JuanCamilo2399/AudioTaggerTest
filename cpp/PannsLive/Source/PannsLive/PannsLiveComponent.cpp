#include "PannsLiveComponent.h"
#include "PannsLiveInput.h"

namespace
{
    constexpr int noCameraId = 1;
    constexpr int firstCameraId = 2;
}

PannsLiveComponent::PannsLiveComponent(DeviceManager& deviceManager, AudioEngine& engine)
    : m_manager(deviceManager),
      m_engine(engine)
{
    m_cameraLabel.setText("Camera", juce::dontSendNotification);
    addAndMakeVisible(m_cameraLabel);

    addAndMakeVisible(m_cameraBox);
    m_cameraBox.onChange = [this]
    {
        const int id = m_cameraBox.getSelectedId();

        if (id >= firstCameraId)
            openCamera(id - firstCameraId);
        else
            closeCamera();
    };

    m_refreshCamerasButton.setButtonText("Refresh");
    m_refreshCamerasButton.onClick = [this] { refreshCameraList(); };
    addAndMakeVisible(m_refreshCamerasButton);

    m_startButton.setButtonText("Start");
    m_startButton.onClick = [this]
    {
        if (m_running)
            stopAnalysis();
        else
            startAnalysis();
    };
    addAndMakeVisible(m_startButton);

    m_clearLogButton.setButtonText("Clear log");
    m_clearLogButton.onClick = [this] { m_log.clear(); };
    addAndMakeVisible(m_clearLogButton);

    m_thresholdLabel.setText("Threshold", juce::dontSendNotification);
    addAndMakeVisible(m_thresholdLabel);

    m_thresholdSlider.setRange(0.05, 0.9, 0.01);
    m_thresholdSlider.setValue(PannsWorker::defaultThreshold, juce::dontSendNotification);
    m_thresholdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_thresholdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    m_thresholdSlider.onValueChange = [this]
    {
        m_worker.setThreshold(static_cast<float>(m_thresholdSlider.getValue()));
    };
    addAndMakeVisible(m_thresholdSlider);

    m_statusLabel.setText("Idle", juce::dontSendNotification);
    addAndMakeVisible(m_statusLabel);

    m_topLabelsLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(m_topLabelsLabel);

    m_log.setMultiLine(true);
    m_log.setReadOnly(true);
    m_log.setScrollbarsShown(true);
    m_log.setCaretVisible(false);
    m_log.setPopupMenuEnabled(true);
    m_log.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain)));
    addAndMakeVisible(m_log);

    refreshCameraList();
}

PannsLiveComponent::~PannsLiveComponent()
{
    stopTimer();
    m_worker.stop();
    m_csv.reset();
    closeCamera();
}

void PannsLiveComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    if (m_cameraViewer == nullptr && !m_cameraArea.isEmpty())
    {
        g.setColour(juce::Colours::black);
        g.fillRect(m_cameraArea);
        g.setColour(juce::Colours::grey);
        g.drawText("No camera", m_cameraArea, juce::Justification::centred);
    }
}

void PannsLiveComponent::resized()
{
    auto area = getLocalBounds().reduced(10);

    auto topRow = area.removeFromTop(30);
    m_cameraLabel.setBounds(topRow.removeFromLeft(60));
    m_cameraBox.setBounds(topRow.removeFromLeft(240));
    topRow.removeFromLeft(5);
    m_refreshCamerasButton.setBounds(topRow.removeFromLeft(70));
    topRow.removeFromLeft(20);
    m_startButton.setBounds(topRow.removeFromLeft(100));
    topRow.removeFromLeft(10);
    m_clearLogButton.setBounds(topRow.removeFromLeft(90));
    topRow.removeFromLeft(20);
    m_thresholdLabel.setBounds(topRow.removeFromLeft(75));
    m_thresholdSlider.setBounds(topRow.removeFromLeft(260));

    area.removeFromTop(5);
    m_statusLabel.setBounds(area.removeFromTop(22));
    m_topLabelsLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(5);

    m_log.setBounds(area.removeFromBottom(juce::roundToInt(area.getHeight() * 0.4f)));
    area.removeFromBottom(5);

    m_cameraArea = area;

    if (m_cameraViewer != nullptr)
        m_cameraViewer->setBounds(m_cameraArea);
}

void PannsLiveComponent::resetUIState()
{
    if (m_running)
        stopAnalysis();
}

void PannsLiveComponent::timerCallback()
{
    std::vector<PannsEvent> events;
    m_worker.popEvents(events);
    appendEvents(events);

    if (m_fifo == nullptr)
        return;

    const float peak = m_fifo->getAndResetPeak();
    const float peakDb = juce::Decibels::gainToDecibels(peak, -100.0f);

    juce::String status = "Input peak: " + juce::String(peakDb, 1) + " dB"
        + "   |   Inference: " + juce::String(m_worker.getLastInferenceMs(), 1) + " ms"
        + "   |   Dropped samples: " + juce::String(m_fifo->getDroppedSamples());

    const auto error = m_worker.getLastError();

    if (error.isNotEmpty())
        status += "   |   Error: " + error;

    m_statusLabel.setText(status, juce::dontSendNotification);
    m_topLabelsLabel.setText(m_worker.getCurrentTopLabels(), juce::dontSendNotification);
}

void PannsLiveComponent::startAnalysis()
{
    if (!ensureModelLoaded())
        return;

    const auto info = m_manager.requestDeviceInOutInfo();

    if (info.maxInputChannels <= 0)
    {
        m_statusLabel.setText("No active input channels (select one in Settings)", juce::dontSendNotification);
        return;
    }

    m_fifo = std::make_shared<PannsAudioFifo>();
    m_engine.setMode<PannsLiveInput>(m_fifo);

    m_worker.setThreshold(static_cast<float>(m_thresholdSlider.getValue()));

    juce::String error;

    if (!m_worker.start(m_fifo, error))
    {
        m_engine.clearMode();
        m_fifo.reset();
        m_statusLabel.setText(error, juce::dontSendNotification);
        return;
    }

    openCsvLog();
    appendLogLine("---- Started " + formatTime(juce::Time::currentTimeMillis(), true) + " ----");

    m_running = true;
    m_startButton.setButtonText("Stop");
    startTimerHz(10);
}

void PannsLiveComponent::stopAnalysis()
{
    stopTimer();

    m_engine.clearMode();
    m_worker.stop();

    std::vector<PannsEvent> events;
    m_worker.popEvents(events);
    appendEvents(events);

    appendLogLine("---- Stopped " + formatTime(juce::Time::currentTimeMillis(), true) + " ----");

    m_fifo.reset();
    m_csv.reset();

    m_running = false;
    m_startButton.setButtonText("Start");
    m_statusLabel.setText("Stopped", juce::dontSendNotification);
    m_topLabelsLabel.setText({}, juce::dontSendNotification);
}

bool PannsLiveComponent::ensureModelLoaded()
{
    if (m_worker.isModelLoaded())
        return true;

    const auto modelsFolder = getProjectRootFolder().getChildFile("Models");
    juce::String error;

    if (!m_worker.loadModel(modelsFolder.getChildFile("cnn14_decisionlevelmax.onnx"),
            modelsFolder.getChildFile("panns_labels.txt"),
            error))
    {
        m_statusLabel.setText(error, juce::dontSendNotification);
        return false;
    }

    return true;
}

void PannsLiveComponent::refreshCameraList()
{
    const int previousId = m_cameraBox.getSelectedId();

    m_cameraBox.clear(juce::dontSendNotification);
    m_cameraBox.addItem("No camera", noCameraId);

    const auto devices = juce::CameraDevice::getAvailableDevices();

    for (int i = 0; i < devices.size(); ++i)
        m_cameraBox.addItem(devices[i], firstCameraId + i);

    const bool previousStillValid = previousId >= firstCameraId && previousId < firstCameraId + devices.size();
    m_cameraBox.setSelectedId(previousStillValid ? previousId : noCameraId, juce::dontSendNotification);

    if (!previousStillValid)
        closeCamera();
}

void PannsLiveComponent::openCamera(int deviceIndex)
{
    closeCamera();

    m_camera.reset(juce::CameraDevice::openDevice(deviceIndex, 128, 64, 1280, 720, true));

    if (m_camera == nullptr)
    {
        m_statusLabel.setText("Could not open camera", juce::dontSendNotification);
        m_cameraBox.setSelectedId(noCameraId, juce::dontSendNotification);
        return;
    }

    m_cameraViewer.reset(m_camera->createViewerComponent());

    if (m_cameraViewer != nullptr)
    {
        addAndMakeVisible(*m_cameraViewer);
        m_cameraViewer->setBounds(m_cameraArea);
    }

    repaint();
}

void PannsLiveComponent::closeCamera()
{
    m_cameraViewer.reset();
    m_camera.reset();
    repaint();
}

void PannsLiveComponent::appendEvents(const std::vector<PannsEvent>& events)
{
    for (const auto& e : events)
    {
        const double durationSeconds = static_cast<double>(e.endMs - e.startMs) / 1000.0;

        appendLogLine(formatTime(e.startMs, true)
            + "  ->  " + formatTime(e.endMs, false)
            + "  |  " + e.label
            + "  |  " + juce::String(e.score, 2));

        if (m_csv != nullptr)
        {
            const juce::Time start(e.startMs);
            *m_csv << start.formatted("%Y-%m-%d") << ","
                   << formatTime(e.startMs, false) << ","
                   << formatTime(e.endMs, false) << ","
                   << juce::String(durationSeconds, 3) << ","
                   << e.label.quoted() << ","
                   << juce::String(e.score, 3) << "\n";
            m_csv->flush();
        }
    }
}

void PannsLiveComponent::appendLogLine(const juce::String& line)
{
    m_log.moveCaretToEnd();
    m_log.insertTextAtCaret(line + juce::newLine);
}

void PannsLiveComponent::openCsvLog()
{
    m_csv.reset();

    const auto logsFolder = getProjectRootFolder().getChildFile("Logs");

    if (!logsFolder.exists())
        logsFolder.createDirectory();

    const auto file = logsFolder.getChildFile("panns_events_"
        + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S") + ".csv");

    m_csv = file.createOutputStream();

    if (m_csv == nullptr)
    {
        appendLogLine("Could not create " + file.getFullPathName());
        return;
    }

    *m_csv << "date,start_time,end_time,duration_s,label,score\n";
    m_csv->flush();
    appendLogLine("CSV: " + file.getFullPathName());
}

juce::File PannsLiveComponent::getProjectRootFolder()
{
    // Carpeta que contiene Models/: junto al ejecutable (bin/) o en alguna carpeta superior
    // (por ejemplo cpp/PannsLive cuando se ejecuta desde la carpeta de compilación).
    const auto exeDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();

    for (auto dir = exeDir;; dir = dir.getParentDirectory())
    {
        if (dir.getChildFile("Models").getChildFile("panns_labels.txt").existsAsFile())
            return dir;

        if (dir.getParentDirectory() == dir)
            break;
    }

    return exeDir;
}

juce::String PannsLiveComponent::formatTime(juce::int64 ms, bool withDate)
{
    const juce::Time t(ms);
    return t.formatted(withDate ? "%Y-%m-%d %H:%M:%S" : "%H:%M:%S")
        + "." + juce::String(t.getMilliseconds()).paddedLeft('0', 3);
}
