// FIAPsensorsCLI for Arduino-1.0.x
// create: 2012-05-30
// update: 2012-06-08
// update: 2013-05-22 -- support for 1.0.x (added udp.parsePacket())
//
// 【説明】
// IEEE1888(FIAP)のプロトコルでアップロードを行う機能を持ちます。
// この版では、センサ自身の固定IPアドレス、ネットマスク、デフォルトゲートウェイ、
// またサーバのIPアドレスなどの情報を、後から、シリアルコンソールで設定することができます。
//
//【コンパイルと実行の方法】
// １．次のライブラリを事前にArduino IDEのlibrariesフォルダへインストールしておきます
//   ・FIAPUploadAgentライブラリ
//   ・Timeライブラリ
//  ２．Arduino IDEでコンパイル＆ダウンロードを行います。
//  ３．Arduino IDEのSerial Monitorを起動して、改行文字を Carrige Return モードにし、コマンド発行。
//
//     ">" は、実行モードであることを意味し、"#"は設定モードであることを意味します。
//     通信などの失敗で、時刻合わせに失敗すると、設定モードで稼働します(起動後10秒程度待つ必要があります)。
//
//     "?" や "help" コマンドを発行すると、利用可能なコマンド群が表示されます。
//     設定変更後、"save"コマンドを実行し、ボード全体を再起動してください。
//
//      また、LEDの状態（下記参照）から動作が正常かどうかを確認し、
//      サーバ上でアップロードしたデータが参照できるかどうかを確認してください。
//
// 【RGB LEDの色の意味】
// ・白色 … NTPによる時刻情報の取得中
// ・青色 … サーバへのアクセスおよびアップロード処理中
// ・緑色 … アップロード成功
// ・消灯 … 次の測定までの待機中
// ・紫色 … サーバへの接続失敗（TCP接続失敗）
// ・黄色 … HTTP通信エラー
// ・水色 … IEEE1888通信エラー
// ・赤色 … その他のエラー

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <FIAPUploadAgent.h>
#include <Time.h>
#include <Dhcp.h>
#include <Dns.h>
#include <EEPROM.h>

#define MAX_SERVER_HOST_LEN 64
#define MAX_SERVER_PATH_LEN 64
#define MAX_FIAP_ID_PREFIX_LEN 128
#define FIRMWARE_VERSION 0x10

// 全体パラメータ
const unsigned int MEASURE_INTERVAL = 60;  // 測定値のアップロード間隔(秒)
char timezone[] = "+09:00";          // JST
const long timezoneOffset = +32400L; // 上記の時間を秒に直したもの（必ず同じにすること）
const byte LED_ON = 25;              // LED点灯時の明るさ(1-255)

// 固有設定
byte local_mac[] = { 0x02, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MACアドレス
char fiap_id_prefix[MAX_FIAP_ID_PREFIX_LEN] = "http://gutp.jp/Arduino/test-001/"; // PointSet ID (=PointIDのプレフィックス) の設定

// ローカル・ネットワーク設定
IPAddress local_ip4;
IPAddress local_gateway;
IPAddress local_netmask;
IPAddress dns_addr;
boolean dhcp_e = true;

// IEEE1888サーバ設定
char server_host[MAX_SERVER_HOST_LEN] = "fiap-sandbox.gutp.ic.i.u-tokyo.ac.jp";
char server_path[MAX_SERVER_PATH_LEN] = "/axis2/services/FIAPStorage";
int server_port = 80;
byte server_ip4[4];   // DNSで解決したIPアドレスが入る

// UDP (NTPサーバとの通信用)
EthernetUDP udp;

// temperature sensor port
#define TEMP_SENS   1
#define TEMP_COEF   10.0  // conversion parameter (mV per ℃)

// light intensity sensor port
#define LIGHT_SENS  0
#define LIGHT_COEF  0.35  // conversion parameter (mV per Lux)

// toggle SW port
#define SW_TOGGLE   16

// DIP SW ports
#define SW_DIP_1    4
#define SW_DIP_2    7
#define SW_DIP_3    8
#define SW_DIP_4    9

// RGB LED ports
#define LED_POLARITY  1	// 0: cathode common, 1: anode common
#define LED_R   3	// Red
#define LED_G   5	// Green
#define LED_B   6	// Blue

// センサ測定値保持フィールド
float temperature;
char str_temperature[7];
float illuminance;
char str_illuminance[7];
int dipsw;
char str_dipsw[5];
int tglsw;
char str_tglsw[5];

// 各ポイントの設定
struct fiap_element element[]=
{
  {"Temperature", str_temperature, 0,0,0,0,0,0,timezone},
  {"Illuminance", str_illuminance, 0,0,0,0,0,0,timezone},
  {"DIPSW", str_dipsw, 0,0,0,0,0,0,timezone},
  {"TGLSW", str_tglsw, 0,0,0,0,0,0,timezone},
};

// NTPサーバ設定
static byte ntp_server1[4] = { 133, 243, 238, 243 };   // 初期設定値
static byte ntp_server2[4] = { 192, 43, 244, 18 };     // 初期設定値

// 動作モード と シリアル文字入力バッファ (parseConfigで利用)
static byte config_mode=0;    // 0 -- 実行モード, 1 -- 設定モード
static char inLine[135];       // シリアル入力バッファ(1行分)
static byte nInLine=0;        // inLineに格納されている文字数
long serial_check_count = 0;  // シリアル入力が無く、Serial.available()がトライされた回数;
                              // 一定時間入力がなかった場合に、NTPをするため
// 前回アップロード時刻
static time_t lastuploaded_time=0;

// FIAPUploadAgent のインスタンス
FIAPUploadAgent FIAP;

// boot時に出力されるメッセージ (プログラムメモリ上に配置)
PROGMEM prog_char bootProcessHeader1[] = "Starting IEEE1888 Learning Kit Ver.1.0 ... ";

// 初期設定ルーチン
void setup()
{
  int i;
  char outLine[60];

  // EEPROM からの 設定読出し
  loadConfig();

  // I/O初期化
  Serial.begin(9600);
  pinMode(TEMP_SENS, INPUT);
  pinMode(LIGHT_SENS, INPUT);
  pinMode(SW_TOGGLE, INPUT);
  pinMode(SW_DIP_1, INPUT);
  pinMode(SW_DIP_2, INPUT);
  pinMode(SW_DIP_3, INPUT);
  pinMode(SW_DIP_4, INPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  led_rgb_set(0, 0, 0);  // LED off

  // PROGMEM prog_char bootProcessHeader1[] = "Starting IEEE1888 Learning Kit Ver.1.0 ... ";
  strcpy_P(outLine,bootProcessHeader1);
  Serial.println(outLine);

  ethernet_init();

  // FIAPライブラリの初期化
  FIAP.begin(server_host, server_path, server_port, fiap_id_prefix);

  // 入力の受け付け開始
  if(config_mode){
    Serial.print("# ");
  }else{
    Serial.print("> ");
  }

}

// イーサネット関連の初期化：dhcpによるアドレス取得
// 注：アドレスが取得できるまで戻らない
PROGMEM prog_char pgstr_DHCP_requesting[] = "DHCP requesting ... \0";
PROGMEM prog_char pgstr_NTP_requesting[] = "NTP requesting ... \0";
PROGMEM prog_char pgstr_fail[] = "failure\0";
PROGMEM prog_char pgstr_success[] = "success\0";
PROGMEM prog_char pgstr_get_into_confmode[] = "Getting into 'conf' mode\0";
void ethernet_init()
{
  char tmpbuf[30];
  byte dhcp_state;
  byte dns_state;
  boolean blink = true;

  // DHCP request to acquire IP & DNS address
  if(dhcp_e){
    strcpy_P(tmpbuf,pgstr_DHCP_requesting);
    Serial.print(tmpbuf);
    if(Ethernet.begin(local_mac)!=0){
      strcpy_P(tmpbuf,pgstr_success);
      Serial.println(tmpbuf);
    }else{
      strcpy_P(tmpbuf,pgstr_fail);
      Serial.println(tmpbuf);
    }
  }else{
    Ethernet.begin(local_mac, local_ip4, dns_addr, local_gateway, local_netmask);
  }

  // address gotten!
  local_ip4 = Ethernet.localIP();
  local_gateway = Ethernet.gatewayIP();
  local_netmask = Ethernet.subnetMask();
  dns_addr = Ethernet.dnsServerIP();

  led_rgb_set(0, 0, 0);  // LED off

  // NTPサーバへの時刻問合せ
  led_rgb_set(LED_ON, LED_ON, LED_ON);  // LED white
  strcpy_P(tmpbuf,pgstr_NTP_requesting);
  Serial.print(tmpbuf);
  udp.begin(8888);    // for NTP client
  setSyncProvider(NTPqueryTime);
  if (timeStatus() == timeNotSet) {
    time_t t = now();
    config_mode=1;
    strcpy_P(tmpbuf,pgstr_fail);
    Serial.println(tmpbuf);
    strcpy_P(tmpbuf,pgstr_get_into_confmode);
    Serial.println(tmpbuf);
  }else{
    strcpy_P(tmpbuf,pgstr_success);
    Serial.println(tmpbuf);
  }
  led_rgb_set(0, 0, 0);  // LED off
}

// メインループ
PROGMEM prog_char pgstr_loop_msg1[] = "Trying NTP query again ... \0";
void loop()
{
  static long lcount = 0;  // loop counter
  int i;
  time_t t;

  if(config_mode || timeStatus() == timeNotSet){
    parseSerial();

    // 時刻合わせがなされていなく、ある一定期間 (serial_check_count)シリアル入力がなければ、再度NTPクエリをする。
    // 成功したら、設定モードから抜け出して、動作モードになる
    if(timeStatus() == timeNotSet && serial_check_count > 10000000){

      char tmpbuf[30];
      strcpy_P(tmpbuf,pgstr_loop_msg1);
      Serial.println(tmpbuf);
      led_rgb_set(LED_ON, LED_ON, LED_ON);  // LED white
      now();
      led_rgb_set(0,0,0);  // LED white
      if(timeStatus() == timeNotSet){
        strcpy_P(tmpbuf,pgstr_fail);
        Serial.println(tmpbuf);
      }else{
        strcpy_P(tmpbuf,pgstr_success);
        Serial.println(tmpbuf);
        config_mode=0;
      }
    }

  }else{
    parseSerial();

    // 各データに現在時刻をセット
    t = now();

    // if time update detected
    if(lastuploaded_time/MEASURE_INTERVAL < t/MEASURE_INTERVAL){
      lastuploaded_time=t;

      for(i = 0; i < 4; i++) {
         element[i].year = year(t);
         element[i].month = month(t);
         element[i].day = day(t);
         element[i].hour = hour(t);
         element[i].minute = minute(t);
         element[i].second = second(t);
      }

      // データ測定＆文字列として格納
      measure_data();

      // FIAPでアップロードを実行
      led_rgb_set(0, 0, LED_ON);  // LED blue
      int ret = FIAP.post(element, 4);

      switch (ret) {  // FIAPの結果に応じてLEDの色を点灯
      case FIAP_UPLOAD_OK:
        led_rgb_set(0, LED_ON, 0);  // LED green
        break;
      case FIAP_UPLOAD_CONNFAIL:
        led_rgb_set(LED_ON, 0, LED_ON);  // LED purple
        break;
      case FIAP_UPLOAD_HTTPERR:
        led_rgb_set(LED_ON, LED_ON, 0);  // LED yellow
        break;
      case FIAP_UPLOAD_FIAPERR:
        led_rgb_set(0, LED_ON, LED_ON);  // LED cyan
        break;
      default:
        led_rgb_set(LED_ON, 0, 0);  // LED red
        break;
      }
    }

  }
}

// measure data and convert data to string
PROGMEM prog_char pgstr_measure_data_format1[] = "%d.%1d\0";
PROGMEM prog_char pgstr_measure_data_format2[] = "%d\0";
PROGMEM prog_char pgstr_measure_data_ON[] = "ON\0";
PROGMEM prog_char pgstr_measure_data_OFF[] = "OFF\0";
void measure_data()
{
  char tmpbuf[8];

  // 温度
  temperature = (float)analogRead(TEMP_SENS) * (5.0 / 1024.0) * 1000.0 / TEMP_COEF;
  strcpy_P(tmpbuf,pgstr_measure_data_format1);
  sprintf(str_temperature, tmpbuf, (int)temperature, (int)(temperature * 10) % 10);

  // 明るさ
  illuminance = (float)analogRead(LIGHT_SENS) * (5.0 / 1024.0) * 1000.0 / LIGHT_COEF;
  strcpy_P(tmpbuf,pgstr_measure_data_format2);
  sprintf(str_illuminance, tmpbuf, (int)illuminance);

  // DIP SW
  dipsw = (digitalRead(SW_DIP_4) == HIGH) * 8 + (digitalRead(SW_DIP_3) == HIGH) * 4
        + (digitalRead(SW_DIP_2) == HIGH) * 2 + (digitalRead(SW_DIP_1) == HIGH);
  sprintf(str_dipsw, tmpbuf, dipsw);

  // Toggle SW
  tglsw = (digitalRead(SW_TOGGLE) == HIGH);
  if (tglsw) {
    strcpy_P(str_tglsw,pgstr_measure_data_ON);
  } else {
    strcpy_P(str_tglsw,pgstr_measure_data_OFF);
  }
}

// control RGB LED
void led_rgb_set(byte r, byte g, byte b)
{
  analogWrite(LED_R, LED_POLARITY ? (255 - (r)) : (r));
  analogWrite(LED_G, LED_POLARITY ? (255 - (g)) : (g));
  analogWrite(LED_B, LED_POLARITY ? (255 - (b)) : (b));
}

// IPアドレス(IPv4)をドットで区切られた文字列に変換する
PROGMEM prog_char pgstr_ip_to_str_format[] = "%d.%d.%d.%d\0";
char *ip_to_str(const byte *ip)
{
  static char str[16];
  char tmpbuf[15];
  strcpy_P(tmpbuf,pgstr_ip_to_str_format);
  sprintf(str, tmpbuf, ip[0], ip[1], ip[2], ip[3]);
  return(str);
}

// Time adjustment via network using NTP(SNTP)
// (This routine is based on UdpNtpClient program of Arduino Ethernet library)
const int NTP_MAX_WAITCOUNT = 10;    // wait count to receive response
const int NTP_PACKET_SIZE = 48;

// query NTP server and get current time
unsigned long NTPqueryTime()
{
  byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
  char n, i;

  // clear received packet before
  while (udp.parsePacket()) {
    udp.read(packetBuffer, NTP_PACKET_SIZE);
  }

  // query NTP servers
  for (n = 0; n < NTP_MAX_WAITCOUNT; n++) {
    sendNTPpacket(ntp_server1, packetBuffer);
    sendNTPpacket(ntp_server2, packetBuffer);
    delay(500);
    if (!udp.parsePacket()) {
      continue;
    }
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    udp.read(packetBuffer, NTP_PACKET_SIZE);

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    if (secsSince1900 == 0) {
      continue;
    }
    unsigned long epoch = secsSince1900 - 2208988800UL + timezoneOffset;

    // NTP success, return time to time library
    //Serial.print("NTP sync. time=");
    //Serial.println(epoch);
    return(epoch);
  }

  // NTP failure
  // Serial.println("NTP sync. failed");
  return(0);
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(byte *address, byte *packetBuffer)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  udp.beginPacket(address, 123);

  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();

}

PROGMEM prog_char parseSerial_Error1[] = "Exceeded maxium length ... force new line";
PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
PROGMEM prog_char parseSerial_Error4[] = "Already in configuration mode.\0";
PROGMEM prog_char parseSerial_Error5[] = "Cannot exit from the configuration mode.\0";
PROGMEM prog_char parseSerial_Error6[] = "Already in running mode.\0";
PROGMEM prog_char parseSerial_Error7[] = "Syntax Error: ";
PROGMEM prog_char parseSerial_Error8[] = "Readonly parameter.";
PROGMEM prog_char parseSerial_Error9[] = "Too Long: ";

PROGMEM prog_char parseSerial_Indent[] = "   " ;
PROGMEM prog_char parseSerial_Is_OK[] = " ... OK" ;
PROGMEM prog_char parseSerial_MAC_Is_Format[] = "MAC=%02x:%02x:%02x:%02x:%02x:%02x";
PROGMEM prog_char parseSerial_IP_Is_Format[] = "IP=%d.%d.%d.%d";
PROGMEM prog_char parseSerial_NM_Is_Format[] = "NM=%d.%d.%d.%d";
PROGMEM prog_char parseSerial_GW_Is_Format[] = "GW=%d.%d.%d.%d";
PROGMEM prog_char parseSerial_DNS_Is_Format[] = "DNS=%d.%d.%d.%d";
PROGMEM prog_char parseSerial_NTP1_Is_Format[] = "NTP1=%d.%d.%d.%d";
PROGMEM prog_char parseSerial_NTP2_Is_Format[] = "NTP2=%d.%d.%d.%d";

PROGMEM prog_char parseSerial_show_Message00[] = "--- Local Network Configuration ---";
PROGMEM prog_char parseSerial_show_Message01[] = "--- IEEE1888 Configuration ---";
PROGMEM prog_char parseSerial_show_Message02[] = "--- NTP Server Configuration ---";
PROGMEM prog_char parseSerial_show_Message03[] = "AutogenID(";
PROGMEM prog_char parseSerial_show_Message04[] = "): ";

PROGMEM prog_char parseSerial_help_Message00[] = "IEEE1888 Learning Kit ver.1.0";
PROGMEM prog_char parseSerial_help_Message01[] = "";
PROGMEM prog_char parseSerial_help_Message02[] = "-- Basic Command Set --";
PROGMEM prog_char parseSerial_help_Message03[] = "conf: goes into configuration mode (indicated as '# ')";
PROGMEM prog_char parseSerial_help_Message04[] = "exit: returns to running mode (indicated as '> ')";
PROGMEM prog_char parseSerial_help_Message05[] = "show: prints out the config";
PROGMEM prog_char parseSerial_help_Message06[] = "save: saves the config into the EEPROM";
PROGMEM prog_char parseSerial_help_Message07[] = "help or ?: prints out this message";
PROGMEM prog_char parseSerial_help_Message08[] = "";
PROGMEM prog_char parseSerial_help_Message09[] = "-- Configuration Parameters --";
PROGMEM prog_char parseSerial_help_Message10[] = "* Local Network";
PROGMEM prog_char parseSerial_help_Message11[] = " - MAC=xx:xx:xx:xx:xx:xx (readonly)";
PROGMEM prog_char parseSerial_help_Message12[] = " - DHCP={true, false}";
PROGMEM prog_char parseSerial_help_Message13[] = " - IP=x.x.x.x";
PROGMEM prog_char parseSerial_help_Message14[] = " - NM=x.x.x.x";
PROGMEM prog_char parseSerial_help_Message15[] = " - GW=x.x.x.x";
PROGMEM prog_char parseSerial_help_Message16[] = " - DNS=x.x.x.x";
PROGMEM prog_char parseSerial_help_Message17[] = "* IEEE1888 (Server & PointSet ID)";
PROGMEM prog_char parseSerial_help_Message18[] = " - HOST=....";
PROGMEM prog_char parseSerial_help_Message19[] = " - PATH=....";
PROGMEM prog_char parseSerial_help_Message20[] = " - PORT=....";
PROGMEM prog_char parseSerial_help_Message21[] = " - ID=http://....";
PROGMEM prog_char parseSerial_help_Message22[] = "* NTP Time Server";
PROGMEM prog_char parseSerial_help_Message23[] = " - NTP1=x.x.x.x";
PROGMEM prog_char parseSerial_help_Message24[] = " - NTP2=x.x.x.x";
PROGMEM prog_char parseSerial_help_Message25[] = "";

PROGMEM const char* parseSerial_help_Message_List[]={
  parseSerial_help_Message00,  parseSerial_help_Message01,  parseSerial_help_Message02,  parseSerial_help_Message03,
  parseSerial_help_Message04,  parseSerial_help_Message05,  parseSerial_help_Message06,  parseSerial_help_Message07,
  parseSerial_help_Message08,  parseSerial_help_Message09,  parseSerial_help_Message10,  parseSerial_help_Message11,
  parseSerial_help_Message12,  parseSerial_help_Message13,  parseSerial_help_Message14,  parseSerial_help_Message15,
  parseSerial_help_Message16,  parseSerial_help_Message17,  parseSerial_help_Message18,  parseSerial_help_Message19,
  parseSerial_help_Message20,  parseSerial_help_Message21,  parseSerial_help_Message22,  parseSerial_help_Message23,
  parseSerial_help_Message24,  parseSerial_help_Message25
};

// parseSerial
void parseSerial(){

  byte is_newline=0;
  char outLine[60];

  serial_check_count++;
  while(Serial.available()){
    serial_check_count=0;
    if(nInLine<134){
      char c=Serial.read();

      if(c==0x7f){
        // Back Space (BS)
        if(nInLine>0){
          Serial.print(c);
          inLine[--nInLine]='\0';
        }
      }else if(0x20<=c && c<=0x7e){
        // 通常文字の処理
        Serial.print(c);
        inLine[nInLine++]=c;
      }else if(c==0x0d){
        is_newline=1;
        inLine[nInLine]=0x00;
        Serial.println();
        break;
      }
    }else{
      Serial.println();
      // PROGMEM prog_char parseSerial_Error1[] = "Exceeded maxium length ... force new line";
      strcpy_P(outLine,parseSerial_Error1);
      Serial.println(outLine);
      nInLine--;
      is_newline=1;
      inLine[nInLine]=0x00;
      break;
    }
  }

  if(is_newline){

     if(inLine[0]=='M' && inLine[1]=='A' && inLine[2]=='C' && inLine[3]=='='){

       if(config_mode){
         // PROGMEM prog_char parseSerial_Error3[] = "Readonly parameter.";
         strcpy_P(outLine,parseSerial_Error8);
         Serial.println(outLine);
       }else{
         // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
         strcpy_P(outLine,parseSerial_Error3);
         Serial.println(outLine);
       }

     // parse DHCP
     }else if(inLine[0]=='D' && inLine[1]=='H' && inLine[2]=='C' && inLine[3]=='P' && inLine[4]=='='){

      if(config_mode){
       if(strcmp("true",&inLine[5])==0){
         dhcp_e=true;
         Serial.print(inLine);
         // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
         strcpy_P(outLine,parseSerial_Is_OK);
         Serial.println(outLine);
       }else if(strcmp("false",&inLine[5])==0){
         dhcp_e=false;
         Serial.print(inLine);
         // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
         strcpy_P(outLine,parseSerial_Is_OK);
         Serial.println(outLine);
       }else{
         // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
         strcpy_P(outLine,parseSerial_Error2);
         Serial.print(outLine);
         Serial.println(inLine);
       }
       byte ip[4];
       if(parseIPExpr(&inLine[3],ip)){
         char tmp[20];
         // PROGMEM prog_char parseSerial_IP_Is_Format[] = "IP=%d.%d.%d.%d";
         strcpy_P(outLine,parseSerial_IP_Is_Format);
         sprintf(tmp,outLine,ip[0],ip[1],ip[2],ip[3]);
         local_ip4[0]=ip[0]; local_ip4[1]=ip[1]; local_ip4[2]=ip[2]; local_ip4[3]=ip[3];
         Serial.print(tmp);
         // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
         strcpy_P(outLine,parseSerial_Is_OK);
         Serial.println(outLine);
       }else{
       }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse IP address
    }else if(inLine[0]=='I' && inLine[1]=='P' && inLine[2]=='='){

      if(config_mode){
       byte ip[4];
       if(parseIPExpr(&inLine[3],ip)){
         char tmp[20];
         // PROGMEM prog_char parseSerial_IP_Is_Format[] = "IP=%d.%d.%d.%d";
         strcpy_P(outLine,parseSerial_IP_Is_Format);
         sprintf(tmp,outLine,ip[0],ip[1],ip[2],ip[3]);
         local_ip4[0]=ip[0]; local_ip4[1]=ip[1]; local_ip4[2]=ip[2]; local_ip4[3]=ip[3];
         Serial.print(tmp);
         // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
         strcpy_P(outLine,parseSerial_Is_OK);
         Serial.println(outLine);
       }else{
         // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
         strcpy_P(outLine,parseSerial_Error2);
         Serial.print(outLine);
         Serial.println(inLine);
       }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse GW address
    }else if(inLine[0]=='G' && inLine[1]=='W' && inLine[2]=='='){
      if(config_mode){
       byte ip[4];
       if(parseIPExpr(&inLine[3],ip)){
         char tmp[20];
         // PROGMEM prog_char parseSerial_GW_Is_Format[] = "GW=%d.%d.%d.%d";
         strcpy_P(outLine,parseSerial_GW_Is_Format);
         sprintf(tmp,outLine,ip[0],ip[1],ip[2],ip[3]);
         local_gateway[0]=ip[0]; local_gateway[1]=ip[1]; local_gateway[2]=ip[2]; local_gateway[3]=ip[3];
         Serial.print(tmp);
         // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
         strcpy_P(outLine,parseSerial_Is_OK);
         Serial.println(outLine);
       }else{
         // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
         strcpy_P(outLine,parseSerial_Error2);
         Serial.print(outLine);
         Serial.println(inLine);
       }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse NM (netmask)
    }else if(inLine[0]=='N' && inLine[1]=='M' && inLine[2]=='='){
      if(config_mode){
       byte ip[4];
       if(parseIPExpr(&inLine[3],ip)){
         char tmp[20];
         // PROGMEM prog_char parseSerial_NM_Is_Format[] = "NM=%d.%d.%d.%d";
         strcpy_P(outLine,parseSerial_NM_Is_Format);
         sprintf(tmp,outLine,ip[0],ip[1],ip[2],ip[3]);
         local_netmask[0]=ip[0]; local_netmask[1]=ip[1]; local_netmask[2]=ip[2]; local_netmask[3]=ip[3];
         Serial.print(tmp);

         // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
         strcpy_P(outLine,parseSerial_Is_OK);
         Serial.println(outLine);
       }else{
         // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
         strcpy_P(outLine,parseSerial_Error2);
         Serial.print(outLine);
         Serial.println(inLine);
       }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse DNS (netmask)
    }else if(inLine[0]=='D' && inLine[1]=='N' && inLine[2]=='S' && inLine[3]=='='){
      if(config_mode){
       byte ip[4];
       if(parseIPExpr(&inLine[4],ip)){
         char tmp[20];
         // PROGMEM prog_char parseSerial_DNS_Is_Format[] = "DNS=%d.%d.%d.%d";
         strcpy_P(outLine,parseSerial_DNS_Is_Format);
         sprintf(tmp,outLine,ip[0],ip[1],ip[2],ip[3]);
         dns_addr[0]=ip[0]; dns_addr[1]=ip[1]; dns_addr[2]=ip[2]; dns_addr[3]=ip[3];
         Serial.print(tmp);

         // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
         strcpy_P(outLine,parseSerial_Is_OK);
         Serial.println(outLine);
       }else{
         // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
         strcpy_P(outLine,parseSerial_Error2);
         Serial.print(outLine);
         Serial.println(inLine);
       }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse HOST (HOST)
    }else if(inLine[0]=='H' && inLine[1]=='O' && inLine[2]=='S' && inLine[3]=='T' && inLine[4]=='='){
      if(config_mode){
        if( ('A'<=inLine[5] && inLine[5]<='Z') ||
            ('a'<=inLine[5] && inLine[5]<='z') ||
            inLine[5]=='_' ){

          if(strlen(&inLine[5])<MAX_SERVER_HOST_LEN){
            strcpy(server_host,&inLine[5]);
            Serial.print("HOST="); Serial.print(server_host);

            // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
            strcpy_P(outLine,parseSerial_Is_OK);
            Serial.println(outLine);
          }else{
           // PROGMEM prog_char parseSerial_Error9[] = "Too Long: ";
            strcpy_P(outLine,parseSerial_Error9);
            Serial.print(outLine);
            Serial.println(inLine);
          }

        }else if('0'<=inLine[5] && inLine[5]<='9'){
          byte ip[4];
          if(parseIPExpr(&inLine[5],ip)){
            strcpy(server_host,&inLine[5]);
            Serial.print("HOST="); Serial.print(server_host);

            // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
            strcpy_P(outLine,parseSerial_Is_OK);
             Serial.println(outLine);
          }else{
            // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
            strcpy_P(outLine,parseSerial_Error2);
            Serial.print(outLine);
            Serial.println(inLine);
          }

        }else{
          // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
          strcpy_P(outLine,parseSerial_Error2);
          Serial.print(outLine);
          Serial.println(inLine);
        }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse PATH (PATH)
    }else if(inLine[0]=='P' && inLine[1]=='A' && inLine[2]=='T' && inLine[3]=='H' && inLine[4]=='='){
      if(config_mode){
        if(0x21<=inLine[5] && inLine[5]<=0x7e){
           if(strlen(&inLine[5])<MAX_SERVER_PATH_LEN){
             strcpy(server_path,&inLine[5]);
             Serial.print("PATH="); Serial.print(server_path);

             // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
             strcpy_P(outLine,parseSerial_Is_OK);
             Serial.println(outLine);
           }else{
             // PROGMEM prog_char parseSerial_Error9[] = "Too Long: ";
             strcpy_P(outLine,parseSerial_Error9);
             Serial.print(outLine);
             Serial.println(inLine);
           }
        }else{
          // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
          strcpy_P(outLine,parseSerial_Error2);
          Serial.print(outLine);
          Serial.println(inLine);
        }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse PORT (TCP port)
    }else if(inLine[0]=='P' && inLine[1]=='O' && inLine[2]=='R' && inLine[3]=='T' && inLine[4]=='='){
      if(config_mode){
        unsigned long port_num=atol(&inLine[5]);
        if(0<port_num && port_num<=65535){
           server_port=(unsigned int)port_num;
           Serial.print("PORT="); Serial.print(server_port);

           // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
           strcpy_P(outLine,parseSerial_Is_OK);
           Serial.println(outLine);
        }else{
          // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
          strcpy_P(outLine,parseSerial_Error2);
          Serial.print(outLine);
          Serial.println(inLine);
        }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse ID (PointSet ID)
    }else if(inLine[0]=='I' && inLine[1]=='D' && inLine[2]=='='){
      if(config_mode){
        if( inLine[3]=='h' && inLine[4]=='t' &&
            inLine[5]=='t' && inLine[6]=='p' &&
            inLine[7]==':' && inLine[8]=='/' &&
            inLine[9]=='/' ){
            if(strlen(&inLine[3])<MAX_FIAP_ID_PREFIX_LEN){
              strcpy(fiap_id_prefix,&inLine[3]);
              Serial.print("ID="); Serial.print(fiap_id_prefix);

              // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
              strcpy_P(outLine,parseSerial_Is_OK);
              Serial.println(outLine);
            }else{
              // PROGMEM prog_char parseSerial_Error9[] = "Too Long: ";
              strcpy_P(outLine,parseSerial_Error9);
              Serial.print(outLine);
              Serial.println(inLine);
            }
        }else{
          // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
          strcpy_P(outLine,parseSerial_Error2);
          Serial.print(outLine);
          Serial.println(inLine);
        }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse NTP1 (NTP server address)
    }else if(inLine[0]=='N' && inLine[1]=='T' && inLine[2]=='P' && inLine[3]=='1' && inLine[4]=='='){
      if(config_mode){
       byte ip[4];
       if(parseIPExpr(&inLine[5],ip)){
         char tmp[22];
         // PROGMEM prog_char parseSerial_NTP1_Is_Format[] = "NTP1=%d.%d.%d.%d";
         strcpy_P(outLine,parseSerial_NTP1_Is_Format);
         sprintf(tmp,outLine,ip[0],ip[1],ip[2],ip[3]);
         ntp_server1[0]=ip[0]; ntp_server1[1]=ip[1]; ntp_server1[2]=ip[2]; ntp_server1[3]=ip[3];
         Serial.print(tmp);
         // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
         strcpy_P(outLine,parseSerial_Is_OK);
         Serial.println(outLine);
       }else{
         // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
         strcpy_P(outLine,parseSerial_Error2);
         Serial.print(outLine);
         Serial.println(inLine);
       }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse NTP2 (NTP server address)
    }else if(inLine[0]=='N' && inLine[1]=='T' && inLine[2]=='P' && inLine[3]=='2' && inLine[4]=='='){
      if(config_mode){
       byte ip[4];
       if(parseIPExpr(&inLine[5],ip)){
         char tmp[22];
         // PROGMEM prog_char parseSerial_NTP2_Is_Format[] = "NTP2=%d.%d.%d.%d";
         strcpy_P(outLine,parseSerial_NTP2_Is_Format);
         sprintf(tmp,outLine,ip[0],ip[1],ip[2],ip[3]);
         ntp_server2[0]=ip[0]; ntp_server2[1]=ip[1]; ntp_server2[2]=ip[2]; ntp_server2[3]=ip[3];
         Serial.print(tmp);

         // PROGMEM prog_char parseSerial_Is_OK[] = " ... OK";
         strcpy_P(outLine,parseSerial_Is_OK);
         Serial.println(outLine);
       }else{
         // PROGMEM prog_char parseSerial_Error2[] = "Schema Error: ";
         strcpy_P(outLine,parseSerial_Error2);
         Serial.print(outLine);
         Serial.println(inLine);
       }
      }else{
        // PROGMEM prog_char parseSerial_Error3[] = "First, type 'conf' to get into configuration mode.";
        strcpy_P(outLine,parseSerial_Error3);
        Serial.println(outLine);
      }

    // parse conf
    }else if(  inLine[0]=='c'
            && inLine[1]=='o'
            && inLine[2]=='n'
            && inLine[3]=='f'
            && nInLine==4){

      if(config_mode){
        // PROGMEM prog_char parseSerial_Error4[] = "Already in configuration mode.";
        strcpy_P(outLine,parseSerial_Error4);
        Serial.println(outLine);
      }else{
        config_mode=1;
      }

    // parse show
    }else if(  inLine[0]=='s'
            && inLine[1]=='h'
            && inLine[2]=='o'
            && inLine[3]=='w'
            && nInLine==4){

      char tmp[60];

      /* Local Network Configuration */
      // PROGMEM prog_char parseSerial_show_Message00[] = "--- Local Network Configuration ---";
      strcpy_P(outLine,parseSerial_show_Message00);
      Serial.println(outLine);

      /* generate MAC */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      // PROGMEM prog_char parseSerial_MAC_Is_Format[] = "MAC=%02x:%02x:%02x:%02x:%02x:%02x" ;
      strcpy_P(outLine,parseSerial_MAC_Is_Format);
      sprintf(tmp,outLine,local_mac[0],local_mac[1],local_mac[2],local_mac[3],local_mac[4],local_mac[5]);
      Serial.println(tmp);

      /* generate DHCP_E */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      Serial.print("DHCP=");
      if(dhcp_e){
        Serial.println("true");
      }else{
        Serial.println("false");
      }

      /* generate IP */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      // PROGMEM prog_char parseSerial_IP_Is_Format[] = "IP=%d.%d.%d.%d";
      strcpy_P(outLine,parseSerial_IP_Is_Format);
      sprintf(tmp,outLine,local_ip4[0],local_ip4[1],local_ip4[2],local_ip4[3]);
      Serial.println(tmp);

      /* generate NM */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      // PROGMEM prog_char parseSerial_NM_Is_Format[] = "NM=%d.%d.%d.%d";
      strcpy_P(outLine,parseSerial_NM_Is_Format);
      sprintf(tmp,outLine,local_netmask[0],local_netmask[1],local_netmask[2],local_netmask[3]);
      Serial.println(tmp);

      /* generate GW */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      // PROGMEM prog_char parseSerial_GW_Is_Format[] = "GW=%d.%d.%d.%d";
      strcpy_P(outLine,parseSerial_GW_Is_Format);
      sprintf(tmp,outLine,local_gateway[0],local_gateway[1],local_gateway[2],local_gateway[3]);
      Serial.println(tmp);

      /* generate DNS */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      // PROGMEM prog_char parseSerial_DNS_Is_Format[] = "DNS=%d.%d.%d.%d";
      strcpy_P(outLine,parseSerial_DNS_Is_Format);
      sprintf(tmp,outLine,dns_addr[0],dns_addr[1],dns_addr[2],dns_addr[3]);
      Serial.println(tmp);
      Serial.println();

      /* IEEE1888 Configuration */
      // PROGMEM prog_char parseSerial_show_Message01[] = "--- IEEE1888 Configuration ---";
      strcpy_P(outLine,parseSerial_show_Message01);
      Serial.println(outLine);

      /* generate HOST */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      Serial.print("HOST=");
      Serial.println(server_host);

      /* generate PATH */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      Serial.print("PATH=");
      Serial.println(server_path);

      /* generate PORT */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      sprintf(tmp,"PORT=%ld",(unsigned long)server_port);
      Serial.println(tmp);

      /* generate PointSet ID */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      Serial.print("ID=");
      Serial.println(fiap_id_prefix);

      /* AutogenID(...): */
      for(int k=0;k<4;k++){
        // PROGMEM prog_char parseSerial_Indent[] = "   " ;
        strcpy_P(outLine,parseSerial_Indent);
        Serial.print(outLine);
        // PROGMEM prog_char parseSerial_show_Message03[];
        strcpy_P(outLine,parseSerial_show_Message03);
        Serial.print(outLine);
        Serial.print(element[k].cid);
        // PROGMEM prog_char parseSerial_show_Message04[];
        strcpy_P(outLine,parseSerial_show_Message04);
        Serial.print(outLine);
        Serial.print(fiap_id_prefix);
        Serial.println(element[k].cid);
      }
      Serial.println();

      /* NTP Server Configuration */
      // PROGMEM prog_char parseSerial_show_Message02[] = "--- NTP Server Configuration ---";
      strcpy_P(outLine,parseSerial_show_Message02);
      Serial.println(outLine);

      /* generate NTP1 */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      // PROGMEM prog_char parseSerial_NTP1_Is_Format[] = "NTP1=%d.%d.%d.%d";
      strcpy_P(outLine,parseSerial_NTP1_Is_Format);
      sprintf(tmp,outLine,ntp_server1[0],ntp_server1[1],ntp_server1[2],ntp_server1[3]);
      Serial.println(tmp);

      /* generate NTP2 */
      // PROGMEM prog_char parseSerial_Indent[] = "   " ;
      strcpy_P(outLine,parseSerial_Indent);
      Serial.print(outLine);
      // PROGMEM prog_char parseSerial_NTP2_Is_Format[] = "NTP2=%d.%d.%d.%d";
      strcpy_P(outLine,parseSerial_NTP2_Is_Format);
      sprintf(tmp,outLine,ntp_server2[0],ntp_server2[1],ntp_server2[2],ntp_server2[3]);
      Serial.println(tmp);
      Serial.println();

    // parse exit
    }else if(  inLine[0]=='e'
            && inLine[1]=='x'
            && inLine[2]=='i'
            && inLine[3]=='t'
            && nInLine==4){

      if(config_mode){
        if(timeStatus() == timeNotSet ){  // 時刻合わせが終了していない場合には、設定モードから抜け出せない
          // PROGMEM prog_char parseSerial_Error5[] = "Cannot exit from the configuration mode.";
          strcpy_P(outLine,parseSerial_Error5);
          Serial.println(outLine);
        }else{
          config_mode=0;
        }
      }else{
        // PROGMEM prog_char parseSerial_Error6[] = "Already in running mode.";
        strcpy_P(outLine,parseSerial_Error6);
        Serial.println(outLine);
      }

    // parse save
    }else if(  inLine[0]=='s'
      && inLine[1]=='a'
      && inLine[2]=='v'
      && inLine[3]=='e'
      && nInLine==4){

      saveConfig();
      Serial.println("Save ... OK");

    // parse help or ?
    }else if( (  inLine[0]=='h'
         && inLine[1]=='e'
         && inLine[2]=='l'
         && inLine[3]=='p'
         && nInLine==4     )
         ||
         (  inLine[0]=='?'
         && nInLine==1      ) ){

      int k;
      for(k=0;k<26;k++){
        strcpy_P(outLine, (char*)pgm_read_word(&(parseSerial_help_Message_List[k])));
        Serial.println(outLine);
      }

    // Only enter (do nothing)
    }else if(nInLine==0){

    // Unknown Command
    }else{
      // PROGMEM prog_char parseSerial_Error7[] = "Syntax Error: ";
      strcpy_P(outLine,parseSerial_Error7);
      Serial.print(outLine);
      Serial.println(inLine);
    }

    // Finished parsing the line and start reading of a new line.
    nInLine=0;
  }

  if(is_newline){
    if(config_mode){
       Serial.print("# ");
    }else{
       Serial.print("> ");
    }
  }
}


// parseMACExpr
//   return 0 if failed
//   return 1 if success
//
byte parseMACExpr(char* str_mac, byte* mac){
  int i;
  for(i=0;i<6;i++){
    byte front=*(str_mac+i*3);
    byte back=*(str_mac+i*3+1);
    byte separator=*(str_mac+i*3+2);
    if(!((i<5 && (separator=='-' || separator==':')) || (i==5 && separator==0))){
       return 0;
    }

    if('A'<=front && front<='F'){
       front=(front-'A')+10;
    }else if('a'<=front && front<='f'){
       front=(front-'a')+10;
    }else if('0'<=front && front<='9'){
       front=(front-'0');
    }else{
       return 0;
    }
    if('A'<=back && back<='F'){
       back=(back-'A')+10;
    }else if('a'<=back && back<='f'){
       back=(back-'a')+10;
    }else if('0'<=back && back<='9'){
       back=(back-'0');
    }else{
       return 0;
    }
    *(mac+i)=(front<<4)+back;
  }
  return 1;
}

// parseIPExpr
//   return 0 if failed
//   return 1 if success
//
byte parseIPExpr(char* str_addr, byte* addr){
  int i,s,e;
  s=0; e=0;

  char c;
  char small_buf[4];

  for(i=0;i<4;i++){
    for(e=s;e<16;e++){
      c=*(str_addr+e);
      if('0'<=c && c<='9'){
        if(e>=s+4){
          return 0;
        }
        small_buf[e-s]=c;
      }else if((s<e && c=='.' && i<3) || ((c==0x00 || c==0x0d) && i==3)){
        small_buf[e-s]=0;
        break;
      }else{
         return 0;
      }
    }
    s=e+1;
    int intValue=atoi(small_buf);
    if(intValue!=(byte)intValue){
      return 0;
    }
    *(addr+i)=(byte)intValue;
    if(e>=16 || (i!=3 && (c==0x00 || c==0x0d))){
      return 0;
    }
  }
  return 1;
}

// 設定のEEPROMへの保存
void saveConfig(){
   int i;
   EEPROM.write(0,0xaa);
   EEPROM.write(1,0x55);
   EEPROM.write(2,FIRMWARE_VERSION);
   for(i=0;i<6;i++){
      EEPROM.write(i+3,local_mac[i]);
   }
   if(dhcp_e){
     EEPROM.write(9,0x0ff);
   }else{
     EEPROM.write(9,0);
   }
   for(i=0;i<4;i++){
      EEPROM.write(i+10,local_ip4[i]);
   }
   for(i=0;i<4;i++){
      EEPROM.write(i+14,local_gateway[i]);
   }
   for(i=0;i<4;i++){
     EEPROM.write(i+18,local_netmask[i]);
   }
   for(i=0;i<4;i++){
      EEPROM.write(i+22,ntp_server1[i]);
   }
   for(i=0;i<4;i++){
      EEPROM.write(i+26,ntp_server2[i]);
   }
   for(i=0;i<4;i++){
      EEPROM.write(i+30,dns_addr[i]);
   }

   for(i=0;i<MAX_SERVER_HOST_LEN;i++){
      EEPROM.write(i+34,server_host[i]);
   }
   for(i=0;i<MAX_SERVER_PATH_LEN;i++){
      EEPROM.write(i+(34+MAX_SERVER_HOST_LEN),server_path[i]);
   }
   EEPROM.write(34+MAX_SERVER_HOST_LEN+MAX_SERVER_PATH_LEN,server_port&0xff);
   EEPROM.write(35+MAX_SERVER_HOST_LEN+MAX_SERVER_PATH_LEN,server_port/0x100);
   for(i=0;i<MAX_FIAP_ID_PREFIX_LEN;i++){
      EEPROM.write(i+(36+MAX_SERVER_HOST_LEN+MAX_SERVER_PATH_LEN),fiap_id_prefix[i]);
   }
}

// 設定のEEPROMからの読出し
void loadConfig(){
   int i;
   if(EEPROM.read(0)!=0xaa || EEPROM.read(1)!=0x55 || EEPROM.read(2) != FIRMWARE_VERSION) {
      return ;
   }

   for(i=0;i<6;i++){
      local_mac[i]=EEPROM.read(i+3);
   }
   if(EEPROM.read(9)!=0){
     dhcp_e=true;
   }else{
     dhcp_e=false;
   }
   for(i=0;i<4;i++){
      local_ip4[i]=EEPROM.read(i+10);
   }
   for(i=0;i<4;i++){
      local_gateway[i]=EEPROM.read(i+14);
   }
   for(i=0;i<4;i++){
      local_netmask[i]=EEPROM.read(i+18);
   }
   for(i=0;i<4;i++){
      ntp_server1[i]=EEPROM.read(i+22);
   }
   for(i=0;i<4;i++){
      ntp_server2[i]=EEPROM.read(i+26);
   }
   for(i=0;i<4;i++){
      dns_addr[i]=EEPROM.read(i+30);
   }
   for(i=0;i<MAX_SERVER_HOST_LEN;i++){
      server_host[i]=EEPROM.read(i+34);
   }
   for(i=0;i<MAX_SERVER_PATH_LEN;i++){
      server_path[i]=EEPROM.read(i+(34+MAX_SERVER_HOST_LEN));
   }
   server_port=EEPROM.read(34+MAX_SERVER_HOST_LEN+MAX_SERVER_PATH_LEN)
              +(EEPROM.read(35+MAX_SERVER_HOST_LEN+MAX_SERVER_PATH_LEN)*0x100);

   for(i=0;i<MAX_FIAP_ID_PREFIX_LEN;i++){
      fiap_id_prefix[i]=EEPROM.read(i+(36+MAX_SERVER_HOST_LEN+MAX_SERVER_PATH_LEN));
   }
}

// end of code
