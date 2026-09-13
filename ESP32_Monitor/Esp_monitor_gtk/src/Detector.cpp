#include "Detector.hpp"
#include <iostream>
#include <algorithm>
#include <gdkmm/pixbuf.h>
#include <gdkmm/pixbufloader.h>

Detector::Detector(const std::string& modelPath)
    : env(ORT_LOGGING_LEVEL_WARNING, "yolo_detector")
{
    Ort::SessionOptions sessionOptions;
    session = Ort::Session(env, modelPath.c_str(), sessionOptions);
    std::cout << "[Detector] Modele charge : " << modelPath << std::endl;
}

int Detector::countPersons(const std::vector<Detection>& detections) {
    int count = 0;
    for (const auto& d : detections) {
        if (d.classId == 0) count++;
    }
    return count;
}

std::vector<Detection> Detector::detect(const std::string& jpegBytes) {
    std::vector<Detection> results;

    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    try {
        auto loader = Gdk::PixbufLoader::create();
        loader->write(reinterpret_cast<const guint8*>(jpegBytes.data()), jpegBytes.size());
        loader->close();
        pixbuf = loader->get_pixbuf();
    } catch (const Glib::Error& ex) {
        std::cerr << "[Detector] Erreur decodage JPEG : " << ex.what() << std::endl;
        return results;
    }

    if (!pixbuf) return results;

    int origWidth = pixbuf->get_width();
    int origHeight = pixbuf->get_height();

    auto resized = pixbuf->scale_simple(INPUT_SIZE, INPUT_SIZE, Gdk::InterpType::BILINEAR);
    if (!resized) return results;

    const guint8* pixels = resized->get_pixels();
    int rowStride = resized->get_rowstride();
    int channels = resized->get_n_channels();

    std::vector<float> inputTensorValues(3 * INPUT_SIZE * INPUT_SIZE);

    for (int y = 0; y < INPUT_SIZE; y++) {
        for (int x = 0; x < INPUT_SIZE; x++) {
            const guint8* pixel = pixels + y * rowStride + x * channels;

            float r = pixel[0] / 255.0f;
            float g = pixel[1] / 255.0f;
            float b = pixel[2] / 255.0f;

            inputTensorValues[0 * INPUT_SIZE * INPUT_SIZE + y * INPUT_SIZE + x] = r;
            inputTensorValues[1 * INPUT_SIZE * INPUT_SIZE + y * INPUT_SIZE + x] = g;
            inputTensorValues[2 * INPUT_SIZE * INPUT_SIZE + y * INPUT_SIZE + x] = b;
        }
    }

    std::vector<int64_t> inputShape = {1, 3, INPUT_SIZE, INPUT_SIZE};

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo, inputTensorValues.data(), inputTensorValues.size(),
        inputShape.data(), inputShape.size());

    const char* inputNames[] = {"images"};
    const char* outputNames[] = {"output0"};

    auto outputTensors = session.Run(Ort::RunOptions{nullptr},
        inputNames, &inputTensor, 1,
        outputNames, 1);

    float* outputData = outputTensors[0].GetTensorMutableData<float>();

    float scaleX = static_cast<float>(origWidth) / INPUT_SIZE;
    float scaleY = static_cast<float>(origHeight) / INPUT_SIZE;

    for (int i = 0; i < 300; i++) {
        float x1 = outputData[i * 6 + 0];
        float y1 = outputData[i * 6 + 1];
        float x2 = outputData[i * 6 + 2];
        float y2 = outputData[i * 6 + 3];
        float score = outputData[i * 6 + 4];
        int classId = static_cast<int>(outputData[i * 6 + 5]);

        if (score < CONFIDENCE_THRESHOLD) continue;

        Detection d;
        d.x1 = x1 * scaleX;
        d.y1 = y1 * scaleY;
        d.x2 = x2 * scaleX;
        d.y2 = y2 * scaleY;
        d.score = score;
        d.classId = classId;

        results.push_back(d);
    }

    return results;
}