cmake_minimum_required(VERSION 3.10)

project(MyFFmpegPlayer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_BUILD_TYPE Debug)

add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g -O2 -Wall -D_REENTRANT -fPIC)
add_link_options(-fsanitize=address -fno-omit-frame-pointer)

include_directories(. /opt/homebrew/Cellar/ffmpeg/6.0_1/include/ /Users/zhacgsn/dev/SDL2-2.28.5/include/)

add_executable(MyFFmpegPlayer AudioDecodeThread.cc AudioPlay.cc DemuxThread.cc 
FFmpegPlayer.cc Log.cc main.cc PacketQueue.cc RenderView.cc SDLApp.cc 
ThreadBase.cc Timer.cc VideoDecodeThread.cc)

target_link_directories(MyFFmpegPlayer PRIVATE /opt/homebrew/Cellar/ffmpeg/6.0_1/lib /Users/zhacgsn/dev/SDL2-2.28.5/build/)

target_link_libraries(MyFFmpegPlayer avcodec avformat swscale swresample avutil SDL2 pthread z)