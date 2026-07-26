#include "Recorder.hpp"
#include <iostream>

Recorder::Recorder(){};
Recorder::~Recorder(){ stop();}

void Recorder::start(const std::string& outputFilePath){
    if(recording)
    {
        std::cerr <<"[INFO] IN START ( recorder.cpp) : A RECORDING IS IN PROGRESS " <<std::endl;
    }

    outputPath = outputFilePath;

    //command ffmpeg: reads images from stdin ( image2pipe) at 10 images by second,
    //encodes into H.264 in a mp4 container
    std::string command =
        "ffmpeg -y -f image2pipe -framerate 10 -i - "
        "-c:v libx264 -pix_fmt yuv420p \"" + outputPath + "\" "
        "> /tmp/ffmpeg_log.txt 2>&1";

    ffmpegPipe = popen(command.c_str(),"w");

    if(!ffmpegPipe){
        std::cerr <<"[DEBUG] IN START ( RECORDER.CPP ): IMPOSSIBLE TO RUN ffmpeg. "<< std::endl;
        return;
    }

    recording = true;
    std::cout <<"[RECODER] : RECORDING HAS STARTED ! "<< outputPath <<std::endl;
}

void Recorder::stop(){
    if(!recording) return;

    recording = false;

    if(ffmpegPipe){
        pclose(ffmpegPipe); // close the pipe and wait for ffmpeg to finish the encoding
        ffmpegPipe = nullptr;
        std::cout <<"[INFO] IN STOP ( RECORDER.CPP) : RECORDING FINISHED "<< outputPath <<std::endl;

    }
}

void Recorder::pushFrame(const std::string& jpegBytes){
    if(!recording || !ffmpegPipe) return; // we must run pushFrame while recording

    fwrite(jpegBytes.data(),1,jpegBytes.size(),ffmpegPipe);
    fflush(ffmpegPipe);

}

