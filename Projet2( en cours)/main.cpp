#include <gtkmm.h>
#include "VueG.hpp"
#include "StreamClient.hpp"
#include <curl/curl.h>
#include <iostream>
#include <thread>
int main(int argc, char *argv[]) {
    curl_global_init(CURL_GLOBAL_ALL);

    StreamClient client("http://10.116.149.169/stream");

    // Launch the networking work on its own thread, so it doesn't block
    // the GTK event loop (which needs to keep running on the main thread
    // to stay responsive).
    //std::thread networkThread([&client](){ client.connect();});

    client.start();
    Gio::init();
    auto app = Gtk::Application::create();
    int result = app->make_window_and_run<VueG>(argc, argv, client);

    // Once the window closes and make_window_and_run returns, we should
    // clean up the background thread before exiting the program.
   // networkThread.detach();

    client.stop();
    curl_global_cleanup();

    return result;
}