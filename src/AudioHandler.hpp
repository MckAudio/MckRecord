#pragma once
#include <vector>
#include <atomic>
#include "rtaudio/RtAudio.h"

class AudioHandler {
    public:
        AudioHandler();
        ~AudioHandler();

        void Init();
        bool OpenInterface(unsigned idx, unsigned recordingLength);
        bool Start();
        bool Stop(std::string path);

        unsigned GetSampleRate() { return m_sampleRate; };
        bool SetSampleRate(unsigned sampleRate);

        bool IsRecording() { return m_recording.load(); };
        bool IsFinished() { return m_finished.load() && m_recording.load(); };

        unsigned GetDeviceCount() { return m_devices.size(); };

        double* m_buffer;
        unsigned m_bufferLen;
        unsigned m_bufferIdx;   
        double m_noiseLevel;
        unsigned m_noiseIdx;
        unsigned m_noiseLen;
        unsigned m_stopIdx;
        unsigned m_stopLen;
        std::atomic<bool> m_finished;
        std::atomic<bool> m_recording;
        std::atomic<bool> m_listening;
        unsigned m_sampleRate;
        unsigned m_bufferSize;

        // RMS Detection
        double m_noiseRmsCoeff;
        double m_noiseRmsValue;
        unsigned m_noiseRmsLen;
        double m_signalRmsCoeff;
        double m_signalRmsValue;
        unsigned m_signalRmsLen;
        unsigned m_minSignalLen;

    private:
        void Free();
        RtAudio m_audio;
        std::vector<RtAudio::DeviceInfo> m_devices;
};