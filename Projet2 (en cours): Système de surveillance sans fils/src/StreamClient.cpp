#include "StreamClient.hpp"
#include <iostream>
#include "VueG.hpp"
StreamClient::StreamClient(const std::string& URL):url(URL){}

void StreamClient::connect(){
    std::cerr << "[DEBUG] Starting connection to : " <<url <<std::endl;

    CURL * curl = curl_easy_init(); 
    // curl_easy_init() creates and returns a new "easy handle" — an opaque object
    // that holds all the configuration (URL, callbacks, options) for a single
    // transfer. Returns nullptr if initialization fails (e.g. memory allocation issue).

    //------------- initialization test 
    if (!curl)
    {
    std::cerr << "[ERROR] Failed to initialize Libcurl !" <<std::endl;
    return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // CURLOPT_URL tells curl which address to connect to and request data from.
    // Must be a C-style string (const char*), hence url.c_str().

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    // CURLOPT_WRITEFUNCTION registers the function curl should call every time
    // it receives a chunk of data from the server. Without this, curl would use
    // its default behavior (writing raw bytes to stdout).

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    // CURLOPT_WRITEDATA passes a pointer of our choosing (here, "this" — the
    // current StreamClient instance) that curl will hand back as the last
    // argument ("userdata") every time it calls our write callback. This is how
    // a static/C-style callback can still reach back into our object's state.


    CURLcode res = curl_easy_perform(curl);
    // curl_easy_perform() actually executes the request using all the options
    // configured above: it opens the connection, sends the HTTP request, and
    // repeatedly invokes our write callback as data arrives. This call blocks
    // until the transfer ends (for our infinite MJPEG stream, that means it
    // blocks until the connection is closed or an error occurs).


    curl_easy_cleanup(curl); //------ curl_easy_cleanup() allow to ...
    // curl_easy_cleanup() releases all resources associated with this handle
    // (memory, open connections, etc). Must be called once you're done with
    // the handle to avoid leaks.

    if (res != CURLE_OK)
    {
    std::cerr << "[CURL ERROR] " << curl_easy_strerror(res) <<std::endl;
    }
    else{ std::cerr << "[DEBUG]  Connection completed" << std::endl;}
    }

/// ------------ FOR TH REINITIALIZATION OPTION OF MENU-------------------
void StreamClient::start(){
    shouldStop = false;

    networkThread = std::thread([this](){ connect();});
}

void StreamClient::stop(){
    if(networkThread.joinable()){ 
        networkThread.join();// Wait the Thread to shutdown correctly
    }
}

void StreamClient::reconnect(std::string newUrl){
    stop(); // Shutdown correctly the thread
    url = newUrl; // replace the last url by the new one
    buffer.clear(); // enpty the partial buffer of the old connection
    start(); // Run a new connexion
}



//-----------------------------------
bool StreamClient::extractFrame(std::string& outFrame){
    size_t labelPos = buffer.find("Content-Length: ");
    if (labelPos == std::string::npos){ return false;} // header not found - we need more data

    size_t numStart = labelPos + std::string("Content-Length: ").length(); // we want to count after "Content-Lenght: "
    size_t numEnd = buffer.find("\r\n",numStart);
    std::string numStr = buffer.substr(numStart, numEnd - numStart);
    int length = std::stoi(numStr);

    size_t headerEnd = buffer.find("\r\n\r\n", labelPos);
    if (headerEnd == std::string::npos){ return false;}

    size_t dataStart = headerEnd + 4;

    if (buffer.size() < dataStart + static_cast<size_t>(length)){ return false;} // not enough data have arrived

    // Frame extraction
    outFrame = buffer.substr(dataStart,length);
    buffer.erase(0,dataStart+length);

    return true;
}

size_t StreamClient::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    StreamClient* self = static_cast<StreamClient*>(userdata);

    if(self->shouldStop){
        return 0; // return a  value different from size*nmemb
    }

    size_t totalBytes = size * nmemb;
    self->buffer.append(ptr, totalBytes);


    std::string frame;
    while(self->extractFrame(frame))
    {
        { 
            std::lock_guard<std::mutex> lock(self->frameMutex);
            self->latestFrame = frame;
        }
        if(!self->paused){
        self->newFrameSignal.emit(); // notify the UI thread that the new frame is ready
        }
    }

    return totalBytes;
}

std::string StreamClient::getLastFrame(){
    std::lock_guard<std::mutex> lock(frameMutex); 
    // UI thread lock the shared part until UI finish 
    // to retrieve and lock again the the networking thread the new frame
    return latestFrame;     // return a copy (safely while holding the lock)
}

void StreamClient::setPaused(bool pause){
    this->paused = pause;
    }
