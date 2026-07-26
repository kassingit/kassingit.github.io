#pragma once
#include <string>
#include <onnxruntime_cxx_api.h>
#include <vector>


struct Detection{
    float x1,y1,x2,y2;
    float score;
    int ClassId;
};

class Detector {
private:
    Ort::Env env;
    Ort::Session session{nullptr};
    Ort::AllocatorWithDefaultOptions allocator;

    static constexpr int INPUT_SIZE = 640;
    static constexpr float CONFIDENCE_THRESHOLD = 0.3f;

public:
    Detector(const std::string& modelPath);

    //TAKE THE OTECT OF THE IMAGE AND RETURN THE LIST OF DETECTION FOUND
    std::vector<Detection> Detect(const std::string& jpegBytes);

    // COUNT HOW MANY DETECTION ARE IN "PERSONNE" CLASS (ClassId = 0)
    int countPersons(const std::vector<Detection>& detection);


};