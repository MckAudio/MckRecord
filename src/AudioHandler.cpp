#include "AudioHandler.hpp"
#include <iostream>
#include <cmath>
#include <string.h>
#include <thread>
#include <chrono>
#include <sndfile.h>

AudioHandler::AudioHandler()
    : m_bufferLen(0),
      m_buffer(nullptr),
      m_bufferIdx(0),
      m_noiseLevel(-200.0),
      m_noiseIdx(0),
      m_noiseLen(0),
      m_stopIdx(0),
      m_stopLen(127),
      m_finished(false),
      m_recording(false),
      m_sampleRate(96000),
      m_bufferSize(256),
      m_noiseRmsCoeff(0.0),
      m_noiseRmsLen(0),
      m_signalRmsCoeff(0.0),
      m_signalRmsLen(0)
{
}

AudioHandler::~AudioHandler()
{
    Free();
}

int playback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void *userData)
{
    AudioHandler *audioHandler = (AudioHandler *)userData;

    double *buffer = (double *)outputBuffer;

    return 0;
}

int record(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void *userData)
{
    AudioHandler *ah = (AudioHandler *)userData;

    double *in = (double *)inputBuffer;
    bool isListening = ah->m_listening.load();
    bool isRecording = ah->m_recording.load();
    bool isFinished = ah->m_finished.load();
    unsigned startIdx = 0;
    double noiseNotValid = std::pow(10.0, -20.0 / 20.0);

    if (isListening)
    {
        if (ah->m_noiseIdx < nBufferFrames)
        {
            ah->m_noiseIdx += nBufferFrames;
        }
        else if (ah->m_noiseIdx < (ah->m_noiseRmsLen + nBufferFrames))
        {
            // RMS measuring
            for (unsigned i = 0; i < nBufferFrames; i++)
            {
                ah->m_noiseRmsValue = std::pow(in[i], 2.0) * ah->m_noiseRmsCoeff + ah->m_noiseRmsValue * (1.0 - ah->m_noiseRmsCoeff);
            }
            ah->m_noiseIdx += nBufferFrames;
        }
        else
        {
            if (ah->m_noiseRmsValue >= noiseNotValid)
            {
                // Noise Level is too high
                ah->m_listening = false;
                ah->m_recording = true;
                ah->m_finished = true;
                return 0;
            }
            // Noise Gate
            double level = -200.0;
            double noiseRmsDb = 20.0 * std::log10(ah->m_noiseRmsValue) + 12.0;
            double signalRmsDb = 0.0;
            for (unsigned i = 0; i < nBufferFrames; i++)
            {
                ah->m_signalRmsValue = std::pow(in[i], 2.0) * ah->m_signalRmsCoeff + ah->m_signalRmsValue * (1.0 - ah->m_signalRmsCoeff);
                signalRmsDb = 20.0 * std::log10(ah->m_signalRmsValue);
                if (signalRmsDb >= noiseRmsDb)
                {
                    ah->m_bufferIdx = 0;
                    ah->m_stopIdx = 0;
                    ah->m_listening = false;
                    ah->m_recording = true;
                    ah->m_signalRmsValue = 1.0;
                    isRecording = true;
                    startIdx = i - std::min(ah->m_signalRmsLen, i);
                    break;
                }
            }
        }
    }

    unsigned samplesLeft = (ah->m_bufferLen - 1) - ah->m_bufferIdx;
    unsigned len = std::min(nBufferFrames - startIdx, samplesLeft);
    if (isRecording && isFinished == false)
    {
        if (ah->m_bufferIdx >= ah->m_minSignalLen)
        {
            double noiseRmsDb = 20.0 * std::log10(ah->m_noiseRmsValue) + 6.0;
            double signalRmsDb = 0.0;

            for (unsigned i = 0; i < len; i++)
            {
                ah->m_signalRmsValue = std::pow(in[i], 2.0) * ah->m_noiseRmsCoeff + ah->m_signalRmsValue * (1.0 - ah->m_noiseRmsCoeff);
                signalRmsDb = 20.0 * std::log10(ah->m_signalRmsValue);
                if (signalRmsDb <= noiseRmsDb)
                {
                    len = i;
                    ah->m_finished = true;
                }
            }
        }
        else
        {
            for (unsigned i = 0; i < len; i++)
            {
                ah->m_signalRmsValue = std::pow(in[i], 2.0) * ah->m_noiseRmsCoeff + ah->m_signalRmsValue * (1.0 - ah->m_noiseRmsCoeff);
            }
        }

        if (len > 0)
        {
            memcpy(ah->m_buffer + ah->m_bufferIdx, in + startIdx, len * sizeof(double));
            ah->m_bufferIdx += len;
        }
        else
        {
            ah->m_finished = true;
        }
    }

    return 0;
}

void AudioHandler::Init()
{
    unsigned nDevices = m_audio.getDeviceCount();
    m_devices.clear();
    m_devices.resize(nDevices);

    std::cout << "Audio Devices:\n";
    for (unsigned i = 0; i < nDevices; i++)
    {
        m_devices[i] = m_audio.getDeviceInfo(i);

        if (m_devices[i].probed)
        {
            std::cout << "  #" << i + 1 << ": " << m_devices[i].name << " (" << m_devices[i].inputChannels << " IN / " << m_devices[i].outputChannels << " OUT)\n";
        }
    }
    std::cout << "Default input device: " << (m_audio.getDefaultInputDevice()) << std::endl;
    std::cout << "Default output device: " << (m_audio.getDefaultOutputDevice()) << std::endl;
}

bool AudioHandler::OpenInterface(unsigned idx, unsigned recordingLengthMs)
{
    if (idx >= m_devices.size())
    {
        return false;
    }

    m_bufferLen = (unsigned)std::ceil((double)recordingLengthMs * ((double)m_sampleRate / 1000.0));

    m_buffer = (double *)malloc(m_bufferLen * sizeof(double));
    memset(m_buffer, 0, m_bufferLen * sizeof(double));

    m_bufferSize = 1024;
    RtAudio::StreamParameters param;
    //param.deviceId = m_audio.getDefaultInputDevice();
    param.deviceId = idx;
    param.firstChannel = 0;
    param.nChannels = 1;

    try
    {
        m_audio.openStream(NULL, &param, RTAUDIO_FLOAT64, m_sampleRate, &m_bufferSize, &record, this);
        m_audio.startStream();
    }
    catch (RtAudioError &e)
    {
        e.printMessage();
        return false;
    }

    return true;
}

bool AudioHandler::Start()
{
    m_listening = false;
    m_finished = false;
    m_recording = false;

    // Wait a few milliseconds to fetch the noise level
    m_noiseLevel = -200.0;
    m_noiseIdx = 0;
    m_noiseLen = (unsigned)(std::ceil(0.1 * (double)m_sampleRate / (double)m_bufferSize) * (double)m_bufferSize);
    m_bufferIdx = 0;

    m_noiseRmsValue = 0.0;
    m_noiseRmsLen = m_noiseLen;
    m_noiseRmsCoeff = 1.0 / (double)m_noiseLen;

    m_signalRmsValue = 0.0;
    m_signalRmsLen = 16;
    m_signalRmsCoeff = 1.0 / (double)m_signalRmsLen;
    m_minSignalLen = m_sampleRate;

    m_listening = true;
    m_finished = false;
    m_recording = false;

    return true;
}

bool AudioHandler::Stop(std::string path)
{
    m_listening = false;
    m_finished = false;
    m_recording = false;

    if (m_noiseLevel >= -20.0)
    {
        std::fprintf(stderr, "Noise level is higher than -20.0dB! Please try again.\n");
        return false;
    }

    double maxVal = 0.0;

    for (unsigned i = 0; i < m_bufferLen; i++)
    {
        double val = std::abs(m_buffer[i]);
        maxVal = val >= maxVal ? val : maxVal;
    }

    if (maxVal >= 1.0)
    {
        return false;
    }

    // Normalize to -1.0dB
    double newMax = std::pow(10.0, -1.0 / 20.0);
    for (unsigned i = 0; i < m_bufferIdx; i++)
    {
        m_buffer[i] = newMax * m_buffer[i] / maxVal;
    }

    unsigned fadeLen = std::min(m_bufferIdx, m_noiseLen);
    double fadeCoeff = 1.0 / ((double)(fadeLen - 1));
    for (unsigned i = m_bufferIdx, j = 0; j < fadeLen; i--, j++)
    {
        m_buffer[i] *= (j * fadeCoeff);
    }

    // Write buffer to file
    SF_INFO sndInfo;
    sndInfo.channels = 1;
    sndInfo.samplerate = m_sampleRate;
    sndInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
    SNDFILE *sndFile = sf_open(path.c_str(), SFM_WRITE, &sndInfo);
    sf_writef_double(sndFile, m_buffer, m_bufferIdx);
    sf_close(sndFile);

    return true;
}

void AudioHandler::Free()
{
    if (m_audio.isStreamRunning())
    {
        try
        {
            m_audio.stopStream();
        }
        catch (RtAudioError &e)
        {
            e.printMessage();
        }
    }

    if (m_audio.isStreamOpen())
    {
        m_audio.closeStream();
    }

    if (m_buffer != nullptr)
    {
        free(m_buffer);
        m_buffer = nullptr;
    }
}