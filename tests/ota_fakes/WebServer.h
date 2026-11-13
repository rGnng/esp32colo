#pragma once
#include "Arduino.h"
#include <functional>
#include <string>
enum { HTTP_GET, HTTP_POST, BASIC_AUTH };
enum { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { int status = 0; uint8_t *buf = nullptr; size_t currentSize = 0, totalSize = 0; };
class WebServer {
 public:
  bool authorized = true;
  int response = 0;
  std::string md5;
  HTTPUpload input;
  std::function<void()> get, finish, chunk;
  void collectHeaders(const char **, int) {}
  void on(const char *, int method, std::function<void()> done) { if (method == HTTP_GET) get = done; }
  void on(const char *, int, std::function<void()> done, std::function<void()> stream) { finish = done; chunk = stream; }
  bool authenticate(const char *user, const char *password) { return authorized && std::string(user) == "admin" && std::string(password) == "test-password"; }
  void send(int status, const char *, const char *) { response = status; }
  void send_P(int status, const char *, const char *) { response = status; }
  void requestAuthentication(int, const char *) { response = 401; }
  HTTPUpload &upload() { return input; }
  String header(const char *) { return md5.c_str(); }
};
