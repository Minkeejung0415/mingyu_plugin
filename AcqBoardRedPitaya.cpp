/*
    ------------------------------------------------------------------

    This file is part of the Open Ephys GUI

    ------------------------------------------------------------------
*/

#include "Acqboardredpitaya.h"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

AcqBoardRedPitaya::AcqBoardRedPitaya()
    : AcquisitionBoard()
{
    boardType = BoardType::RedPitaya;

    // Default sample rate; can be overridden with setSampleRate().
    settings.boardSampleRate = 1000.0f;

    // By default, expose ADC channels only.
    acquireAdc = true;
    acquireAux = false;
}

AcqBoardRedPitaya::~AcqBoardRedPitaya()
{
    stopAcquisition();
}
/*
bool AcqBoardRedPitaya::detectBoard()
{
    std::cout << "detectBoard called" << std::endl;

    deviceFound = false;
    StreamingSocket socket;

    // Use a slightly larger timeout for discovery since the RP might be scanning AXI
    if (! socket.connect ("10.19.172.233", 5000, 1000))
    {
        std::cout << "connect failed" << std::endl;
        return false;
    }

    std::cout << "connected" << std::endl;

    const char* msg = "REDPITAYA\n";
    socket.write (msg, (int) strlen (msg));

    if (! socket.waitUntilReady (true, 500))
    {
        std::cout << "no reply" << std::endl;
        socket.close();
        return false;
    }

    // Increased buffer size to catch "OK CHANNELS:XX"
    char buffer[64] = { 0 };
    int n = socket.read (buffer, sizeof (buffer) - 1, false);

    if (n > 0)
    {
        buffer[n] = '\0';
        String response (buffer);
        std::cout << "Received response: " << response << std::endl;

        // Check for the "OK" flag
        if (response.contains ("OK"))
        {
            // Parse the channel count
            if (response.contains ("CHANNELS:"))
            {
                int startIdx = response.indexOf ("CHANNELS:") + 9;

                // substring(startIdx) gets everything after the colon
                // getIntValue() stops parsing at the newline/space
                this->numAdcChannels = response.substring (startIdx).getIntValue();

                std::cout << "Detected Red Pitaya with " << numAdcChannels << " channels." << std::endl;
                deviceFound = true;
            }
            else
            {
                // Fallback for legacy code if needed
                this->numAdcChannels = 6;
                deviceFound = true;
            }
        }
    }

    socket.close();
    return deviceFound;
} */

bool AcqBoardRedPitaya::detectBoard()
{
    std::cout << "detectBoard called" << std::endl;

    deviceFound = false;

    if (commandSocket != nullptr && ! commandSocket->isConnected())
    {
        delete commandSocket;
        commandSocket = nullptr;
    }

    if (commandSocket == nullptr)
        commandSocket = new StreamingSocket();

    if (! commandSocket->connect ("10.19.172.233", 5000, 1000))
    {
        std::cout << "Red Pitaya: simulation mode active (hardware not reachable at 10.19.172.233:5000)" << std::endl;
        delete commandSocket;
        commandSocket  = nullptr;
        simulationMode = true;
        deviceFound    = true;
        numAdcChannels = fakeImuCount * 6;
        std::cout << "Red Pitaya: simulation mode using numAdcChannels = " << numAdcChannels
                  << " (" << fakeImuCount << " IMUs)" << std::endl;
        return true;
    }

    simulationMode = false;
    std::cout << "Red Pitaya: TCP connected to 10.19.172.233:5000" << std::endl;

    const char* msg = "REDPITAYA\n";
    commandSocket->write (msg, (int) strlen (msg));

    if (! commandSocket->waitUntilReady (true, 500))
    {
        std::cout << "Red Pitaya: no handshake reply — aborting detection" << std::endl;
        commandSocket->close();
        delete commandSocket;
        commandSocket = nullptr;
        return false;
    }

    // Increased buffer size to catch "OK CHANNELS:XX"
    char buffer[64] = { 0 };
    int n = commandSocket->read (buffer, sizeof (buffer) - 1, false);

    if (n > 0)
    {
        buffer[n] = '\0';
        String response (buffer);
        std::cout << "Red Pitaya: handshake response: " << response << std::endl;

        if (response.contains ("OK"))
        {
            if (response.contains ("CHANNELS:"))
            {
                int startIdx = response.indexOf ("CHANNELS:") + 9;
                this->numAdcChannels = response.substring (startIdx).getIntValue();
                deviceFound = true;
            }
            else
            {
                this->numAdcChannels = 6;
                deviceFound = true;
            }

            std::cout << "Red Pitaya: REAL hardware connected, numAdcChannels = "
                      << numAdcChannels << std::endl;
        }
    }
    if (! deviceFound)
    {
        commandSocket->close();
        delete commandSocket;
        commandSocket = nullptr;
    }

    return deviceFound;
}

bool AcqBoardRedPitaya::initializeBoard()
{
    if (! deviceFound)
        return false;

    // Any Red Pitaya-specific configuration (e.g. network setup) can be
    // performed here. At this stage we simply consider the device ready.

    return true;
}

bool AcqBoardRedPitaya::foundInputSource() const
{
    return deviceFound;
}

Array<const Headstage*> AcqBoardRedPitaya::getHeadstages()
{
    // Red Pitaya is modeled as an ADC-only device with no headstages.
    Array<const Headstage*> none;
    return none;
}

Array<int> AcqBoardRedPitaya::getAvailableSampleRates()
{
    Array<int> sampleRates;

    sampleRates.add (100);
    sampleRates.add (250);
    sampleRates.add (500);
    sampleRates.add (1000);
    sampleRates.add (2000);

    return sampleRates;
}

void AcqBoardRedPitaya::setSampleRate (int sampleRateHz)
{
    settings.boardSampleRate = static_cast<float> (sampleRateHz);
}

float AcqBoardRedPitaya::getSampleRate() const
{
    return settings.boardSampleRate;
}

void AcqBoardRedPitaya::scanPorts()
{
    // Re-run hardware detection so the RESCAN button can correct the channel
    // count if Open Ephys was opened before the Red Pitaya server was running.
    std::cout << "Red Pitaya: scanPorts() — retrying hardware detection..." << std::endl;
    detectBoard();
    std::cout << "Red Pitaya: numAdcChannels = " << numAdcChannels
              << (simulationMode ? " (simulation mode)" : " (REAL hardware)") << std::endl;
}

void AcqBoardRedPitaya::enableAuxChannels (bool enabled)
{
    acquireAux = enabled;
}

bool AcqBoardRedPitaya::areAuxChannelsEnabled() const
{
    return acquireAux;
}

void AcqBoardRedPitaya::enableAdcChannels (bool enabled)
{
    acquireAdc = enabled;
}

bool AcqBoardRedPitaya::areAdcChannelsEnabled() const
{
    return acquireAdc;
}

float AcqBoardRedPitaya::getBitVolts (ContinuousChannel::Type channelType) const
{
    if (channelType == ContinuousChannel::ADC)
        return adcBitVolts;

    return 1.0f;
}

void AcqBoardRedPitaya::measureImpedances()
{
}

void AcqBoardRedPitaya::impedanceMeasurementFinished()
{
}

void AcqBoardRedPitaya::saveImpedances (File& /*file*/)
{
}

void AcqBoardRedPitaya::setNamingScheme (ChannelNamingScheme scheme)
{
    channelNamingScheme = scheme;
}

ChannelNamingScheme AcqBoardRedPitaya::getNamingScheme()
{
    return channelNamingScheme;
}

bool AcqBoardRedPitaya::isReady()
{
    return deviceFound;
}

bool AcqBoardRedPitaya::startAcquisition()
{
    if (! deviceFound)
        return false;

    streamSensorNames.clear();

    if (isThreadRunning())
    {
        std::cout << "Red Pitaya WARNING: previous reader thread still running; stopping before restart." << std::endl;
        signalThreadShouldExit();

        if (commandSocket != nullptr)
            commandSocket->close();

        stopThread (2000);

        if (isThreadRunning())
        {
            std::cout << "Red Pitaya ERROR: previous reader thread did not stop; refusing to start a new acquisition." << std::endl;
            return false;
        }
    }

    // If we started in simulation mode, retry real hardware now —
    // the board may have been connected after Open Ephys launched.
    if (simulationMode)
    {
        std::cout << "Red Pitaya: retrying hardware connection before acquisition..." << std::endl;
        detectBoard();   // updates simulationMode, deviceFound, numAdcChannels
    }

    std::cout << "Red Pitaya: using numAdcChannels = " << numAdcChannels << std::endl;

    if (simulationMode)
    {
        std::cout << "Red Pitaya: simulation mode active — generating synthetic data" << std::endl;
    }
    else
    {
        std::cout << "Red Pitaya: REAL hardware mode" << std::endl;

        // Always use a new TCP session for each acquisition so the board's command
        // loop never inherits stale RX data from a previous stream.
        if (commandSocket != nullptr)
        {
            commandSocket->close();
            delete commandSocket;
            commandSocket = nullptr;
        }

        commandSocket = new StreamingSocket();
        if (! commandSocket->connect ("10.19.172.233", 5000, 1000))
        {
            std::cout << "Red Pitaya ERROR: Could not connect to board." << std::endl;
            delete commandSocket;
            commandSocket = nullptr;
            return false;
        }

        {
            char freqMsg[32];
            int targetHz = jlimit (1, 2000, (int) settings.boardSampleRate);
            snprintf (freqMsg, sizeof (freqMsg), "FREQ:%d\n", targetHz);
            commandSocket->write (freqMsg, (int) strlen (freqMsg));
        }

        const char* msg = "START\n";
        commandSocket->write (msg, (int) strlen (msg));

        lastRecordingPath = {};
        lastRecordingCsvPath = {};

        String responseText;

        while (commandSocket->waitUntilReady (true, 1000))
        {
            char c = 0;
            if (commandSocket->read (&c, 1, false) <= 0 || c == '\n')
                break;

            responseText += c;
        }

        auto failStartAndResetSocket = [this]()
        {
            if (commandSocket != nullptr)
            {
                commandSocket->close();
                delete commandSocket;
                commandSocket = nullptr;
            }
        };

        if (responseText.startsWith ("ERROR_FILE"))
        {
            std::cout << "Red Pitaya ERROR: Server could not open recording files." << std::endl;
            failStartAndResetSocket();
            return false;
        }

        if (! (responseText == "STARTED" || responseText.startsWith ("STARTED ")))
        {
            std::cout << "Red Pitaya ERROR: Expected STARTED from board, got: "
                      << (responseText.isEmpty() ? String ("(empty / timeout)") : responseText) << std::endl;
            failStartAndResetSocket();
            return false;
        }

        if (responseText == "STARTED")
        {
            std::cout << "Red Pitaya: Streaming started." << std::endl;
        }
        else
        {
            const String pathText = responseText.fromFirstOccurrenceOf ("STARTED ", false, false).trim();

            if (pathText.contains ("BIN:") && pathText.contains (" CSV:"))
            {
                lastRecordingPath = pathText.fromFirstOccurrenceOf ("BIN:", false, false)
                                        .upToFirstOccurrenceOf (" CSV:", false, false)
                                        .trim();
                lastRecordingCsvPath = pathText.fromFirstOccurrenceOf (" CSV:", false, false).trim();
            }
            else
            {
                lastRecordingPath = pathText;
            }

            std::cout << "Red Pitaya: Streaming to " << lastRecordingPath;
            if (lastRecordingCsvPath.isNotEmpty())
                std::cout << " and " << lastRecordingCsvPath;
            std::cout << std::endl;
        }

        // Second line: SENSORS:0,Name;1,Name2
        {
            String sensorsLine;
            while (commandSocket->waitUntilReady (true, 1000))
            {
                char c = 0;
                if (commandSocket->read (&c, 1, false) <= 0 || c == '\n')
                    break;
                sensorsLine += c;
            }

            if (sensorsLine.startsWith ("SENSORS:"))
            {
                const String body = sensorsLine.fromFirstOccurrenceOf ("SENSORS:", false, false).trim();
                StringArray segments;
                segments.addTokens (body, ";", "");

                for (int si = 0; si < segments.size(); ++si)
                {
                    const String seg = segments[si].trim();
                    const int comma = seg.indexOfChar (',');

                    if (comma > 0)
                        streamSensorNames.add (seg.substring (comma + 1).trim());
                    else if (seg.isNotEmpty())
                        streamSensorNames.add (seg);
                }
            }
        }

        {
            const char* filterMsg = filterEnabled ? "FILTER ON\n" : "FILTER OFF\n";
            commandSocket->write (filterMsg, (int) strlen (filterMsg));
        }
    }

    // Auto-connect OpenSim UDP forwarding to Python bridge on 127.0.0.1:5000
    openSimSocket  = std::make_unique<DatagramSocket>();
    openSimEnabled = true;
    std::cout << "Red Pitaya: OpenSim UDP forwarding enabled -> 127.0.0.1:5000" << std::endl;

    startThread();
    return true;
}

bool AcqBoardRedPitaya::stopAcquisition()
{
    // 1. Ask run() to exit on its next poll.
    if (isThreadRunning())
        signalThreadShouldExit();

    // 2. Close the TCP connection. The Red Pitaya server then sees EOF /
    //    send failure and leaves run_stream; we must NOT write STOP (or anything)
    //    on this socket while run() is still consuming the same byte stream as
    //    binary packets — that corrupts framing and leaves stale bytes for the
    //    next START (classic "second start works" failure).
    if (commandSocket != nullptr)
        commandSocket->close();

    // 3. Wait for run() to finish before deleting the socket object.
    //    The backend can keep streaming if the old reader/socket lifecycle does
    //    not complete cleanly before Open Ephys starts a new acquisition.
    stopThread (2000);

    if (isThreadRunning())
    {
        std::cout << "Red Pitaya WARNING: reader thread did not stop; keeping socket object alive." << std::endl;
        return false;
    }

    if (commandSocket != nullptr)
    {
        delete commandSocket;
        commandSocket = nullptr;
    }

    openSimEnabled = false;
    openSimSocket.reset();

    if (openSimLiveProcess != nullptr)
    {
        openSimLiveProcess->kill();
        openSimLiveProcess.reset();
    }

    streamSensorNames.clear();

    return true;
}
bool AcqBoardRedPitaya::sendRecordOnCommand()
{
    if (commandSocket == nullptr)
        return false;

    String msg;

    if (lastRecordingPath.isNotEmpty())
    {
        msg = "RECORD ON\n";
    }
    else
    {
        const String basePath = "/root/Measurements/recording_" + String (Time::currentTimeMillis());
        lastRecordingPath = basePath + ".bin";
        lastRecordingCsvPath = basePath + ".csv";
        msg = "RECORD ON " + basePath + "\n";
    }

    int written = commandSocket->write (msg.toRawUTF8(), (int) msg.getNumBytesAsUTF8());

    if (written <= 0)
        return false;

    return true;
}

bool AcqBoardRedPitaya::sendRecordOffCommand()
{
    if (commandSocket == nullptr)
        return false;

    const char* msg = "RECORD OFF\n";

    int written = commandSocket->write (msg, (int) strlen (msg));

    if (written <= 0)
        return false;

    return true;
}

void AcqBoardRedPitaya::updateSampleFrequency (int newFreq)
{
    if (! deviceFound)
        return;

    int targetHz = jlimit (1, 2000, newFreq);
    settings.boardSampleRate = static_cast<float> (targetHz);

    // 1. SELF-HEALING: Trash dead sockets
    if (commandSocket != nullptr && ! commandSocket->isConnected())
    {
        commandSocket->close();
        delete commandSocket;
        commandSocket = nullptr;
    }

    // 2. SELF-HEALING: Build a new connection if needed
    if (commandSocket == nullptr)
    {
        commandSocket = new StreamingSocket();

        if (! commandSocket->connect ("10.19.172.233", 5000, 1000))
        {
            std::cout << "Red Pitaya ERROR: Could not connect to board." << std::endl;
            delete commandSocket;
            commandSocket = nullptr;
            return;
        }
    }

    // 3. FIRE AND FORGET
    char msg[32];
    snprintf (msg, sizeof (msg), "FREQ:%d\n", targetHz);

    int written = commandSocket->write (msg, (int) strlen (msg));

    if (written > 0)
    {
        std::cout << "Red Pitaya: Sent command -> " << msg;
    }
    else
    {
        std::cout << "Red Pitaya Backend ERROR: Socket write failed." << std::endl;
        commandSocket->close();
    }
}

void AcqBoardRedPitaya::setFilterEnabled (bool enabled)
{
    if (commandSocket == nullptr)
        return;

    const char* msg = enabled ? "FILTER ON\n" : "FILTER OFF\n";

    int written = commandSocket->write (msg, (int) strlen (msg));

    if (written > 0)
        std::cout << "Red Pitaya: Sent command -> " << msg;
    else
        std::cout << "Red Pitaya Backend ERROR: Socket write failed." << std::endl;

    filterEnabled = enabled;
}

void AcqBoardRedPitaya::setAnalogInGain (float gain)
{
    if (commandSocket == nullptr)
        return;

    char msg[32];
    snprintf (msg, sizeof (msg), "AIN_GAIN:%.2f\n", gain);

    int written = commandSocket->write (msg, (int) strlen (msg));

    if (written > 0)
        std::cout << "Red Pitaya: Sent command -> " << msg;
    else
        std::cout << "Red Pitaya Backend ERROR: Socket write failed." << std::endl;

    analogInGain = gain;
}

void AcqBoardRedPitaya::setAnalogOutVoltage (float voltage)
{
    if (commandSocket == nullptr)
        return;

    char msg[32];
    snprintf (msg, sizeof (msg), "AOUT:%.2f\n", voltage);

    int written = commandSocket->write (msg, (int) strlen (msg));

    if (written > 0)
        std::cout << "Red Pitaya: Sent command -> " << msg;
    else
        std::cout << "Red Pitaya Backend ERROR: Socket write failed." << std::endl;

    analogOutVoltage = voltage;
}

String AcqBoardRedPitaya::getStreamSensorName (int index) const
{
    if (index < 0 || index >= streamSensorNames.size())
        return {};
    return streamSensorNames.getReference (index);
}

/* LSB per physical unit for each preset index (matches server-side tables).
 * ACC: LSB per g.   GYR: LSB per °/s. */
static const float kAccSensitivity[4] = { 16384.0f, 8192.0f, 4096.0f, 2048.0f };
static const float kGyrSensitivity[4] = { 131.072f, 65.536f, 32.768f, 16.384f };

bool AcqBoardRedPitaya::sendSensorCfgAcc (int sensorIndex, int presetId)
{
    if (sensorIndex >= 0 && sensorIndex < 6)
        sensorAccPreset[sensorIndex] = presetId;

    if (commandSocket == nullptr)
        return false;

    char msg[48];
    snprintf (msg, sizeof (msg), "CFG %d ACC %d\n", sensorIndex, presetId);
    return commandSocket->write (msg, (int) strlen (msg)) > 0;
}

bool AcqBoardRedPitaya::sendSensorCfgGyr (int sensorIndex, int presetId)
{
    if (sensorIndex >= 0 && sensorIndex < 6)
        sensorGyrPreset[sensorIndex] = presetId;

    if (commandSocket == nullptr)
        return false;

    char msg[48];
    snprintf (msg, sizeof (msg), "CFG %d GYR %d\n", sensorIndex, presetId);
    return commandSocket->write (msg, (int) strlen (msg)) > 0;
}

bool AcqBoardRedPitaya::sendSensorCfgSrate (int sensorIndex, int targetHz)
{
    if (commandSocket == nullptr)
        return false;

    char msg[48];
    snprintf (msg, sizeof (msg), "CFG %d SRATE %d\n", sensorIndex, targetHz);
    return commandSocket->write (msg, (int) strlen (msg)) > 0;
}

double AcqBoardRedPitaya::setUpperBandwidth (double upperBandwidth)
{
    upperBandwidthHz = upperBandwidth;
    return upperBandwidthHz;
}

double AcqBoardRedPitaya::setLowerBandwidth (double lowerBandwidth)
{
    lowerBandwidthHz = lowerBandwidth;
    return lowerBandwidthHz;
}

double AcqBoardRedPitaya::setDspCutoffFreq (double freq)
{
    dspCutoffFreqHz = freq;
    return dspCutoffFreqHz;
}

double AcqBoardRedPitaya::getDspCutoffFreq() const
{
    return dspCutoffFreqHz;
}

void AcqBoardRedPitaya::setDspOffset (bool /*enabled*/)
{
}

void AcqBoardRedPitaya::setTTLOutputMode (bool enabled)
{
    ttlOutputMode = enabled;
}

void AcqBoardRedPitaya::setDAChpf (float /*cutoff*/, bool /*enabled*/)
{
}

void AcqBoardRedPitaya::setFastTTLSettle (bool /*state*/, int /*channel*/)
{
}

int AcqBoardRedPitaya::setNoiseSlicerLevel (int level)
{
    return level;
}

void AcqBoardRedPitaya::enableBoardLeds (bool /*enabled*/)
{
}

int AcqBoardRedPitaya::setClockDivider (int divide_ratio)
{
    clockDivideRatio = divide_ratio;
    return clockDivideRatio;
}

void AcqBoardRedPitaya::connectHeadstageChannelToDAC (int /*headstageChannelIndex*/, int /*dacChannelIndex*/)
{
}

void AcqBoardRedPitaya::setDACTriggerThreshold (int /*dacChannelIndex*/, float /*threshold*/)
{
}

bool AcqBoardRedPitaya::isHeadstageEnabled (int /*hsNum*/) const
{
    return false;
}

int AcqBoardRedPitaya::getActiveChannelsInHeadstage (int /*hsNum*/) const
{
    return 0;
}

int AcqBoardRedPitaya::getChannelsInHeadstage (int /*hsNum*/) const
{
    return 0;
}

int AcqBoardRedPitaya::getNumDataOutputs (ContinuousChannel::Type channelType)
{
    if (channelType == ContinuousChannel::ELECTRODE)
        return 0;

    if (channelType == ContinuousChannel::AUX)
    {
        if (acquireAux)
            return 0;

        return 0;
    }

    if (channelType == ContinuousChannel::ADC)
    {
        if (acquireAdc)
            return numAdcChannels; // The DeviceThread assumes up to 6 ADC channels.

        return 0;
    }

    return 0;
}

void AcqBoardRedPitaya::setNumHeadstageChannels (int /*headstageIndex*/, int /*channelCount*/)
{
}

void AcqBoardRedPitaya::launchOpenSimMotion()
{
    // --until-idle 5: bridge collects packets until 5 s of silence, then runs IK
    // and launches OpenSim64.exe with the resulting .sto via AUTO_OPEN_GUI.
    // --scripted is added for fake-8-IMU mode: Python writes quaternions directly
    // instead of running AHRS, guaranteeing correct neutral-standing start pose.
    const bool useFakeScripted = (simulationMode && fakeImuCount == 8);
    const juce::String extraFlag = useFakeScripted ? " --scripted" : "";

    const juce::String workDir = "C:\\Users\\KIN Student\\Open-Sim--Bio-Mech";
    const juce::String bridgePath = workDir + "\\ephys_to_opensim_bridge.py";
    const juce::String logPath = workDir + "\\ephys_bridge_run.log";
    juce::File batFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("opensim_motion_hidden_launch.bat");

    batFile.replaceWithText (
        "@echo off\r\n"
        "cd /d \"" + workDir + "\"\r\n"
        "set PATH=C:\\OpenSim 4.5\\bin;%PATH%\r\n"
        "set PYTHONPATH=C:\\OpenSim 4.5\\sdk\\Python;%PYTHONPATH%\r\n"
        "py -3.12 \"" + bridgePath + "\" listen --until-idle 5" + extraFlag
        + " > \"" + logPath + "\" 2>&1\r\n");

    const juce::String command = "cmd.exe /c \"\"" + batFile.getFullPathName() + "\"\"";

    openSimProcess = std::make_unique<juce::ChildProcess>();
    if (openSimProcess->start (command))
    {
        std::cout << "OpenSim Motion generation launched hidden. Log: "
                  << logPath << std::endl;
    }
    else
    {
        openSimProcess.reset();
        std::cout << "OpenSim Motion generation failed to launch." << std::endl;
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            "Gen Motion",
            "Could not launch the OpenSim bridge. See ephys_bridge_run.log.");
        return;
    }

    openSimSocket  = std::make_unique<DatagramSocket>();
    openSimEnabled = true;
    std::cout << "OpenSim UDP forwarding enabled -> 127.0.0.1:5000" << std::endl;
}

void AcqBoardRedPitaya::launchOpenSimLive()
{
    const juce::String workDir    = "C:\\Users\\KIN Student\\Open-Sim--Bio-Mech";
    const juce::String scriptPath = workDir + "\\opensim_live_realtime.py";
    const juce::String logPath    = workDir + "\\ephys_live_run.log";
    const juce::String sourceLabel = simulationMode ? "fake_imu" : "real_redpitaya";

    juce::File batFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("opensim_live_visible.bat");

    batFile.replaceWithText (
        "@echo off\r\n"
        "cd /d \"" + workDir + "\"\r\n"
        "set PATH=C:\\OpenSim 4.5\\bin;%PATH%\r\n"
        "set PYTHONPATH=C:\\OpenSim 4.5\\sdk\\Python;%PYTHONPATH%\r\n"
        "set OPENSIM_LIVE_SOURCE=" + sourceLabel + "\r\n"
        "set OPENSIM_LIVE_GYRO_TEST=normal\r\n"
        "for /f \"tokens=5\" %%p in ('netstat -ano ^| findstr \":5000\"') do taskkill /PID %%p /F >nul 2>&1\r\n"
        "start \"OpenSim Live\" cmd /k py -3.8 -u \"" + scriptPath + "\"\r\n");

    const juce::String command = "cmd.exe /c \"\"" + batFile.getFullPathName() + "\"\"";

    openSimLiveProcess = std::make_unique<juce::ChildProcess>();
    if (! openSimLiveProcess->start (command))
    {
        openSimLiveProcess.reset();
        std::cout << "OpenSim Live: failed to launch." << std::endl;
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            "OpenSim Live",
            "Could not start the live bridge.");
    }
    else
    {
        std::cout << "OpenSim Live: launched visible console (py -3.8 -u)." << std::endl;
    }
}

void AcqBoardRedPitaya::sendOpenSimImuPacket (float timestamp, const float* data, int numImus)
{
    if (! openSimEnabled || openSimSocket == nullptr)
        return;
    const int totalFloats = 1 + numImus * 6;
    juce::HeapBlock<float> pkt (totalFloats);
    pkt[0] = timestamp;
    for (int i = 0; i < numImus * 6; ++i)
        pkt[1 + i] = data[i];
    openSimSocket->write ("127.0.0.1", 5000, pkt, totalFloats * (int) sizeof (float));

    static int openSimUdpSendCount = 0;
    ++openSimUdpSendCount;
    if (openSimUdpSendCount == 1 || (openSimUdpSendCount % 1000) == 0)
    {
        const float imu2Gy = (numImus >= 3) ? data[2 * 6 + 4] : std::numeric_limits<float>::quiet_NaN();
        std::cout << "OpenSim UDP sent #" << openSimUdpSendCount
                  << " t=" << timestamp
                  << " n_imus=" << numImus
                  << " gy[2]=" << imu2Gy << std::endl;
    }
}

void AcqBoardRedPitaya::setFakeImuCount (int count)
{
    fakeImuCount   = count;
    numAdcChannels = count * 6;
    simulationMode = true;
    deviceFound    = true;
    std::cout << "Red Pitaya: Fake " << count << " IMU mode active — "
              << numAdcChannels << " channels" << std::endl;
}

void AcqBoardRedPitaya::run()
{
    if (simulationMode)
    {
        const double sampleRate      = jmax (1.0, static_cast<double> (settings.boardSampleRate));
        const int64 samplesPerBuffer = jmax (int64 (1), int64 (std::lround (sampleRate / 1000.0)));
        const int msPerSample        = jmax (1, (int) std::round (1000.0 / sampleRate));
        int64   sampleNumber         = 0;
        double  elapsedSeconds       = 0.0;
        uint64  eventCode            = 0;
        int     lastPhase8           = -1;   // tracks phase changes for 8-IMU logging
        static int variationCounter  = 0;
        const int fakeVariation      = variationCounter % 3;
        ++variationCounter;
        const auto _scenarioStart = std::chrono::steady_clock::now();

        // Write a phase summary file so the Python side can annotate results.
        if (fakeImuCount == 8)
        {
            juce::File summary (juce::File ("C:\\Users\\KIN Student\\Open-Sim--Bio-Mech")
                                    .getChildFile ("fake8imu_phase_log.txt"));
            summary.replaceWithText (
                "Fake 8-IMU motion phase log\r\n"
                "Phase 0: idle          0 – 2 s\r\n"
                "Phase 1: right leg kick  2 – 4 s\r\n"
                "Phase 2: walking       4 – 7 s\r\n"
                "Phase 3: fall          7 – 9 s\r\n"
                "Phase 4: get up        9 – 12 s\r\n"
                "IMU order: torso(0) pelvis(1) femur_r(2) tibia_r(3) "
                "calcn_r(4) femur_l(5) tibia_l(6) calcn_l(7)\r\n");
        }

        while (! threadShouldExit())
        {
            DataBuffer* currentBuffer = buffer;
            if (currentBuffer == nullptr) { Thread::sleep (1); continue; }

            const int sampleIndex = (int) (sampleNumber % samplesPerBuffer);
            elapsedSeconds = std::chrono::duration<double> (
                std::chrono::steady_clock::now() - _scenarioStart).count();
            const float t  = (float) elapsedSeconds;
            const float pi = juce::MathConstants<float>::pi;

            const int nImus = jlimit (1, MAX_CHANNELS / 6, fakeImuCount);
            const int nCh   = nImus * 6;

            if (sampleNumber == 0)
            {
                std::cout << "Red Pitaya: Fake IMU Stream started — Fake " << nImus
                          << " IMU mode — sending " << nCh << " floats/packet" << std::endl;
                if (nImus != 8)
                    std::cout << "Fake " << nImus << " IMU variation "
                              << (fakeVariation + 1) << " started" << std::endl;
            }

            // ── 8-IMU scripted motion sequence ───────────────────────────────
            if (nImus == 8)
            {
                // Determine phase and local time within that phase.
                // Phase 0: idle       0–2 s
                // Phase 1: kick       2–4 s   right leg kick and return
                // Phase 2: walk       4–7 s   bilateral walking gait at 1 Hz
                // Phase 3: fall       7–9 s   forward fall
                // Phase 4: get up     9–12 s  recovery to standing
                const float sequenceT = std::fmod (t, 12.0f);
                int   phase  = 0;
                float phaseT = sequenceT;
                if      (sequenceT < 2.0f) { phase = 0; phaseT = sequenceT; }
                else if (sequenceT < 4.0f) { phase = 1; phaseT = sequenceT - 2.0f; }
                else if (sequenceT < 7.0f) { phase = 2; phaseT = sequenceT - 4.0f; }
                else if (sequenceT < 9.0f) { phase = 3; phaseT = sequenceT - 7.0f; }
                else                       { phase = 4; phaseT = sequenceT - 9.0f; }

                if (phase != lastPhase8)
                {
                    const char* names[] = { "idle", "right leg kick", "walking", "fall", "get up" };
                    std::cout << "Fake 8 IMU phase: " << names[phase] << std::endl;
                    lastPhase8 = phase;
                }

                // theta[i] = sagittal-plane angle (rad) — rotation around sensor Y axis.
                // omega[i] = d(theta)/dt in rad/s — fed to gyro channel (converted to deg/s).
                // Gravity in sensor frame: ax = sin(theta), ay = 0, az = cos(theta).
                // IMU order: 0=torso 1=pelvis 2=femur_r 3=tibia_r 4=calcn_r
                //            5=femur_l 6=tibia_l 7=calcn_l
                float theta[8] = {}, omega[8] = {};

                switch (phase)
                {
                    case 0: // idle — all segments at rest
                        break;

                    case 1: // right leg kick and return
                    {
                        // Smooth arc: 0 → peak → 0 over 2 s using sin(π·t/2)
                        const float k  = std::sin (pi * phaseT / 2.0f);
                        const float dk = (pi / 2.0f) * std::cos (pi * phaseT / 2.0f);
                        theta[0] = -0.15f * k;  omega[0] = -0.15f * dk;  // torso leans back
                        theta[1] =  0.10f * k;  omega[1] =  0.10f * dk;  // pelvis tilts
                        theta[2] =  1.20f * k;  omega[2] =  1.20f * dk;  // hip flexion ~69°
                        theta[3] =  0.80f * k;  omega[3] =  0.80f * dk;  // knee flexion ~46°
                        theta[4] =  0.30f * k;  omega[4] =  0.30f * dk;  // ankle ~17°
                        theta[5] = -0.10f * k;  omega[5] = -0.10f * dk;  // support hip extends
                        theta[6] =  0.05f * k;  omega[6] =  0.05f * dk;
                        theta[7] =  0.0f;        omega[7] =  0.0f;
                        break;
                    }

                    case 2: // bilateral walking at 1 Hz
                    {
                        const float freq = 2.0f * pi;          // 1 Hz in rad/s
                        const float wt   = freq * phaseT;
                        // Each IMU: theta = A·sin(wt + phase_offset)
                        //           omega = A·freq·cos(wt + phase_offset)
                        auto sw = [&] (float A, float ph) -> std::pair<float,float>
                        {
                            return { A * std::sin (wt + ph),
                                     A * freq * std::cos (wt + ph) };
                        };
                        auto [t0,w0] = sw (0.10f, 0.0f);   theta[0]=t0; omega[0]=w0;  // torso
                        auto [t1,w1] = sw (0.12f, 0.0f);   theta[1]=t1; omega[1]=w1;  // pelvis
                        auto [t2,w2] = sw (0.50f, 0.0f);   theta[2]=t2; omega[2]=w2;  // femur_r
                        auto [t3,w3] = sw (0.40f, 0.5f);   theta[3]=t3; omega[3]=w3;  // tibia_r
                        auto [t4,w4] = sw (0.25f, 0.8f);   theta[4]=t4; omega[4]=w4;  // calcn_r
                        auto [t5,w5] = sw (0.50f,  pi );   theta[5]=t5; omega[5]=w5;  // femur_l
                        auto [t6,w6] = sw (0.40f, pi+0.5f);theta[6]=t6; omega[6]=w6;  // tibia_l
                        auto [t7,w7] = sw (0.25f, pi+0.8f);theta[7]=t7; omega[7]=w7;  // calcn_l
                        break;
                    }

                    case 3: // forward fall — linear ramp over 2 s
                    {
                        const float p  = phaseT / 2.0f;         // 0→1
                        const float dp = 1.0f / 2.0f;
                        const float fa[] = { 0.90f, 0.70f, 0.50f, 0.30f,
                                             0.15f, 0.50f, 0.30f, 0.15f };
                        for (int i = 0; i < 8; ++i) { theta[i] = fa[i]*p; omega[i] = fa[i]*dp; }
                        break;
                    }

                    case 4: // get up — linear recovery over 3 s
                    {
                        const float up = jmin (1.0f, phaseT / 3.0f);
                        const float rem = 1.0f - up;
                        const float dUp = (up < 1.0f) ? -1.0f / 3.0f : 0.0f;
                        const float fa[] = { 0.90f, 0.70f, 0.50f, 0.30f,
                                             0.15f, 0.50f, 0.30f, 0.15f };
                        for (int i = 0; i < 8; ++i) { theta[i] = fa[i]*rem; omega[i] = fa[i]*dUp; }
                        break;
                    }
                }

                // Convert angles → physical IMU readings.
                // Accel: gravity vector in tilted sensor frame (unit-g, Y-axis rotation).
                // Gyro:  sagittal angular velocity converted to deg/s.
                const float radToDeg = 180.0f / pi;
                for (int imu = 0; imu < 8; ++imu)
                {
                    const int base = imu * 6;
                    samples[(base+0)*samplesPerBuffer+sampleIndex] = std::sin (theta[imu]);   // ax (g)
                    samples[(base+1)*samplesPerBuffer+sampleIndex] = 0.0f;                     // ay
                    samples[(base+2)*samplesPerBuffer+sampleIndex] = std::cos (theta[imu]);   // az (g)
                    samples[(base+3)*samplesPerBuffer+sampleIndex] = 0.0f;                     // gx (deg/s)
                    samples[(base+4)*samplesPerBuffer+sampleIndex] = omega[imu] * radToDeg;   // gy (deg/s)
                    samples[(base+5)*samplesPerBuffer+sampleIndex] = 0.0f;                     // gz (deg/s)
                }
            }
            else
            {
                // ── 2/4/6-IMU scenario motions ─────────────────────────────
                const float radToDeg = 180.0f / pi;
                const float tWall = static_cast<float> (
                    std::chrono::duration<double> (
                        std::chrono::steady_clock::now() - _scenarioStart).count());
                const float ampScale[] = { 1.0f, 0.70f, 1.35f };
                const float amp = ampScale[jlimit (0, 2, fakeVariation)];

                auto writeImuY = [&] (int imu, float theta, float omega)
                {
                    const int base = imu * 6;
                    samples[(base+0)*samplesPerBuffer+sampleIndex] = std::sin (theta);
                    samples[(base+1)*samplesPerBuffer+sampleIndex] = 0.0f;
                    samples[(base+2)*samplesPerBuffer+sampleIndex] = std::cos (theta);
                    samples[(base+3)*samplesPerBuffer+sampleIndex] = 0.0f;
                    samples[(base+4)*samplesPerBuffer+sampleIndex] = omega * radToDeg;
                    samples[(base+5)*samplesPerBuffer+sampleIndex] = 0.0f;
                };

                auto writeImuX = [&] (int imu, float theta, float omega)
                {
                    const int base = imu * 6;
                    samples[(base+0)*samplesPerBuffer+sampleIndex] = 0.0f;
                    samples[(base+1)*samplesPerBuffer+sampleIndex] = -std::sin (theta);
                    samples[(base+2)*samplesPerBuffer+sampleIndex] = std::cos (theta);
                    samples[(base+3)*samplesPerBuffer+sampleIndex] = omega * radToDeg;
                    samples[(base+4)*samplesPerBuffer+sampleIndex] = 0.0f;
                    samples[(base+5)*samplesPerBuffer+sampleIndex] = 0.0f;
                };

                auto writeImuZ = [&] (int imu, float theta, float omega)
                {
                    const int base = imu * 6;
                    samples[(base+0)*samplesPerBuffer+sampleIndex] = std::sin (theta);
                    samples[(base+1)*samplesPerBuffer+sampleIndex] = 0.0f;
                    samples[(base+2)*samplesPerBuffer+sampleIndex] = std::cos (theta);
                    samples[(base+3)*samplesPerBuffer+sampleIndex] = 0.0f;
                    samples[(base+4)*samplesPerBuffer+sampleIndex] = 0.0f;
                    samples[(base+5)*samplesPerBuffer+sampleIndex] = omega * radToDeg;
                };

                for (int imu = 0; imu < nImus; ++imu)
                    writeImuY (imu, 0.0f, 0.0f);

                if (nImus == 6)
                {
                    const float cycleT = std::fmod (tWall, 12.0f);
                    float theta[6] = {}, omega[6] = {};

                    if (cycleT >= 2.0f && cycleT < 4.0f)
                    {
                        const float u = (cycleT - 2.0f) / 2.0f;
                        const float k = 0.5f - 0.5f * std::cos (pi * u);
                        const float dk = 0.5f * pi * std::sin (pi * u) / 2.0f;
                        theta[0] =  0.18f * amp * k;  omega[0] =  0.18f * amp * dk;
                        theta[1] =  0.14f * amp * k;  omega[1] =  0.14f * amp * dk;
                        theta[2] = -0.70f * amp * k;  omega[2] = -0.70f * amp * dk;
                        theta[3] =  0.85f * amp * k;  omega[3] =  0.85f * amp * dk;
                        theta[4] =  0.25f * amp * k;  omega[4] =  0.25f * amp * dk;
                        theta[5] = -0.70f * amp * k;  omega[5] = -0.70f * amp * dk;
                    }
                    else if (cycleT >= 4.0f && cycleT < 6.0f)
                    {
                        const float u = (cycleT - 4.0f) / 2.0f;
                        const float k = 0.5f + 0.5f * std::cos (pi * u);
                        const float dk = -0.5f * pi * std::sin (pi * u) / 2.0f;
                        theta[0] =  0.18f * amp * k;  omega[0] =  0.18f * amp * dk;
                        theta[1] =  0.14f * amp * k;  omega[1] =  0.14f * amp * dk;
                        theta[2] = -0.70f * amp * k;  omega[2] = -0.70f * amp * dk;
                        theta[3] =  0.85f * amp * k;  omega[3] =  0.85f * amp * dk;
                        theta[4] =  0.25f * amp * k;  omega[4] =  0.25f * amp * dk;
                        theta[5] = -0.70f * amp * k;  omega[5] = -0.70f * amp * dk;
                    }
                    else if (cycleT >= 6.0f && cycleT < 9.0f)
                    {
                        const float u = (cycleT - 6.0f) / 3.0f;
                        const float k = std::sin (pi * u);
                        const float dk = (pi / 3.0f) * std::cos (pi * u);
                        theta[0] = -0.08f * amp * k;  omega[0] = -0.08f * amp * dk;
                        theta[1] = -0.10f * amp * k;  omega[1] = -0.10f * amp * dk;
                        theta[5] =  0.85f * amp * k;  omega[5] =  0.85f * amp * dk;
                    }
                    else if (cycleT >= 9.0f)
                    {
                        const float u = (cycleT - 9.0f) / 3.0f;
                        const float k = std::sin (pi * u);
                        const float dk = (pi / 3.0f) * std::cos (pi * u);
                        theta[0] =  0.08f * amp * k;  omega[0] =  0.08f * amp * dk;
                        theta[1] =  0.10f * amp * k;  omega[1] =  0.10f * amp * dk;
                        theta[2] =  0.85f * amp * k;  omega[2] =  0.85f * amp * dk;
                        theta[3] =  0.35f * amp * k;  omega[3] =  0.35f * amp * dk;
                    }

                    for (int imu = 0; imu < nImus; ++imu)
                        writeImuY (imu, theta[imu], omega[imu]);
                }
                else if (nImus == 4)
                {
                    const float cycleT = std::fmod (tWall, 10.0f);
                    if (cycleT >= 2.0f && cycleT < 4.0f)
                    {
                        const float u = (cycleT - 2.0f) / 2.0f;
                        const float k = std::sin (pi * u);
                        const float dk = (pi / 2.0f) * std::cos (pi * u);
                        writeImuX (0,  0.55f * amp * k,  0.55f * amp * dk);
                        writeImuX (1,  0.25f * amp * k,  0.25f * amp * dk);
                    }
                    else if (cycleT >= 4.0f && cycleT < 6.0f)
                    {
                        const float u = (cycleT - 4.0f) / 2.0f;
                        const float k = std::sin (pi * u);
                        const float dk = (pi / 2.0f) * std::cos (pi * u);
                        writeImuX (0, -0.45f * amp * k, -0.45f * amp * dk);
                        writeImuX (1, -0.20f * amp * k, -0.20f * amp * dk);
                    }
                    else if (cycleT >= 6.0f && cycleT < 8.0f)
                    {
                        const float u = (cycleT - 6.0f) / 2.0f;
                        const float k = std::sin (pi * u);
                        const float dk = (pi / 2.0f) * std::cos (pi * u);
                        writeImuZ (0,  0.65f * amp * k,  0.65f * amp * dk);
                        writeImuZ (1,  0.35f * amp * k,  0.35f * amp * dk);
                    }
                    else if (cycleT >= 8.0f)
                    {
                        const float u = (cycleT - 8.0f) / 2.0f;
                        const float k = std::sin (pi * u);
                        const float dk = (pi / 2.0f) * std::cos (pi * u);
                        writeImuZ (0, -0.65f * amp * k, -0.65f * amp * dk);
                        writeImuZ (1, -0.35f * amp * k, -0.35f * amp * dk);
                    }
                }
                else if (nImus == 2)
                {
                    const float cycleT = std::fmod (tWall, 8.0f);
                    float femurTheta = 0.0f, femurOmega = 0.0f;
                    float tibiaTheta = 0.0f, tibiaOmega = 0.0f;

                    if (cycleT >= 2.0f && cycleT < 5.0f)
                    {
                        const float u = (cycleT - 2.0f) / 3.0f;
                        const float k = 0.5f - 0.5f * std::cos (pi * u);
                        const float dk = 0.5f * pi * std::sin (pi * u) / 3.0f;
                        femurTheta = 0.25f * amp * k;  femurOmega = 0.25f * amp * dk;
                        tibiaTheta = 1.10f * amp * k;  tibiaOmega = 1.10f * amp * dk;
                    }
                    else if (cycleT >= 5.0f)
                    {
                        const float u = (cycleT - 5.0f) / 3.0f;
                        const float k = 0.5f + 0.5f * std::cos (pi * u);
                        const float dk = -0.5f * pi * std::sin (pi * u) / 3.0f;
                        femurTheta = 0.25f * amp * k;  femurOmega = 0.25f * amp * dk;
                        tibiaTheta = 1.10f * amp * k;  tibiaOmega = 1.10f * amp * dk;
                    }

                    writeImuY (0, femurTheta, femurOmega);
                    writeImuY (1, tibiaTheta, tibiaOmega);
                }
            }

            // Build flat packet [ax0,ay0,az0,gx0,gy0,gz0, ax1,...] and send
            float imuPkt[MAX_CHANNELS];
            for (int ch = 0; ch < nCh; ++ch)
                imuPkt[ch] = samples[ch * samplesPerBuffer + sampleIndex];
            sendOpenSimImuPacket ((float) elapsedSeconds, imuPkt, nImus);

            sampleNumbers[sampleIndex] = sampleNumber;
            timestamps[sampleIndex]    = elapsedSeconds;
            event_codes[sampleIndex]   = eventCode;

            ++sampleNumber;

            if (sampleIndex == (int) samplesPerBuffer - 1)
            {
                if (! threadShouldExit())
                    currentBuffer->addToBuffer (samples, sampleNumbers, timestamps, event_codes, (int) samplesPerBuffer);
            }

            Thread::sleep (msPerSample);
        }
        return;
    }

    if (commandSocket == nullptr)
        return;

    int64 sampleNumber = 0;
    double elapsedSeconds = 0.0;
    const int64 samplesPerBuffer = jmax (
        int64 (1),
        int64 (std::lround (static_cast<double> (settings.boardSampleRate) / 1000.0)));
    uint64 eventCode = 0;

    const int numAdcChannelsLocal = getNumDataOutputs (ContinuousChannel::ADC);

    if (numAdcChannelsLocal <= 0 || numAdcChannelsLocal > MAX_CHANNELS || samplesPerBuffer <= 0)
        return;

    // Open a dedicated UDP socket for the data stream.
    // The TCP commandSocket remains open exclusively for control commands
    // (STOP signalled by TCP close in stopAcquisition()).
    DatagramSocket udpSocket;
    if (! udpSocket.bindToPort (55001))
    {
        std::cout << "Red Pitaya ERROR: Failed to bind UDP socket on port 55001" << std::endl;
        return;
    }

    constexpr int headerSize = 22;
    const int payloadSize = numAdcChannelsLocal * 2; // int16 per channel
    const int packetSize = headerSize + payloadSize;
    constexpr int packetsPerChunk = 1; // must match CHUNK_SAMPLES in RedPitaya_justin.c
    const int chunkSize = packetSize * packetsPerChunk;

    uint8_t chunkBuffer[65507]; // max UDP payload

    while (! threadShouldExit())
    {
        DataBuffer* currentBuffer = buffer;

        if (currentBuffer == nullptr)
        {
            Thread::sleep (1);
            continue;
        }

        // Rebuild per-channel scale factors each buffer so mid-stream range
        // changes (sendSensorCfgAcc/Gyr) take effect within one buffer.
        float channelScale[MAX_CHANNELS];
        for (int i = 0; i < MAX_CHANNELS; ++i)
            channelScale[i] = 1.0f;

        {
            int chanOffset = 0;
            const int numSensors = streamSensorNames.size();

            for (int si = 0; si < numSensors && si < 6; ++si)
            {
                const String& sname = streamSensorNames.getReference (si);
                const float accScale = 1.0f / kAccSensitivity[jlimit (0, 3, sensorAccPreset[si])];
                const float gyrScale = 1.0f / kGyrSensitivity[jlimit (0, 3, sensorGyrPreset[si])];
                const bool is6axis = (sname == "MPU6050");
                const bool isBNO = (sname == "BNO055");
                const int numRaw = is6axis ? 6 : 9;

                // acc axes are always the first three channels for all sensors
                channelScale[chanOffset + 0] = accScale;
                channelScale[chanOffset + 1] = accScale;
                channelScale[chanOffset + 2] = accScale;

                if (isBNO)
                {
                    // BNO055 layout: [0-2]=acc, [3-5]=mag, [6-8]=gyr
                    channelScale[chanOffset + 6] = gyrScale;
                    channelScale[chanOffset + 7] = gyrScale;
                    channelScale[chanOffset + 8] = gyrScale;
                    // mag [3-5] left at 1.0 — no user-settable range
                }
                else
                {
                    // MPU family: [0-2]=acc, [3-5]=gyr, [6-8]=mag (9-axis only)
                    channelScale[chanOffset + 3] = gyrScale;
                    channelScale[chanOffset + 4] = gyrScale;
                    channelScale[chanOffset + 5] = gyrScale;
                    // mag [6-8] left at 1.0 — no user-settable range
                }
                // quaternion channels [numRaw..numRaw+3] left at 1.0

                chanOffset += numRaw + 4;
            }
            // analog waveform channels after sensors left at 1.0
        }

        // Poll with 100ms timeout so threadShouldExit() is checked regularly.
        if (! udpSocket.waitUntilReady (true, 100))
            continue;

        int n = udpSocket.read ((char*) chunkBuffer, chunkSize, false);

        // Skip malformed or empty datagrams.
        if (n <= 0 || (n % packetSize) != 0)
            continue;

        const int numPackets = n / packetSize;

        for (int p = 0; p < numPackets && ! threadShouldExit(); ++p)
        {
            uint8_t* pkt = chunkBuffer + (p * packetSize);
            const int16_t* channels = reinterpret_cast<const int16_t*> (pkt + headerSize);
            const int channelsInPacket = payloadSize / 2;
            const int sampleIndex = p % (int) samplesPerBuffer;

            for (int adc = 0; adc < numAdcChannelsLocal; ++adc)
            {
                const float raw = (adc < channelsInPacket) ? float (channels[adc]) : 0.0f;
                samples[(adc * samplesPerBuffer) + sampleIndex] = raw * channelScale[adc];
            }

            // Forward N-IMU packet: each IMU = 6 channels, layout ax,ay,az,gx,gy,gz
            {
                const int numImusLocal = numAdcChannelsLocal / 6;
                if (numImusLocal >= 1)
                {
                    const int nCh = numImusLocal * 6;
                    float imuPkt[MAX_CHANNELS];
                    for (int ch = 0; ch < nCh; ++ch)
                        imuPkt[ch] = samples[ch * samplesPerBuffer + sampleIndex];
                    if (sampleNumber == 0)
                        std::cout << "Red Pitaya: real " << numImusLocal << "-IMU packet with "
                                  << nCh << " floats — IMU0 ax=" << imuPkt[0] << std::endl;
                    sendOpenSimImuPacket ((float) elapsedSeconds, imuPkt, numImusLocal);
                }
            }

            const double currentSampleRate = jmax (1.0, static_cast<double> (settings.boardSampleRate));
            sampleNumbers[sampleIndex] = sampleNumber;
            timestamps[sampleIndex] = elapsedSeconds;
            event_codes[sampleIndex] = eventCode;

            ++sampleNumber;
            elapsedSeconds += 1.0 / currentSampleRate;

            if (sampleIndex == (int) samplesPerBuffer - 1)
            {
                currentBuffer = buffer;
                if (currentBuffer != nullptr && ! threadShouldExit())
                    currentBuffer->addToBuffer (samples,
                                                sampleNumbers,
                                                timestamps,
                                                event_codes,
                                                (int) samplesPerBuffer);
            }
        }
    }
}
