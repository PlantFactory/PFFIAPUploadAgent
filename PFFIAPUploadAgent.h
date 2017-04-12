//
//  2011/09/19 ver.1   H.Inoue & H.Ochiai
//

// --------- FIAPUploadAgent.h (begin) ---------
#ifndef __FIAPUploadAgent__
#define __FIAPUploadAgent__

#define FIAP_RESPONSE_TIMEOUT 500000 // 500ms

// return code of post method
#define FIAP_UPLOAD_OK       0  // Succeeded
#define FIAP_UPLOAD_CONNFAIL 1  // Connection faild (Socket I/O error)
#define FIAP_UPLOAD_DNSERR   2  // DNS error
#define FIAP_UPLOAD_HTTPERR  3  // HTTP Server error (The response was not "200 OK")
#define FIAP_UPLOAD_FIAPERR  4  // FIAP Server error
#define FIAP_UPLOAD_HTTPTOERR 5  // HTTP Timeout error

#include <Arduino.h>
#include "TimeLib.h"
#include "LocalTimeLib.h"

// point element
struct fiap_element {
  const char* cid;       // ポイントIDのポストフィックス
  char* value;           // 値(文字列)
  time_t time;
  TimeZone* timezone;        // タイムゾーン
};

// class definition
class FIAPUploadAgent {
public:
  void begin(
    String server_host,
    String server_path,
    unsigned short server_port,
    String fiap_id_prefix);
  int post(struct fiap_element* v, byte esize);

private:
  char *element_time_to_str(struct fiap_element* e);

private:
  String server_host;
  String server_path;
  unsigned short server_port;
  String fiap_id_prefix;
};

#endif  // #ifndef FIAPUploadAgent
// --------- FIAPUploadAgent.h (end) ---------
