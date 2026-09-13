#include "StreamClient.hpp"
#include <iostream>

StreamClient::StreamClient(const std::string& URL) : url(URL) {}

void StreamClient::connect() {
    std::cerr << "[DEBUG] Starting connection to : " << url << std::endl;
    CURL* curl = curl_easy_init();

    if (!curl) {
        std::cerr << "[ERROR] Failed to initialize Libcurl !" << std::endl;
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

    CURLcode res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[CURL ERROR] " << curl_easy_strerror(res) << std::endl;
    } else {
        std::cerr << "[DEBUG] Connection completed" << std::endl;
    }
}

bool StreamClient::extractFrame(std::string& outFrame) {
    size_t labelPos = buffer.find("Content-Length: ");
    if (labelPos == std::string::npos) { return false; }

    size_t numStart = labelPos + std::string("Content-Length: ").length();
    size_t numEnd = buffer.find("\r\n", numStart);
    std::string numStr = buffer.substr(numStart, numEnd - numStart);
    int length = std::stoi(numStr);

    size_t headerEnd = buffer.find("\r\n\r\n", labelPos);
    if (headerEnd == std::string::npos) { return false; }
    size_t dataStart = headerEnd + 4;

    if (buffer.size() < dataStart + static_cast<size_t>(length)) { return false; }

    outFrame = buffer.substr(dataStart, length);
    buffer.erase(0, dataStart + length);
    return true;
}

size_t StreamClient::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    StreamClient* self = static_cast<StreamClient*>(userdata);

    if (self->shouldStop) {
        return 0;
    }

    size_t totalBytes = size * nmemb;
    self->buffer.append(ptr, totalBytes);

    std::string frame;
    while (self->extractFrame(frame)) {
        {
            std::lock_guard<std::mutex> lock(self->frameMutex);
            self->latestFrame = frame;
        }
        self->newFrameSignal.emit();
    }

    return totalBytes;
}

std::string StreamClient::getLastFrame() {
    std::lock_guard<std::mutex> lock(frameMutex);
    return latestFrame;
}

void StreamClient::start() {
    if (running) {
        return;
    }
    shouldStop = false;
    running = true;
    networkThread = std::thread([this]() {
        connect();
    });
}

void StreamClient::stop() {
    shouldStop = true;
    if (networkThread.joinable()) {
        networkThread.join();
    }
    running = false;
}

void StreamClient::reconnect(const std::string& newUrl) {
    stop();
    url = newUrl;
    buffer.clear();
    start();
}