#pragma once
#include <cstdio>
#include <thread>
#include <atomic>
#include <string>

class Recorder {
private:
    FILE* ffmpegPipe = nullptr;
    std::atomic<bool> recording{false};
    std::thread recordThread;
    std::string outputPath;

public:
    Recorder();
    ~Recorder();

    void start(const std::string& outputFilePATH);
    void stop();
    bool isRecording() const {return recording;}

    // CALL TO REGULAR INTERVALLE TO SEND DATA TO FFMPEG
    void pushFrame(const std::string& jpegBytes);


};