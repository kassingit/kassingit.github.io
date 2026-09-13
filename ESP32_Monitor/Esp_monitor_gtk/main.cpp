#include <gtkmm.h>
#include "VueG.hpp"
#include "StreamClient.hpp"
#include <curl/curl.h>
#include <iostream>

int main(int argc, char *argv[]) {
    curl_global_init(CURL_GLOBAL_ALL);

    StreamClient client("http://10.175.84.169/stream");
    client.start();

    auto app = Gtk::Application::create();
    int result = app->make_window_and_run<VueG>(argc, argv, client);

    client.stop();
    curl_global_cleanup();
    return result;
}