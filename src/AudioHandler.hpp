#pragma once
#include <vector>
#include <atomic>
#include "rtaudio/RtAudio.h"

class AudioHandler {
    public:
        AudioHandler();
        ~AudioHandler();

        void Init();
        bool Start(unsigned idx, unsigned recordingLength);
        bool Stop(std::string path);

        unsigned GetSampleRate() { return m_sampleRate; }
        bool SetSampleRate(unsigned sampleRate);

        bool IsRecording() { return m_recording.load(); }
        bool IsFinished() { return m_finished.load() && m_recording.load(); }

        double* m_buffer;
        unsigned m_bufferLen;
        unsigned m_bufferIdx;   
        double m_border;
        std::atomic<bool> m_finished;
        std::atomic<bool> m_recording;

    private:
        void Free();
        RtAudio m_audio;
        std::vector<RtAudio::DeviceInfo> m_devices;
        unsigned m_sampleRate;
};