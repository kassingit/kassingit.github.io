#include "Recorder.hpp"
#include <iostream>

Recorder::Recorder() {}

Recorder::~Recorder() {
    stop();
}

void Recorder::start(const std::string& outputFilePath) {
    if (recording) {
        std::cerr << "[Recorder] Un enregistrement est deja en cours." << std::endl;
        return;
    }

    outputPath = outputFilePath;

    std::string command =
        "ffmpeg -y -f image2pipe -framerate 10 -i - "
        "-c:v libx264 -pix_fmt yuv420p \"" + outputPath + "\" "
        "> /tmp/ffmpeg_log.txt 2>&1";

    ffmpegPipe = popen(command.c_str(), "w");

    if (!ffmpegPipe) {
        std::cerr << "[Recorder] Impossible de lancer ffmpeg." << std::endl;
        return;
    }

    recording = true;
    std::cout << "[Recorder] Enregistrement demarre : " << outputPath << std::endl;
}

void Recorder::stop() {
    if (!recording) return;

    recording = false;

    if (ffmpegPipe) {
        pclose(ffmpegPipe);
        ffmpegPipe = nullptr;
        std::cout << "[Recorder] Enregistrement termine : " << outputPath << std::endl;
    }
}

void Recorder::pushFrame(const std::string& jpegBytes) {
    if (!recording || !ffmpegPipe) return;

    fwrite(jpegBytes.data(), 1, jpegBytes.size(), ffmpegPipe);
    fflush(ffmpegPipe);
}