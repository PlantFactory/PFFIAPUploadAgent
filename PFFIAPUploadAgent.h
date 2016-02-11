//
//  2011/09/19 ver.1   H.Inoue & H.Ochiai
//

// --------- FIAPUploadAgent.h (begin) ---------
#ifndef __FIAPUploadAgent__
#define __FIAPUploadAgent__

// return code of post method
#define FIAP_UPLOAD_OK       0  // Succeeded
#define FIAP_UPLOAD_CONNFAIL 1  // Connection faild (Socket I/O error)
#define FIAP_UPLOAD_DNSERR   2  // DNS error
#define FIAP_UPLOAD_HTTPERR  3  // HTTP Server error (The response was not "200 OK")
#define FIAP_UPLOAD_FIAPERR  4  // FIAP Server error

// point element
struct fiap_element {
  const char* cid;       // ポイントIDのポストフィックス
  char* value;           // 値(文字列)
  unsigned short year;         // 年
  byte month;         // 月 1 - 12
  byte day;           // 日 1 - 31
  byte hour;          // 時 0 - 23
  byte minute;        // 分 0 - 59
  byte second;        // 秒 0 - 59
  char* timezone;        // タイムゾーン表記 "+09:00"
};

// class definition
class FIAPUploadAgent {
public: 
  void begin(
             const char* server_host,
             const char* server_path,
             unsigned short server_port,
             const char* fiap_id_prefix);
  int post(struct fiap_element* v, byte esize);
  
private:
  char *element_time_to_str(struct fiap_element* e);
  
private: 
  const char* server_host;
  const char* server_path;
  unsigned short server_port;
  const char* fiap_id_prefix;
};

#endif  // #ifndef FIAPUploadAgent
// --------- FIAPUploadAgent.h (end) ---------
