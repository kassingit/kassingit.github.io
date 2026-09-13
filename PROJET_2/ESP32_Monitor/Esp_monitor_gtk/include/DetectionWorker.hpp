#pragma once
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <giomm.h>
#include "Detector.hpp"
#include "StreamClient.hpp"

class DetectionWorker {
private:
    Detector detector;
    StreamClient& client;

    std::vector<Detection> latestDetections;
    std::mutex detectionsMutex;

    std::thread workerThread;
    std::atomic<bool> running{false};

public:
    Glib::Dispatcher newDetectionsSignal;

    DetectionWorker(const std::string& modelPath, StreamClient& streamClient);

    void start();
    void stop();

    std::vector<Detection> getLatestDetections();
};