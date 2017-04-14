//
//  2011/09/19 ver.1   H.Inoue & H.Ochiai
//  2017/04/14 ver.1.2.2 Makoto Uju & Hiromasa Ihara forked and removed ethernetclient
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

#define DEBUG 0

#if DEBUG
#define debug_print(msg) Serial.print(msg)
#define debug_println(msg) Serial.println(msg)
#else
#define debug_print(msg) do{}while(0)
#define debug_println(msg) do{}while(0)
#endif

#define print(a) print(a); debug_print(a)
#define println(a) println(a); debug_println(a)

#include <Arduino.h>
#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

#include "PFFIAPUploadAgent.h"
#include "_protocol_literals.h"

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
template <class ClientT>
class FIAPUploadAgent {
public:
  void begin(
    String server_host,
    String server_path,
    unsigned short server_port,
    String fiap_id_prefix) {
      this->server_host=server_host;
      this->server_path=server_path;
      this->server_port=server_port;
      this->fiap_id_prefix=fiap_id_prefix;
    }
  int post(struct fiap_element* v, byte esize) {
    int rescode = 0;  // HTTP response code
    int clen = 0;     // content length
    struct fiap_element *v0;
    char count;
    unsigned int receive_loop_count;
    unsigned char c;

    // TCP接続開始
    if (!client.connect(server_host.c_str(), server_port)) {
      // TCP接続失敗
      return(FIAP_UPLOAD_CONNFAIL);
    }

    // TCP接続成功
    // コンテンツサイズ計算
    v0 = v;
    clen = 294; // sum of literal strings
    for (count = 0; count < esize; count++) {
      clen += strlen(fiap_id_prefix.c_str());
      clen += strlen(v0->cid);
      clen += strlen(v0->value);
      clen += 69;
      v0++;
    } // Serial.print("len="); Serial.println(clen);

    // sending message buffer
    char sbuf[55];

    // send HTTP header
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPHEADER01);
    client.print(sbuf);  // "POST "
    client.print(server_path.c_str());
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPHEADER02);
    client.println(sbuf); // " HTTP/1.1"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPHEADER03);
    client.println(sbuf); // "Content-Type: text/xml; charset=UTF-8"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPHEADER04);
    client.println(sbuf); // "User-Agent: PFFIAPUploadAgent (Arduino HCU)"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPHEADER05);
    client.print(sbuf); // "Host: "
    client.println(server_host.c_str());
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPHEADER06);
    client.println(sbuf); // "SOAPAction: \"http://soap.fiap.org/data\""
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPHEADER07);
    client.print(sbuf); // "Content-Length: "
    client.println(clen);
    client.println();

    // send HTTP body
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY01);
    client.print(sbuf); // "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY02);
    client.print(sbuf); // "<soapenv:Envelope xmlns:soapenv="
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY03);
    client.print(sbuf); // "\"http://schemas.xmlsoap.org/soap/envelope/\">"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY04);
    client.print(sbuf); // "<soapenv:Body>"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY05);
    client.print(sbuf); // "<ns2:dataRQ xmlns:ns2=\"http://soap.fiap.org/\">"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY06);
    client.print(sbuf); // "<transport xmlns=\"http://gutp.jp/fiap/2009/11/\">"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY07);
    client.print(sbuf); // "<body>"


    v0=v;
    for (count = 0; count < esize; count++) {
      strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY10);
      client.print(sbuf); // "<point id=\""
      client.print(fiap_id_prefix.c_str()); client.print(v0->cid);
      strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY11);
      client.print(sbuf); // "\">"

      strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY12);
      client.print(sbuf); // "<value time=\""
      client.print(element_time_to_str(v0));
      strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY13);
      client.print(sbuf); // "\">"
      client.print(v0->value);

      strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY14);
      client.print(sbuf); // "</value>"

      strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY15);
      client.print(sbuf); // "</point>"
      v0++;
    }
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY17);
    client.print(sbuf); // "</body>"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY18);
    client.print(sbuf); // "</transport>"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY19);
    client.print(sbuf); // "</ns2:dataRQ>"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY20);
    client.print(sbuf); // "</soapenv:Body>"
    strcpy_P(sbuf,FIAPUploadAgent_Post_HTTPBODY21);
    client.print(sbuf); // "</soapenv:Envelope>"
    // client.println();

    // parse HTTP response
    count = 0;
    receive_loop_count = 0;
    while (client.connected()) {
      // Serial.print("C");
      if (client.available()) {
        c = client.read();  // Serial.print(c);
        debug_print(c);

        if (count == 1 && isDigit(c)) {  // parse HTTP response code
          rescode = rescode * 10 + (c - '0');
          continue;
        }
        if (c == ' ') {  // 応答コードの切れ目検出
          count++;
        }
        if (count == 2 || c == '\n') {  // end of HTTP response code
          break;    // 応答ヘッダの2行目以降は見ない
        }
      }
      receive_loop_count++;
      if(receive_loop_count >= FIAP_RESPONSE_TIMEOUT) {
        client.stop();
        return(FIAP_UPLOAD_HTTPTOERR);
      }
      delayMicroseconds(1);
    }
    if (!client.connected()) {  // unexpected disconnect
      client.stop();
      return(FIAP_UPLOAD_HTTPERR);
    }

    // disconnect HTTP
    while (client.connected() && client.available()) {
      c = client.read(); // Serial.print(c);  // 応答を最後まで受信
      debug_print(c);
    }
    client.stop();
    if (rescode == 200) {
      return(FIAP_UPLOAD_OK);
    }
    return(FIAP_UPLOAD_HTTPERR);
  }

private:
  char *element_time_to_str(struct fiap_element* e) {
    TimeElements* tm;

    static char fiap_time[28];
    char str_timeformat[33];

    tm = localtime(e->time);
    strcpy_P(str_timeformat,FIAPUploadAgent_Post_TimeFormat);
    sprintf(fiap_time, str_timeformat, tm->Year + 1970, tm->Month, tm->Day, tm->Hour, tm->Minute, tm->Second, e->timezone->iso_string);

    return fiap_time;
  }

private:
  String server_host;
  String server_path;
  unsigned short server_port;
  String fiap_id_prefix;
  ClientT client;
};

#endif  // #ifndef FIAPUploadAgent
// --------- FIAPUploadAgent.h (end) ---------
