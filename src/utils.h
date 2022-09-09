#ifndef UTILS_H
#define UTILS_H

#include <onion/log.h>
#include <onion/codecs.h>
#include <onion/onion.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>  // open()
#include <unistd.h> // close()
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <pthread.h>

#define MSG_BUF_SIZE 4096
#define JPEG_BUF_SIZE 1000000

extern const char* pac_username;
extern const char* pac_passwd;
extern pthread_mutex_t mutex_lock;

struct ValveSessionPayload {
  size_t sec;
};
struct CamPayload {
  const char* devicePath;
  uint8_t* jpeg_image;
  int jpeg_image_size;
};

/**
 * @brief If a request is authenticated, neither request nor response will be 
 * modified, the caller should do whatever it wants as if such authentication
 * has never happened. If a request is not authenticated, proper response has been
 * written to res, caller should just return OCS_PROCESSED to its caller.
*/
bool authenticate(onion_request *req, onion_response *res);

int play_sound(const char* sound_path);

void* handle_sound_name_queue();

bool is_file_accessible(const char* file_path);

#endif /* UTILS_H */
