#include "DetectionWorker.hpp"
#include <iostream>
#include <chrono>

DetectionWorker::DetectionWorker(const std::string& modelPath, StreamClient& streamClient)
    : detector(modelPath), client(streamClient)
{
}

void DetectionWorker::start() {
    if (running) {
        std::cerr << "[DetectionWorker] Deja en cours d'execution." << std::endl;
        return;
    }

    running = true;
    workerThread = std::thread([this]() {
        while (running) {
            std::string frame = client.getLastFrame();

            if (!frame.empty()) {
                auto detections = detector.detect(frame);

                {
                    std::lock_guard<std::mutex> lock(detectionsMutex);
                    latestDetections = detections;
                }

                newDetectionsSignal.emit();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    std::cout << "[DetectionWorker] Thread de detection demarre." << std::endl;
}

void DetectionWorker::stop() {
    running = false;
    if (workerThread.joinable()) {
        workerThread.join();
    }
    std::cout << "[DetectionWorker] Thread de detection arrete." << std::endl;
}

std::vector<Detection> DetectionWorker::getLatestDetections() {
    std::lock_guard<std::mutex> lock(detectionsMutex);
    return latestDetections;
}