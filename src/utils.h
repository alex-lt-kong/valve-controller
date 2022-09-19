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
/* JSON */
#include <json-c/json.h>

#define MSG_BUF_SIZE 512

extern pthread_mutex_t mutex_image;
extern pthread_mutex_t mutex_valve;

struct ValveSessionPayload {
  size_t sec;
  char username[MSG_BUF_SIZE];
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
 * @param req: the onion_request structure as passed by ONION
 * @param res: the onion_response structure as passed by ONION
 * @param users: the json_object structure as defined in <json-c/json.h>, storing all username/password combinations
 * as JSON object
 * @returns if a crediential is authenticated, returns a char pointer to the user being authenticated, else returns NULL
 * Users needs to free() the returned const char* themselves
*/
char* authenticate(onion_request *req, onion_response *res, json_object* users);

int play_sound(const char* sound_path);

void* handle_sound_name_queue();

bool is_file_accessible(const char* file_path);

#endif /* UTILS_H */
