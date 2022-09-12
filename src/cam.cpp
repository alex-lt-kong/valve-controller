#include <opencv4/opencv2/core/core.hpp>
#include <opencv4/opencv2/highgui/highgui.hpp>
#include <opencv4/opencv2/imgproc/imgproc.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <unistd.h>
#include <libgen.h>
#include <iostream>
#include "cam.h"
#include "utils.h"

#define IMAGES_PATH_SIZE 5432

using namespace std;
using namespace cv;

volatile int done = 0;


void stop_capture_live_image() {
  ONION_INFO("stop signal sent to thread_capture_live_image()");
  done = 1;
}

void* thread_capture_live_image(void* payload) {
  struct CamPayload* pl = (struct CamPayload*)payload;
  char msg[MSG_BUF_SIZE];
  ONION_INFO("thread_capture_live_image() started.");
  Mat frame;
  bool result = false;
  VideoCapture cap;
  time_t now;

  char dt_buf[] = "19700101-000000";
  std::vector<uint8_t> buf = {};
  std::vector<int> s = {};
  const uint16_t interval = 2;
  uint32_t iter = interval;
  while (!done) {
    
    if (cap.isOpened() == false || result == false) {
      snprintf(msg, MSG_BUF_SIZE, "cap.open(%s)'ing...", pl->devicePath);
      ONION_INFO(msg);
      result = cap.open(pl->devicePath);
      cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M', 'J', 'P', 'G')); 
      cap.set(CAP_PROP_FRAME_WIDTH, 1280);
      cap.set(CAP_PROP_FRAME_HEIGHT, 720);
      if (cap.isOpened()) {
        snprintf(msg, MSG_BUF_SIZE, "cap.open(%s)'ed", pl->devicePath);
        ONION_INFO(msg);
      }
    }
    if (!result) {
      ONION_ERROR("cap.open() failed");
      sleep(1);
      continue;
    }    
    result = cap.read(frame);
    if (!result) {
      ONION_ERROR("cap.read() failed");
      sleep(1);
      continue;
    }
    ++iter;
    if (iter < interval) { continue; }
    iter = 0;
    rotate(frame, frame, ROTATE_180);
    uint16_t font_scale = 1;
    time(&now);
    strftime(dt_buf, sizeof(dt_buf), "%Y%m%d-%H%M%S", localtime(&now));
    //Size textSize = getTextSize(dt_buf, FONT_HERSHEY_DUPLEX, font_scale, 8 * font_scale, nullptr);
   //putText(
    //  frame, dt_buf, Point(5, textSize.height * 1.05), FONT_HERSHEY_DUPLEX, font_scale, Scalar(0,  0,  0  ), 8 * font_scale, LINE_AA, false
    //);
    imencode(".jpg", frame, buf, s);
    pthread_mutex_lock(&mutex_lock);
    if (pl->jpeg_image_size > 0) {
      pl->jpeg_image_size = 0;
      free(pl->jpeg_image);
    }
    pl->jpeg_image = (uint8_t*)malloc(buf.size());
    pl->jpeg_image_size = buf.size();
    memcpy(pl->jpeg_image, &(buf[0]), buf.size());
    pthread_mutex_unlock(&mutex_lock);
  }
  cap.release();
  ONION_INFO("thread_capture_live_image() quits gracefully");
  return NULL;
}
