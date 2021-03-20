#include <cstdio>
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include "AudioHandler.hpp"

namespace fs = std::filesystem;

int main(int argc, char **argv)
{
    std::printf(">> Welcome to MCK-Record <<\n\n");

    if (argc < 1)
    {
        std::fprintf(stderr, "No filepath provided, I need a a filename!");
        return 1;
    }

    // #1 - Open Audio Device
    AudioHandler m_audioHandler;
    m_audioHandler.Init();

    // #2 - File path
    fs::path argPath(argv[1]);
    if (argPath.is_relative())
    {
        argPath = fs::absolute(argPath);
    }

    // #3 - Check path
    bool dirMode = false;
    unsigned wavCount = 0;
    if (fs::is_directory(argPath) || argPath.has_extension() == false)
    {
        if (fs::exists(argPath) == false)
        {
            fs::create_directories(argPath);
        }
        else
        {
            for (auto &p : fs::directory_iterator(argPath))
            {
                if (p.is_regular_file())
                {
                    if (p.path().extension() == ".wav")
                    {
                        wavCount += 1;
                    }
                }
            }
        }
        dirMode = true;
    }
    else
    {
        if (fs::exists(argPath.parent_path()) == false)
        {
            fs::create_directories(argPath.parent_path());
        }
    }

    while (true)
    {
        fs::path filePath = argPath;
        if (dirMode)
        {
            std::string fileName = std::to_string(wavCount + 1) + ".wav";
            filePath.append(fileName);
            wavCount += 1;
        }

        std::printf("Recording to path %s", filePath.c_str());
        std::printf("\nListening to incoming audio\n");
        m_audioHandler.Start(2000);

        bool isRecording = false;

        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (m_audioHandler.IsRecording() && isRecording == false)
            {
                isRecording = true;
                std::printf("Recording started\n");
            }

            if (m_audioHandler.IsFinished())
            {
                break;
            }
        }

        std::printf("Finished recording!\n");
        std::printf("Writing recording to file %s\n\n", filePath.c_str());
        std::fflush(stdout);

        m_audioHandler.Stop(filePath.string());
        if (dirMode == false)
        {
            break;
        }
        while (true)
        {
            std::cout << "Continue? [Y/n]" << std::endl;
            char c = 'y';
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) {
                break;
            }
            c = line[0];
            if (c == 'y' || c == 'Y')
            {
                break;
            }
            else if (c == 'n' || c == 'N')
            {
                std::printf("Bye bye!\n");
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    return 0;
}