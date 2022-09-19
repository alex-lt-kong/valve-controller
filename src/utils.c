#include <onion/log.h>
#include <pthread.h>

#include "utils.h"

pthread_mutex_t mutex_image;
pthread_mutex_t mutex_valve;
char* authenticate(onion_request *req, onion_response *res, json_object* users) {
  const char *auth_header = onion_request_get_header(req, "Authorization");
  char *auth_header_val = NULL;
  char *supplied_username = NULL;
  char *supplied_passwd = NULL;
  if (auth_header && strncmp(auth_header, "Basic", 5) == 0) {
    auth_header_val = onion_base64_decode(&auth_header[6], NULL);
    supplied_username = auth_header_val;
    int i = 0;
    while (auth_header_val[i] != '\0' && auth_header_val[i] != ':' && i < MSG_BUF_SIZE + 1) { i++; }    
    if (auth_header_val[i] == ':' && i < MSG_BUF_SIZE) {
        auth_header_val[i] = '\0'; // supplied_username is set to auth_header_val, we terminate auth to make supplied_username work
        supplied_passwd = &auth_header_val[i + 1];
    }
    if (supplied_username != NULL && supplied_passwd != NULL) {
      json_object* users_passwd = json_object_object_get(users, supplied_username);
      
      const char* password = json_object_get_string(users_passwd);
      if (password != NULL && strncmp(supplied_passwd, password, strlen(password)) == 0) {
        char* authenticated_username = malloc(strlen(supplied_username) + 1);
        strcpy(authenticated_username, supplied_username);
        supplied_username = NULL;
        supplied_passwd = NULL;
        free(auth_header_val);
        return authenticated_username;
      }
    }
    free(auth_header_val);
  } 

  const char RESPONSE_UNAUTHORIZED[] = "<h1>Unauthorized access</h1>";
  // Not authorized. Ask for it.
  char temp[256];
  sprintf(temp, "Basic realm=PAC");
  onion_response_set_header(res, "WWW-Authenticate", temp);
  onion_response_set_code(res, HTTP_UNAUTHORIZED);
  onion_response_set_length(res, sizeof(RESPONSE_UNAUTHORIZED));
  onion_response_write(res, RESPONSE_UNAUTHORIZED, sizeof(RESPONSE_UNAUTHORIZED));
  return NULL;
}
