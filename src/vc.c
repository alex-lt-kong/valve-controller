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

char* bin_path;
char* db_path;
char* public_dir;
char* settings_path;
struct CamPayload cpl;
json_object* root_users;

int get_valve_session_history_json(void *p, onion_request *req, onion_response *res) {
  sqlite3 *db;
  if (sqlite3_open(db_path, &db)) {
    ONION_ERROR("Could not open the.db");
    return 1;
  }

  sqlite3_stmt *stmt;
  if (
    sqlite3_prepare_v2(
      db,
      "SELECT record_id, record_time, username, duration_sec FROM valve_session LIMIT 100",
      -1, &stmt, NULL
    )
  ) {
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
  pthread_mutex_lock(&mutex_image);
  uint8_t* jpeg_copy = (uint8_t*)malloc(cpl.jpeg_image_size);
  if (jpeg_copy == NULL) {
    ONION_ERROR("malloc() failed");
    return onion_shortcut_response("", HTTP_INTERNAL_ERROR, req, res);
  }
  memcpy(jpeg_copy, cpl.jpeg_image, cpl.jpeg_image_size);
  pthread_mutex_unlock(&mutex_image);
  onion_response_set_header(res, "Content-Type", "image/jpg");  
  onion_response_write(res, (char*)jpeg_copy, cpl.jpeg_image_size);
  free(jpeg_copy);
  return OCS_PROCESSED;
}

void* valve_session(void* payload) {
  pthread_mutex_lock(&mutex_valve);
  struct ValveSessionPayload* pl = (struct ValveSessionPayload*)payload;
  gpioSetMode(GPIO_PIN, PI_OUTPUT); //make P0 output
  gpioWrite(GPIO_PIN, PI_HIGH);
  ONION_INFO("Solenoid valve opened by [%s] for [%d] seconds", pl->username, pl->sec);
  sleep(pl->sec);  
  gpioWrite(GPIO_PIN, PI_LOW);
  ONION_INFO("Solenoid valve closed", pl->username);
  pthread_mutex_unlock(&mutex_valve);

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
  free(pl);
  return NULL;
}

int oepn_valve(void *p, onion_request *req, onion_response *res) {
  char* authenticated_user = authenticate(req, res, root_users);
  if (authenticated_user == NULL) {
    ONION_WARNING("Failed login attempt");
    return OCS_PROCESSED;
  }
  char msg[MSG_BUF_SIZE];
  struct ValveSessionPayload* pl = (struct ValveSessionPayload*)malloc(sizeof(struct ValveSessionPayload));
  strcpy(pl->username, authenticated_user);
  free(authenticated_user);

  const char* length_sec_str = onion_request_get_query(req, "length_sec");
  if (length_sec_str == NULL) {
    return onion_shortcut_response("length_sec not specified", HTTP_BAD_REQUEST, req, res);
  }
  if (strnlen(length_sec_str, 5) > 4) {
    snprintf(msg, MSG_BUF_SIZE, "%s is too long for parameter length_sec", length_sec_str);
    return onion_shortcut_response(msg, HTTP_BAD_REQUEST, req, res);
  }
  pl->sec = atoi(length_sec_str);
  if (pl->sec <= 0) {
    snprintf(msg, MSG_BUF_SIZE, "{\"status\":\"error\", \"data\":\"pl.sec == %d, valve session will be skipped\"}", pl->sec);
    ONION_ERROR(msg);
    onion_response_printf(res, msg);
    onion_response_set_code(res, HTTP_BAD_REQUEST);
    return OCS_PROCESSED;
  }
  if (pthread_mutex_trylock(&mutex_valve) == 0) {
    pthread_mutex_unlock(&mutex_valve) ;
    pthread_t tid;
    if (pthread_create(&tid, NULL, valve_session, pl) == 0) {
      snprintf(msg, MSG_BUF_SIZE, 
        "{"
          "\"status\":\"success\", "
          "\"data\":{\"session length\": %d}"
        "}",
        pl->sec
      );
      onion_response_printf(res, msg);
      onion_response_set_code(res, HTTP_OK);
      return OCS_PROCESSED;
    } else {
      pthread_mutex_unlock(&mutex_valve);
      snprintf(msg, MSG_BUF_SIZE, "{\"status\":\"error\", \"data\":\"Failed to create a valve session\"}");
      ONION_ERROR(msg);
      return onion_shortcut_response(msg, HTTP_INTERNAL_ERROR, req, res);
    }
  }
  snprintf(msg, MSG_BUF_SIZE, "{\"status\":\"error\", \"data\":\"Existing valve session sill running\"}");
  ONION_ERROR(msg);
  return onion_shortcut_response(msg, HTTP_BAD_REQUEST, req, res);
  
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
  char file_path[1024] = "";
  const char* file_name = onion_request_get_query(req, "file_name");
  strcpy(file_path, public_dir);
  if (file_name == NULL) {
    strcat(file_path, "html/index.html");
  }
  else if (strcmp(file_name, "vc.js") == 0) {
    strcat(file_path, "js/vc.js");
  } else if (strcmp(file_name, "favicon.svg") == 0) {
    strcat(file_path, "img/favicon.svg");
  } else {
    strcat(file_path, "html/index.html");
  }
  return onion_shortcut_response_file(file_path, req, res);
}


onion *o=NULL;

static void shutdown_server(int sig_num) {
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

void initialize_paths(char* argv) {
  settings_path = (char*)malloc(PATH_MAX);
  bin_path = (char*)malloc(PATH_MAX);
  realpath(argv, bin_path);
  strcpy(settings_path, dirname(bin_path));
  db_path = (char*)malloc(PATH_MAX);
  public_dir = (char*)malloc(PATH_MAX);
  strcpy(public_dir, settings_path);  
  strcpy(db_path, settings_path);
  strcat(settings_path, "/settings.json");
  strcat(db_path, "/data.sqlite");
  strcat(public_dir, "/public/");

  ONION_INFO("public_dir: [%s], db_path: [%s], settings_path: [%s]", public_dir, db_path, settings_path);
}

void free_paths() {
  free(public_dir);
  free(db_path);
  free(bin_path);
  free(settings_path);
}

int main(int argc, char **argv) {
  initialize_paths(argv[0]);
  
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
    log_path == NULL ||  ssl_crt_path == NULL || ssl_key_path == NULL
  ) {
    ONION_ERROR("Either root_app_log_path, ssl_crt_path, ssl_key_path is not set.");
    return 3;
  }
  
  if (argc == 2 && strcmp(argv[1], "--debug") == 0) {
    fprintf(stderr, "Debug mode enabled, the only change is that log will be sent to stderr instead of a log file\n");
  } else {
    freopen(log_path, "a", stderr);
  }

  if (gpioInitialise() < 0) {
    ONION_ERROR("pigpio initialisation failed, program will quit\n");
    json_object_put(root);
    free_paths();
    return 4;
  }
  if (pthread_mutex_init(&mutex_image, NULL) != 0 || pthread_mutex_init(&mutex_valve, NULL) != 0) {
    ONION_ERROR("Failed to initialize a mutex");
    gpioTerminate();
    json_object_put(root);
    free_paths();
    return 5;
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
    pthread_mutex_destroy(&mutex_image);
    pthread_mutex_destroy(&mutex_valve);
    free_paths();
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
  pthread_mutex_destroy(&mutex_image);
  pthread_mutex_destroy(&mutex_valve);
  free_paths();
  return 0;
}
