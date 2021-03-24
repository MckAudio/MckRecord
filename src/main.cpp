#include <cstdio>
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <regex>

#include "AudioHandler.hpp"

namespace fs = std::filesystem;

int main(int argc, char **argv)
{
    std::printf(">> Welcome to MCK-Record <<\n\n");

    if (argc < 2)
    {
        std::fprintf(stderr, "No filepath provided, I need a a filename!");
        return 1;
    }

    // #1 - Open Audio Device
    AudioHandler m_audioHandler;
    m_audioHandler.Init();

    if (std::string(argv[1]) == "--info")
    {
        fflush(stdout);
        return 0;
    }

    // #2 - Choose an interface
    std::cout << "Please choose an interface idx:" << std::endl;
    int interfaceIndex = 0;
    std::string ifaceLine = "";
    std::getline(std::cin, ifaceLine);
    if (ifaceLine.empty()) {
        std::cout << "Invalid index" << std::endl;
        return 1;
    }
    interfaceIndex = std::stoi(ifaceLine) - 1;
    if (interfaceIndex < 0 || interfaceIndex >= m_audioHandler.GetDeviceCount())
    {
        std::cout << "Invalid index: " << (interfaceIndex + 1) << std::endl;
        return 1;
    }
    std::cin.clear();

    if (m_audioHandler.OpenInterface(interfaceIndex, 5000) == false) {
        return 1;
    }

    // #3 - File path
    fs::path argPath(argv[1]);
    if (argPath.is_relative())
    {
        argPath = fs::absolute(argPath);
    }

    // #4 - Check path
    bool dirMode = false;
    unsigned wavCount = 0;
    std::string prefix = "";
    if (fs::is_directory(argPath) || argPath.has_extension() == false)
    {
        if (fs::exists(argPath) == false)
        {
            fs::create_directories(argPath);
        }
        else if (argc >= 3)
        {
            prefix = std::string(argv[2]) + "_";
            std::regex preRegex(prefix);
            std::cmatch matchRegex;

            for (auto &p : fs::directory_iterator(argPath))
            {
                if (p.is_regular_file())
                {
                    if (p.path().extension() == ".wav")
                    {
                        if (std::regex_search(p.path().filename().c_str(), matchRegex, preRegex)) {
                            wavCount += 1;
                        }
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
            std::string strIdx = std::to_string(wavCount + 1);
            while(strIdx.size() < 3) {
                strIdx = "0" + strIdx;
            }
            std::string fileName = prefix + strIdx + ".wav";
            filePath.append(fileName);
        }

        std::printf("Recording to path %s", filePath.c_str());
        std::printf("\nListening to incoming audio\n");
        if (m_audioHandler.Start() == false)
        {
            return 1;
        }

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

        if (m_audioHandler.Stop(filePath.string()))
        {
            std::printf("Recording was successfully saved to file %s\n\n", filePath.c_str());
        } else {
            std::fprintf(stderr, "Recording clipped, please repeat the recording!\n");
        }
        
        std::fflush(stdout);

        if (dirMode == false)
        {
            break;
        }
        while (true)
        {
            std::cout << "Continue, repeat or quit? [C/r/q]" << std::endl;
            char c = 'c';
            std::string line;
            std::cin.clear();
            std::getline(std::cin, line);
            if (line.empty()) {
                wavCount += 1;
                break;
            }
            c = line[0];
            if (c == 'c' || c == 'C')
            {
                wavCount += 1;
                break;
            } else if (c == 'r' || c == 'R')
            {
                break;
            }
            else if (c == 'q' || c == 'Q')
            {
                std::printf("Bye bye!\n");
                return 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    return 0;
}