#pragma once
#include <string>
#include <vector>
#include </home/kelly/Bureau/Personnal_Projects/ESP32_Monitor/onnxruntime-linux-x64-1.23.0/include/onnxruntime_cxx_api.h>

struct Detection {
    float x1, y1, x2, y2;
    float score;
    int classId;
};

class Detector {
private:
    Ort::Env env;
    Ort::Session session{nullptr};
    Ort::AllocatorWithDefaultOptions allocator;

    static constexpr int INPUT_SIZE = 640;
    static constexpr float CONFIDENCE_THRESHOLD = 0.5f;

public:
    Detector(const std::string& modelPath);

    std::vector<Detection> detect(const std::string& jpegBytes);
    int countPersons(const std::vector<Detection>& detections);
};