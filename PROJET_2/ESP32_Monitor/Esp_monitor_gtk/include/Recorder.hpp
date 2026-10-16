#pragma once
#include <string>
#include <cstdio>
#include <atomic>
#include <thread>

class Recorder {
private:
    FILE* ffmpegPipe = nullptr;
    std::atomic<bool> recording{false};
    std::thread recordThread;
    std::string outputPath;

public:
    Recorder();
    ~Recorder();

    void start(const std::string& outputFilePath);
    void stop();
    bool isRecording() const { return recording; }

    void pushFrame(const std::string& jpegBytes);
};