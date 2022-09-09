#include <onion/onion.h>
#include <onion/version.h>
#include <onion/shortcuts.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pigpio.h>
#include <sys/wait.h> /* for waitpid */
/* JSON */
#include <json-c/json.h>
/* Check directory's existence*/
#include <dirent.h>
#include <libgen.h> /* dirname() */
#include <onion/types.h>
#include <onion/response.h>

#include "cam.h"
#include "utils.h"
#define GPIO_PIN 17

struct CamPayload cpl;

int get_live_image_jpg(void *p, onion_request *req, onion_response *res) {
  if (authenticate(req, res) == false) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  if (cpl.jpeg_image_size == 0) {
    return onion_shortcut_response("JPEG image is empty!", HTTP_NOT_FOUND, req, res);
  }
  pthread_mutex_lock(&mutex_lock);
  uint8_t* jpeg_copy = (uint8_t*)malloc(cpl.jpeg_image_size);
  if (jpeg_copy == NULL) {
    ONION_ERROR("malloc() failed");
    return onion_shortcut_response("", HTTP_INTERNAL_ERROR, req, res);
  }
  memcpy(jpeg_copy, cpl.jpeg_image, cpl.jpeg_image_size);
  pthread_mutex_unlock(&mutex_lock);
  onion_response_set_header(res, "Content-Type", "image/jpg");  
  onion_response_write(res, (char*)jpeg_copy, cpl.jpeg_image_size);
  free(jpeg_copy);
  return OCS_PROCESSED;
}

void* valve_session(void* payload) {
  struct ValveSessionPayload* pl = (struct ValveSessionPayload*)payload;
  if (pl->sec <= 0) {
    return NULL;
  }
  gpioSetMode(GPIO_PIN, PI_OUTPUT); //make P0 output
  gpioWrite(GPIO_PIN, PI_HIGH);
  ONION_INFO("Solenoid valve opened");
  sleep(pl->sec);  
  gpioWrite(GPIO_PIN, PI_LOW);
  ONION_INFO("Solenoid valve closed");
  return NULL;
}

int oepn_valve(void *p, onion_request *req, onion_response *res) {
  if (authenticate(req, res) == false) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  char msg[MSG_BUF_SIZE];
  struct ValveSessionPayload pl;
  
  const char* length_sec_str = onion_request_get_query(req, "length_sec");
  if (length_sec_str == NULL) {
    return onion_shortcut_response("length_sec not specified", HTTP_BAD_REQUEST, req, res);
  }
  if (strnlen(length_sec_str, 5) > 4) {
    snprintf(msg, MSG_BUF_SIZE, "%s is too long for parameter length_sec", length_sec_str);
    return onion_shortcut_response(msg, HTTP_BAD_REQUEST, req, res);
  }
  pl.sec = atoi(length_sec_str);
  pthread_t tid;
  if (pthread_create(&tid, NULL, valve_session, &pl) == 0) {
    snprintf(msg, MSG_BUF_SIZE, "{\"status\":\"success\", \"data\":\"Valve session length: %dsec\"}", pl.sec);
    return onion_shortcut_response(msg, HTTP_OK, req, res);
  } else {
    snprintf(msg, MSG_BUF_SIZE, "{\"status\":\"error\", \"data\":\"Failed to create a valve session\"}");
    ONION_ERROR(msg);
    return onion_shortcut_response(msg, HTTP_INTERNAL_ERROR, req, res);
  }
  
}

int index_page(void *p, onion_request *req, onion_response *res) {

  //char err_msg[MSG_BUF_SIZE];
  //char info_msg[MSG_BUF_SIZE];
  if (authenticate(req, res) == false) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  return onion_shortcut_response("Hello world", HTTP_OK, req, res);
}


onion *o=NULL;

static void shutdown_server(int sig_num){
  char msg[MSG_BUF_SIZE];
  snprintf(msg, MSG_BUF_SIZE, "Signal %d received", sig_num);
  ONION_WARNING(msg);
  stop_capture_live_image();
	if (o) {
    ONION_WARNING("stopping ONION server");
		onion_listen_stop(o);
  }
  
}


int main(int argc, char **argv) {

  char bin_path[PATH_MAX], settings_path[PATH_MAX] = "";
 
  realpath(argv[0], bin_path);
  strcpy(settings_path, dirname(bin_path));
  strcat(settings_path, "/settings.json");
  json_object* root = json_object_from_file(settings_path);
  json_object* root_app = json_object_object_get(root, "app");
  json_object* root_app_port = json_object_object_get(root_app, "port");
  json_object* root_app_interface = json_object_object_get(root_app, "interface");
  json_object* root_app_username = json_object_object_get(root_app, "username");
  json_object* root_app_passwd = json_object_object_get(root_app, "passwd");
  json_object* root_app_ssl = json_object_object_get(root_app, "ssl");
  json_object* root_app_ssl_crt_path = json_object_object_get(root_app_ssl, "crt_path");
  json_object* root_app_ssl_key_path = json_object_object_get(root_app_ssl, "key_path");
  json_object* root_app_video_device_path = json_object_object_get(root_app, "video_device_path");
  json_object* root_app_log_path = json_object_object_get(root_app, "log_path");
  
  pac_username = json_object_get_string(root_app_username);
  pac_passwd = json_object_get_string(root_app_passwd);
  const char* log_path = json_object_get_string(root_app_log_path);
  const char* ssl_crt_path = json_object_get_string(root_app_ssl_crt_path);
  const char* ssl_key_path = json_object_get_string(root_app_ssl_key_path);
  cpl.devicePath = json_object_get_string(root_app_video_device_path);
  if (
    log_path ==NULL || pac_username == NULL || pac_passwd == NULL ||
    ssl_crt_path == NULL || ssl_key_path == NULL
  ) {
    ONION_ERROR("Either root_app_log_path, username, passwd, ssl_crt_path, ssl_key_path is not set.");
    return 3;
  }

  if (gpioInitialise() < 0) {
    ONION_ERROR("pigpio initialisation failed, program will quit\n");
    json_object_put(root);
    return 4;
  }
  if (pthread_mutex_init(&mutex_lock, NULL) != 0) {
      ONION_ERROR("Failed to initialize a mutex");
      gpioTerminate();
      json_object_put(root);
      return 5;
  }

  if (argc == 2 && strcmp(argv[1], "--debug") == 0) {
    fprintf(stderr, "Debug mode enabled, the only change is that log will be sent to stderr instead of a log file\n");
  } else {
    freopen(log_path, "a", stderr);
  }

  signal(SIGINT,shutdown_server);
  signal(SIGTERM,shutdown_server);

  ONION_VERSION_IS_COMPATIBLE_OR_ABORT();

  o=onion_new(O_THREADED);
  onion_set_timeout(o, 300 * 1000);
  // We set this to a large number, hoping the client closes the connection itself
  // If the server times out before client does, GnuTLS complains "The TLS connection was non-properly terminated."
  onion_set_certificate(o, O_SSL_CERTIFICATE_KEY, ssl_crt_path, ssl_key_path);
  onion_set_hostname(o, json_object_get_string(root_app_interface));
  onion_set_port(o, json_object_get_string(root_app_port));
  onion_url *urls=onion_root_url(o);
  onion_url_add(urls, "", index_page);
  onion_url_add(urls, "get_live_image_jpg/", get_live_image_jpg);
  onion_url_add(urls, "open_valve/", oepn_valve);
  
  pthread_t tid;
  if (pthread_create(&tid, NULL, thread_capture_live_image, &cpl) != 0) {
    ONION_ERROR("Failed to pthread_create() thread_capture_live_image");
    onion_free(o);
    gpioTerminate();
    json_object_put(root);
    pthread_mutex_destroy(&mutex_lock);
    return 6;
  }

  ONION_INFO(
    "Valve controller listening on %s:%s",
    json_object_get_string(root_app_interface), json_object_get_string(root_app_port)
  );

  struct sigaction act;
  act.sa_handler = shutdown_server;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &act, 0);
  sigaction(SIGABRT, &act, 0);
  sigaction(SIGTERM, &act, 0);

  onion_listen(o);
  ONION_INFO("Onion server quits gracefully");
  pthread_join(tid, NULL);
  onion_free(o);
  gpioTerminate();
  json_object_put(root);
  pthread_mutex_destroy(&mutex_lock);
  freopen("/dev/tty", "a", stderr); // may have side effects, but fit our purpose for the time being.
  return 0;
}
