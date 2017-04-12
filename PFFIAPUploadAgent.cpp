//
//  IEEE1888 / FIAP Uploader Library for Arduino
//
//  2011/09/19 ver.1     H.Inoue & H.Ochiai
//  2011/10/15 ver.1.1   H.Ochiai (updated to use PGM for const strings: 572byte SRAM freed)
//  2016/02/10 ver.1.2   Hiromasa Ihara(forked)
//

// --------- PFFIAPUploadAgent.cpp (begin) ---------
#include <Arduino.h>
#include <SPI.h>              // Ethernetシールド用
#include <Ethernet.h>         // Ethernetシールド用
#include <Client.h>           // TCPクライアント用
#include <avr/pgmspace.h>     // Retrieve Strings from the Program Memory
#include "PFFIAPUploadAgent.h"
#include "TimeLib.h"
#include "LocalTimeLib.h"

// void void FIAPUploadAgent::begin( ... );
// Initialize the FIAPUploadAgent instance
//    with specifying server information and PointSetID (=PointID prefix)
void FIAPUploadAgent::begin(
  String server_host,
  String server_path,
  unsigned short server_port,
  String fiap_id_prefix) {

  this->server_host=server_host;
  this->server_path=server_path;
  this->server_port=server_port;
  this->fiap_id_prefix=fiap_id_prefix;
}

// Messages (Stored in the program memory) -- HTTP Header Part
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER01[] =  "POST ";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER02[] =  " HTTP/1.1";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER03[] =  "Content-Type: text/xml charset=UTF-8";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER04[] =  "User-Agent: PFFIAPUploadAgent (Arduino HCU)";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER05[] =  "Host: ";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER06[] =  "SOAPAction: \"http://soap.fiap.org/data\"";
PROGMEM const char FIAPUploadAgent_Post_HTTPHEADER07[] =  "Content-Length: ";

// Messages (Stored in the program memory) -- HTTP Body Part
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY01[] =  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY02[] =  "<soapenv:Envelope xmlns:soapenv=";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY03[] =  "\"http://schemas.xmlsoap.org/soap/envelope/\">";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY04[] =  "<soapenv:Body>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY05[] =  "<ns2:dataRQ xmlns:ns2=\"http://soap.fiap.org/\">";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY06[] =  "<transport xmlns=\"http://gutp.jp/fiap/2009/11/\">";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY07[] =  "<body>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY10[] =  "<point id=\"";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY11[] =  "\">";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY12[] =  "<value time=\"";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY13[] =  "\">";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY14[] =  "</value>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY15[] =  "</point>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY17[] =  "</body>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY18[] =  "</transport>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY19[] =  "</ns2:dataRQ>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY20[] =  "</soapenv:Body>";
PROGMEM const char FIAPUploadAgent_Post_HTTPBODY21[] =  "</soapenv:Envelope>";

// int FIAPUploadAgent::post(struct fiap_element* v, byte esize);
// Upload the data stored in (struct fiap_element[]).
//   esize is the number of the array elements.
//    Return values
//           FIAP_UPLOAD_OK       0
//           FIAP_UPLOAD_CONNFAIL 1  // Connection faild (Socket I/O error)
//           FIAP_UPLOAD_DNSERR   2  // DNS error
//           FIAP_UPLOAD_HTTPERR  3  // HTTP Server error (The response was not "200 OK")
//           FIAP_UPLOAD_FIAPERR  4  // FIAP Server error
int FIAPUploadAgent::post(struct fiap_element* v, byte esize){

  int rescode = 0;  // HTTP response code
  int clen = 0;     // content length
  struct fiap_element *v0;
  char count;
  unsigned char c;

  EthernetClient client;

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
    clen += 44;
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
  while (client.connected()) {
    // Serial.print("C");
    if (client.available()) {
      c = client.read();  // Serial.print(c);
      if (count == 1 && (c >= '0' && c <= '9')) {  // parse HTTP response code
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
  }
  if (!client.connected()) {  // unexpected disconnect
    client.stop();
    return(FIAP_UPLOAD_HTTPERR);
  }

  // disconnect HTTP
  while (client.connected() && client.available()) {
    c = client.read(); // Serial.print(c);  // 応答を最後まで受信
  }
  client.stop();
  if (rescode == 200) {
    return(FIAP_UPLOAD_OK);
  }
  return(FIAP_UPLOAD_HTTPERR);
}

PROGMEM const char FIAPUploadAgent_Post_TimeFormat[] = "%04d-%02d-%02dT%02d:%02d:%02d%s\0";

// [private method] char* FIAPUploadAgent::element_time_to_str(struct fiap_element* e);
// struct fiap_element に指定された時刻から "2011-08-26T10:28:00+00:00" の表記を得る
char* FIAPUploadAgent::element_time_to_str(struct fiap_element* e)
{
  TimeElements* tm;

  static char fiap_time[28];
  char str_timeformat[33];

  tm = localtime(e->time);
  strcpy_P(str_timeformat,FIAPUploadAgent_Post_TimeFormat);
  sprintf(fiap_time, str_timeformat, tm->Year + 1970, tm->Month, tm->Day, tm->Hour, tm->Minute, tm->Second, e->timezone->iso_string);

  return fiap_time;
}
// --------- PFFIAPUploadAgent.cpp (end) ---------
