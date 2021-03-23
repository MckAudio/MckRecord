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
      m_noiseLevel(0.0),
      m_noiseIdx(0),
      m_noiseLen(0),
      m_finished(false),
      m_recording(false),
      m_sampleRate(96000)
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
    bool isRecording = ah->m_recording.load();
    bool isFinished = ah->m_finished.load();
    unsigned startIdx = 0;

    if (isRecording == false && isFinished == false)
    {
        if (ah->m_noiseIdx < ah->m_noiseLen)
        {
            // Measuring noise level
            double level = 0.0;
            for (unsigned i = 0; i < nBufferFrames; i++)
            {
                level = 20.0 * std::log10(std::abs(in[i]));
                ah->m_noiseLevel = std::max(level, ah->m_noiseLevel);
            }
            ah->m_noiseIdx += nBufferFrames;
        }
        else
        {
            if (ah->m_noiseLevel >= -20.0)
            {
                // Noise Level is too high
                ah->m_recording = true;
                ah->m_finished = true;
                return 0;
            }
            // Noise Gate
            double level = -200.0;
            for (unsigned i = 0; i < nBufferFrames; i++)
            {
                level = 20.0 * std::log10(std::abs(in[i])) - 3.0;
                if (level >= ah->m_noiseLevel)
                {
                    ah->m_bufferIdx = 0;
                    ah->m_recording = true;
                    isRecording = true;
                    startIdx = i > 0 ? i - 1 : i;
                    break;
                }
            }
        }
    }

    unsigned samplesLeft = (ah->m_bufferLen - 1) - ah->m_bufferIdx;
    unsigned len = std::min(nBufferFrames - startIdx, samplesLeft);
    if (isRecording && isFinished == false)
    {
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

bool AudioHandler::Start(unsigned idx, unsigned recordingLengthMs)
{
    Free();

    if (idx >= m_devices.size())
    {
        return false;
    }

    m_bufferLen = (unsigned)std::ceil((double)recordingLengthMs * ((double)m_sampleRate / 1000.0));
    unsigned bufferSize = 256;
    RtAudio::StreamParameters param;
    //param.deviceId = m_audio.getDefaultInputDevice();
    param.deviceId = idx;
    param.firstChannel = 0;
    param.nChannels = 1;

    m_buffer = (double *)malloc(m_bufferLen * sizeof(double));
    memset(m_buffer, 0, m_bufferLen * sizeof(double));
    m_finished = false;
    m_recording = false;

    // Wait a few milliseconds to fetch the noise level
    m_noiseLevel = -200.0;
    m_noiseIdx = 0;
    m_noiseLen = (unsigned)std::ceil(0.1 * (double)m_sampleRate);

    try
    {
        m_audio.openStream(NULL, &param, RTAUDIO_FLOAT64, m_sampleRate, &bufferSize, &record, this);
        m_audio.startStream();
    }
    catch (RtAudioError &e)
    {
        e.printMessage();
        return false;
    }

    return true;
}

bool AudioHandler::Stop(std::string path)
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

    m_finished = false;
    m_recording = false;

    if (m_noiseLevel >= 0.1)
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
    for (unsigned i = 0; i < m_bufferLen; i++)
    {
        m_buffer[i] = newMax * m_buffer[i] / maxVal;
    }

    // Write buffer to file
    SF_INFO sndInfo;
    sndInfo.channels = 1;
    sndInfo.samplerate = m_sampleRate;
    sndInfo.format = SF_FORMAT_WAV | SF_FORMAT_DOUBLE;
    SNDFILE *sndFile = sf_open(path.c_str(), SFM_WRITE, &sndInfo);
    sf_writef_double(sndFile, m_buffer, m_bufferLen);
    sf_close(sndFile);

    return true;
}

void AudioHandler::Free()
{
    if (m_buffer != nullptr)
    {
        free(m_buffer);
        m_buffer = nullptr;
    }
}