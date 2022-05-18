/**
 * @file FFMPEGCapturerSample.cpp
 * @author Alex Li (lizhiqin46783937@live.com)
 * @brief
 * @version 0.1
 * @date 2022-05-15
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include "FFMPEGCapturer.h"

static void printHelp(void) {
  std::cerr << "usage: FFMPEGCapturerSample <device> <frames>" << std::endl;
  std::cerr << "i.e: FFMPEGCapturerSample /dev/video0 10" << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printHelp();
    return -EINVAL;
  }

  FFMPEGCapturerHandle pHandle = ffmpegCapturerOpen(argv[1]);

  if (!pHandle) {
    return -EFAULT;
  }

  int frames = 0;
  try {
    frames = std::stoi(argv[2]);
  } catch (const std::exception &e) {
    printHelp();
    return -EINVAL;
  }

  auto frameBuffer = std::vector<char>(512000);

  for (int i = 1; i <= frames; ++i) {
    std::stringstream fileName;
    fileName << "frame-" << std::setfill('0') << std::setw(3) << i << ".h264";
    std::ofstream stream(fileName.str(), std::ios::out | std::ios::binary);

    size_t frameSize = 0;
    int ret = ffmpegCapturerSyncGetFrame(pHandle, &frameBuffer[0],
                                         frameBuffer.size(), &frameSize);
    if (ret == -EAGAIN) {
      i--;
      stream.close();
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(5ms);
    } else if (ret != 0) {
      stream.close();
      return ret;
    }

    stream.write(&frameBuffer[0], frameSize);
    stream.close();
  }

  ffmpegGCapturerClose(&pHandle);

  return 0;
}
