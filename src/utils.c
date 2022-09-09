#include <onion/log.h>
#include <pthread.h>

#include "utils.h"


const char* pac_username;
const char* pac_passwd;
pthread_mutex_t mutex_lock;

bool authenticate(onion_request *req, onion_response *res) {
  const char *auth_header = onion_request_get_header(req, "Authorization");
  char *auth_header_val = NULL;
  char *supplied_username = NULL;
  char *supplied_passwd = NULL;
  if (auth_header && strncmp(auth_header, "Basic", 5) == 0) {
    auth_header_val = onion_base64_decode(&auth_header[6], NULL);
    supplied_username = auth_header_val;
    int i = 0;
    while (auth_header_val[i] != '\0' && auth_header_val[i] != ':') { i++; }    
    if (auth_header_val[i] == ':') {
        auth_header_val[i] = '\0'; // supplied_username is set to auth_header_val, we terminate auth to make supplied_username work
        supplied_passwd = &auth_header_val[i + 1];
    }
    if (
      supplied_username != NULL && supplied_passwd != NULL &&
      strncmp(supplied_username, pac_username, strlen(pac_username)) == 0 &&
      strncmp(supplied_passwd, pac_passwd, strlen(pac_passwd)) == 0
    ) {
      // C evaluates the && and || in a "short-circuit" manner. That is, for &&, if A in (A && B)
      // is false, B will NOT be evaluated.
      supplied_username = NULL;
      supplied_passwd = NULL;
      free(auth_header_val);
      return true;
    }
  } 

  const char RESPONSE_UNAUTHORIZED[] = "<h1>Unauthorized access</h1>";
  // Not authorized. Ask for it.
  char temp[256];
  sprintf(temp, "Basic realm=PAC");
  onion_response_set_header(res, "WWW-Authenticate", temp);
  onion_response_set_code(res, HTTP_UNAUTHORIZED);
  onion_response_set_length(res, sizeof(RESPONSE_UNAUTHORIZED));
  onion_response_write(res, RESPONSE_UNAUTHORIZED, sizeof(RESPONSE_UNAUTHORIZED));
  return false;
}
