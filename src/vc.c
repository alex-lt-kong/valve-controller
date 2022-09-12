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
/* Check directory's existence*/
#include <dirent.h>
#include <libgen.h> /* dirname() */
#include <onion/types.h>
#include <onion/response.h>
#include <sqlite3.h>

#include "cam.h"
#include "utils.h"
#define GPIO_PIN 17

char db_path[PATH_MAX];
struct CamPayload cpl;
json_object* root_users;

int get_valve_session_history_json(void *p, onion_request *req, onion_response *res) {
  printf("db_path: %s\n", db_path);
  sqlite3 *db;
  if (sqlite3_open(db_path, &db)) {
    ONION_ERROR("Could not open the.db");
    return 1;
  }

  sqlite3_stmt *stmt;
  if (sqlite3_prepare_v2(db, "SELECT record_id, record_time, username, duration_sec FROM valve_session LIMIT 100", -1, &stmt, NULL)) {
      ONION_ERROR("Error executing SELECT statement");
      sqlite3_close(db);
      return 2;
  }

  char result[65536] = "{\"status\":\"success\",\"data\":[";
  const size_t item_size = 256;
  char item[item_size];
  while (sqlite3_step(stmt) != SQLITE_DONE) {
      snprintf(
        item, item_size,
        "{\"record_id\":%d, \"record_time\":\"%s\", \"username\":\"%s\", \"duration_sec\":%d},",
        sqlite3_column_int(stmt, 0),
        sqlite3_column_text(stmt, 1),
        sqlite3_column_text(stmt, 2),
        sqlite3_column_int(stmt, 3)
      );
      strcat(result, item);
  }
  strcpy(result + strlen(result) - 1, "]}");
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return onion_shortcut_response(result, HTTP_OK, req, res);
}

int get_live_image_jpg(void *p, onion_request *req, onion_response *res) {
  char* authenticated_user = authenticate(req, res, root_users);
  if (authenticated_user == NULL) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  free(authenticated_user);
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


  sqlite3 *db;
  time_t now;
  char msg[MSG_BUF_SIZE];

  const char* sql_create = 
      "CREATE TABLE IF NOT EXISTS valve_session"
      "("
      "  [record_id] INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  [record_time] TEXT,"
      "  [username] TEXT,"
      "  [duration_sec] INTEGER"
      ")";
   const char* sql_insert = "INSERT INTO valve_session"
            "(record_time, username, duration_sec) "
            "VALUES(?, ?, ?);";

  int rc = sqlite3_open(db_path, &db);
  char *sqlite_err_msg = 0;
  if (rc != SQLITE_OK) {
    snprintf(msg, MSG_BUF_SIZE, "Cannot open database [%s]: %s. INSERT will be skipped", db_path, sqlite3_errmsg(db));
    ONION_ERROR(msg);
    sqlite3_close(db);
  }
  rc = sqlite3_exec(db, sql_create, 0, 0, &sqlite_err_msg);      
  if (rc != SQLITE_OK) {
    snprintf(msg, MSG_BUF_SIZE, "SQL error: %s. CREATE is not successful.", sqlite_err_msg);
    ONION_ERROR(msg);
    sqlite3_free(sqlite_err_msg);
    sqlite3_close(db);
  }
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql_insert, 512, &stmt, NULL);
  if(stmt != NULL) {
      time(&now);
      char buf[sizeof("1970-01-01 00:00:00")];
      strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", localtime(&now)); 
      sqlite3_bind_text(stmt, 1, buf, -1, NULL);
      sqlite3_bind_text(stmt, 2, pl->username, -1, NULL);
      sqlite3_bind_int(stmt, 3, pl->sec);
      sqlite3_step(stmt);
      rc = sqlite3_finalize(stmt);
      if (rc != SQLITE_OK) {
        snprintf(msg, MSG_BUF_SIZE, "SQL error: %d. INSERT is not successful.\n", rc);
        ONION_ERROR(msg);
        sqlite3_close(db);
      }
  }
  sqlite3_close(db);
  return NULL;
}

int oepn_valve(void *p, onion_request *req, onion_response *res) {
  char* authenticated_user = authenticate(req, res, root_users);
  if (authenticated_user == NULL) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  char msg[MSG_BUF_SIZE];
  struct ValveSessionPayload pl;
  strcpy(pl.username, authenticated_user);
  free(authenticated_user);

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

int get_logged_in_user_json(void *p, onion_request *req, onion_response *res) {
  char* authenticated_user = authenticate(req, res, root_users);
  if (authenticated_user == NULL) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  char msg[MSG_BUF_SIZE];
  snprintf(msg, MSG_BUF_SIZE, "{\"status\":\"success\",\"data\":\"%s\"}", authenticated_user);
  free(authenticated_user);
  return onion_shortcut_response(msg, HTTP_OK, req, res);
}

int index_page(void *p, onion_request *req, onion_response *res) {

  char* authenticated_user = authenticate(req, res, root_users);
  if (authenticated_user == NULL) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  free(authenticated_user);
  char tmp_dir[PATH_MAX], public_dir[PATH_MAX], file_path[PATH_MAX];
  readlink("/proc/self/exe", tmp_dir, PATH_MAX - 128);
  char* parent_dir = dirname(tmp_dir); // doc exlicitly says we shouldNT free() it.
  //strncpy(public_dir, parent_dir, PATH_MAX - 1);
  // If the length of src is less than n, strncpy() writes an additional NULL characters to dest to ensure that
  // a total of n characters are written.
  // HOWEVER, if there is no null character among the first n character of src, the string placed in dest will
  // not be null-terminated. So strncpy() does not guarantee that the destination string will be NULL terminated.
  // Ref: https://www.geeksforgeeks.org/why-strcpy-and-strncpy-are-not-safe-to-use/
  strncpy(public_dir, parent_dir, PATH_MAX - 1);
  strcpy(public_dir + strnlen(public_dir, PATH_MAX - 1), "/public/");
  const char* file_name = onion_request_get_query(req, "file_name");
  strncpy(file_path, public_dir, PATH_MAX);
  if (file_name == NULL) {
    strcpy(file_path + strnlen(file_path, PATH_MAX), "html/index.html");
  }
  else if (strcmp(file_name, "vc.js") == 0) {
    strcpy(file_path + strnlen(file_path, PATH_MAX), "js/vc.js");
  } else if (strcmp(file_name, "favicon.svg") == 0) {
    strcpy(file_path + strnlen(file_path, PATH_MAX), "img/favicon.svg");
  } else {
    strcpy(file_path + strnlen(file_path, PATH_MAX), "html/index.html");
  }
  return onion_shortcut_response_file(file_path, req, res);
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

void initialize_sig_handler() {
  struct sigaction act;
  act.sa_handler = shutdown_server;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &act, 0);
  sigaction(SIGABRT, &act, 0);
  sigaction(SIGTERM, &act, 0);
}

int main(int argc, char **argv) {

  char bin_path[PATH_MAX], settings_path[PATH_MAX] = "";
 
  realpath(argv[0], bin_path);
  strcpy(settings_path, dirname(bin_path));
  strcpy(db_path, settings_path);
  strcat(settings_path, "/settings.json");
  strcat(db_path, "/data.sqlite");
  json_object* root = json_object_from_file(settings_path);
  json_object* root_app = json_object_object_get(root, "app");
  json_object* root_app_port = json_object_object_get(root_app, "port");
  json_object* root_app_interface = json_object_object_get(root_app, "interface");
  json_object* root_app_ssl = json_object_object_get(root_app, "ssl");
  root_users = json_object_object_get(root_app, "users");
  json_object* root_app_ssl_crt_path = json_object_object_get(root_app_ssl, "crt_path");
  json_object* root_app_ssl_key_path = json_object_object_get(root_app_ssl, "key_path");
  json_object* root_app_video_device_path = json_object_object_get(root_app, "video_device_path");
  json_object* root_app_log_path = json_object_object_get(root_app, "log_path");
  
  const char* log_path = json_object_get_string(root_app_log_path);
  const char* ssl_crt_path = json_object_get_string(root_app_ssl_crt_path);
  const char* ssl_key_path = json_object_get_string(root_app_ssl_key_path);
  cpl.devicePath = json_object_get_string(root_app_video_device_path);
  if (
    log_path ==NULL ||  ssl_crt_path == NULL || ssl_key_path == NULL
  ) {
    ONION_ERROR("Either root_app_log_path, ssl_crt_path, ssl_key_path is not set.");
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
  onion_url_add(urls, "get_logged_in_user_json/", get_logged_in_user_json);
  onion_url_add(urls, "get_valve_session_history_json/", get_valve_session_history_json);
  

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

  initialize_sig_handler();

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
