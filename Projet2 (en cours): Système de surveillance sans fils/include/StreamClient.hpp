#pragma once
#include <giomm.h>   // for Glib::Dispatcher
#include <string>
#include <curl/curl.h>  // for Libcurl
#include <mutex>
#include <thread>

class StreamClient {
private:
    std::string url;
    std::string buffer;
    
    std::string latestFrame;
    std::mutex frameMutex;

    // For the reinitialization of the connection
    std::thread networkThread;
    std::atomic<bool> shouldStop{false};



public:
    void start();   // Run connect() on a new Thread
    void stop();    // to correctly shutdown the connection
    void reconnect(std::string newUrl);
    Glib::Dispatcher newFrameSignal; // doorbell, it admit signal when new frame is ready

    std::string getNewUrl()const { return url;}
    StreamClient(const std::string& URL);
    void connect();

    // call by the UI to safely retrieve the new frame
    std::string getLastFrame();

    // PAUSE BUTTON
    void setPaused( bool pause);

private:
    bool extractFrame(std::string& outFrame);
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    std::atomic<bool> paused{false};
};