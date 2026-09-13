#pragma once
#include <giomm.h>   // for Glib::Dispatcher
#include <string>
#include <curl/curl.h>  // for Libcurl
#include <mutex>
#include <thread>
#include <atomic>

class StreamClient {
private:
    std::string url;
    std::string buffer;
    std::string latestFrame;
    std::mutex frameMutex;

    std::thread networkThread;
    std::atomic<bool> shouldStop{false};
    std::atomic<bool> running{false};

public:
    Glib::Dispatcher newFrameSignal; // doorbell, emits signal when new frame is ready

    StreamClient(const std::string& URL);
    void connect();

    void start();   // Run connect() on a new thread
    void stop();    // Correctly shutdown the connection
    void reconnect(const std::string& newUrl);

    std::string getLastFrame();
    std::string getNewUrl() const { return url; }

private:
    bool extractFrame(std::string& outFrame);
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
};