// 1-channel LoRa Gateway for ESP8266 and ESP32     20230408
// Copyright (c) 2016-2020 free
//
// Based on work done by Thomas Telkamp for Raspberry PI 1-ch gateway and many others.
//
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the MIT License
// which accompanies this distribution, and is available at
// https://opensource.org/licenses/mit-license.php
//
// NO WARRANTY OF ANY KIND IS PROVIDED
//
// The protocols and specifications used for this 1ch gateway:
// 1. LoRA Specification version V1.0 and V1.1 for Gateway-Node communication
//
// 2. Semtech Basic communication protocol between Lora gateway and server version 3.0.0
//	https://github.com/Lora-net/packet_forwarder/blob/master/PROTOCOL.TXT
//
// Notes:
// - Once call hostbyname() to get IP for services, after that only use IP
//	 addresses (too many gethost name makes the ESP unstable)
// - Only call yield() in main stream (not for background NTP sync).
//
// ----------------------------------------------------------------------------------------
#define ESP32_ARCH 1  //定义ESP32 SPI

#if defined (ARDUINO_ARCH_ESP32) || defined(ESP32)
#	define ESP32_ARCH 1
#	ifndef _PIN_OUT
#		define _PIN_OUT 4									// For ESP32 pin-out 4 is default
#	endif
#elif defined(ARDUINO_ARCH_ESP8266)
//
#else
#	error "Architecture unknown and not supported"
#endif


// The followion file contains most of the definitions
// used in other files. It should be the first file.
#include "configGway.h"										// contains the configuration data of GWay
#include "configNode.h"										// Contains the personal data of Wifi etc.

#include <Esp.h>											// ESP8266 specific IDE functions
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
//#include <time.h>
#include <sys/time.h>
#include <cstring>
#include <string>											// C++ specific string functions

#include <SPI.h>											// For the RFM95 bus
#include <TimeLib.h>										// http://playground.arduino.cc/code/time
#include <ArduinoJson.h>
#include <FS.h>												// ESP8266 Specific
#include <WiFiUdp.h>
#include <pins_arduino.h>
#include <gBase64.h>										// https://github.com/adamvr/arduino-base64 (changed the name)

// Local include files
#include "loraModem.h"
#include "loraFiles.h"
#include "oLED.h"

extern "C" {
#	include "lwip/err.h"
#	include "lwip/dns.h"
}

#if (_GATEWAYNODE==1) || (_LOCALSERVER==1)
#	include "AES-128_V10.h"
#endif

// ----------- Specific ESP32 stuff --------------
#if defined(ESP32_ARCH)
#	include <WiFi.h>
#	include <ESPmDNS.h>
#	include <SPIFFS.h>
#	include <WiFiManager.h>								// Standard lib for ESP WiFi config through an AP

#	define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

//#	if _SERVER==1
//#		include <WebServer.h>								// Standard Webserver for ESP32
//#		include <Streaming.h>          						// http://arduiniana.org/libraries/streaming/
//WebServer server(_SERVERPORT);
//#	endif //_SERVER


#include <WiFi.h>
//#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>


// ----------- Specific ESP8266 stuff --------------
#elif defined(ARDUINO_ARCH_ESP8266)
extern "C" {
#		include "user_interface.h"
#		include "c_types.h"
}
#	include <ESP8266WiFi.h>									// Which is specific for ESP8266
#	include <ESP8266mDNS.h>
#	include <WiFiManager.h>									// Library for ESP WiFi config through an AP

#	define ESP_getChipId()   (ESP.getChipId())

#	if _SERVER==1
#		include <ESP8266WebServer.h>
#		include <Streaming.h>          						// http://arduiniana.org/libraries/streaming/
ESP8266WebServer server(_SERVERPORT);				// Standard IDE lib
#	endif //_SERVER

#	if _OTA==1
#		include <ESP8266httpUpdate.h>
#		include <ArduinoOTA.h>
#	endif //_OTA


#else
#	error "Architecture not supported"

#endif //ESP_ARCH

#include <DNSServer.h>										// Local DNSserver


// ----------- Declaration of variables --------------

bool led_show = false;
bool show_flag = true;
String Rx_last_time = "";
String Tx_last_time = "";
int Rx_cont = 0;
int Tx_cont = 0;
String sys_time = "";
uint8_t debug = 1;											// Debug level! 0 is no msgs, 1 normal, 2 extensive
uint8_t pdebug = P_MAIN ;									// Initially only MAIN and GUI

#if _GATEWAYNODE==1
#	if _GPS==1
#		include <TinyGPS++.h>
TinyGPSPlus gps;
HardwareSerial sGps(1);
#	endif //_GPS
#endif //_GATEWAYNODE

using namespace std;
byte 		currentMode = 0x81;
bool		sx1272 = true;									// Actually we use sx1276/RFM95

#define GATEWAY_REST_PIN 0
#define GATEWAY_RXTX_PIN 5
// ----------------------------------------------------------------------------
//
// Configure these values only if necessary!
//
// ----------------------------------------------------------------------------

// Set spreading factor (SF7 - SF12)
sf_t sf;							// Initial value of SF

// Set location, description and other configuration parameters
// Defined in ESP-sc_gway.h
//
float lat			= _LAT;									// Configuration specific info...
float lon			= _LON;
int   alt			= _ALT;
char platform[24]	= _PLATFORM; 							// platform definition
char email[40]		= _EMAIL;    							// used for contact email
char description[64] = _DESCRIPTION;							// used for free form description

// JSON definitions
StaticJsonDocument<312> jsonBuffer;							// Use of arduinoJson version 6!

// define servers

IPAddress ntpServer;										  // IP address of NTP_TIMESERVER
IPAddress ttnServer;										  // IP Address of thethingsnetwork server
IPAddress thingServer;										// Only if we use a second (backup) server

#include <ETH.h> //引用以使用ETH
#define ETH_PHY_ADDR    0
#define ETH_PHY_POWER  -1
#define ETH_PHY_MDC    23
#define ETH_PHY_MDIO   18
#define ETH_PHY_TYPE   ETH_PHY_LAN8720
#define ETH_CLK_MODE   ETH_CLOCK_GPIO17_OUT

WiFiUDP Udp;

time_t startTime = 0;										  // The time in seconds since 1970 that the server started.
uint32_t eventTime = 0;										// Timing of _event to change value (or not).
uint32_t sendTime = 0;										// Time that the last message transmitted
uint32_t doneTime = 0;										// Time to expire when CDDONE takes too long
uint32_t statTime = 0;										// last time we sent a stat message to server
uint32_t pullTime = 0;										// last time we sent a pull_data request to server
uint32_t rstTime  = 0;										// When to reset the timer
uint32_t fileTime = 0;										// Write the configuration to file

#define TX_BUFF_SIZE  1024								// Upstream buffer to send to MQTT
#define RX_BUFF_SIZE  1024								// Downstream received from MQTT
#define STATUS_SIZE	  512									// Should(!) be enough based on the static text .. was 1024

#if _SERVER==1
uint32_t wwwtime = 0;
#endif
#if NTP_INTR==0
uint32_t ntptimer = 0;
#endif
#if _GATEWAYNODE==1
uint16_t frameCount = 0;									// We write this to SPIFF file
#endif
#ifdef _PROFILER
uint32_t endTmst = 0;
#endif

// Init the indexes of the data we display on the webpage
// We use this for circular buffers
uint16_t iMoni = 0;
uint16_t iSeen = 0;
uint16_t iSens = 0;

// volatile bool inSPI This initial value of mutex is to be free,
// which means that its value is 1 (!)
//
int16_t mutexSPI = 1;

// ----------------------------------------------------------------------------
// FORWARD DECLARATIONS
// These forward declarations are done since other .ino fils are linked by the
// compiler/linker AFTER the main ESP-sc-gway.ino file.
// And espcecially when calling functions with ICACHE_RAM_ATTR the complier
// does not want this.
// Solution can also be to specify less STRICT compile options in Makefile
// ----------------------------------------------------------------------------

void ICACHE_RAM_ATTR Interrupt_0();
void ICACHE_RAM_ATTR Interrupt_1();

int sendPacket(uint8_t *buf, uint8_t length);							// _txRx.ino forward

void printIP(IPAddress ipa, const char sep, String & response);			// _wwwServer.ino
void setupWWW();														// _wwwServer.ino forward

void mPrint(String txt);												// _utils.ino
int getNtpTime(time_t *t);												// _utils.ino
int mStat(uint8_t intr, String & response);								// _utils.ino
void SerialStat(uint8_t intr);											// _utils.ino
void printHexDigit(uint8_t digit, String & response);					// _utils.ino
int inDecodes(char * id);												// _utils.ino
static void stringTime(time_t t, String & response);					// _utils.ino

int initMonitor(struct moniLine *monitor);								// _loraFiles.ino
void initConfig(struct espGwayConfig *c);								// _loraFiles.ino
int writeSeen(const char *fn, struct nodeSeen *listSeen);				// _loraFiles.ino
int readGwayCfg(const char *fn, struct espGwayConfig *c);				// _loraFiles.ino

void setupOta(char *hostname);											// _otaServer.ino

void initLoraModem();													// _loraModem.ino
void rxLoraModem();														// _loraModem.ino
void writeRegister(uint8_t addr, uint8_t value);						// _loraModem.ino
void cadScanner();														// _loraModem.ino
void startReceiver();													// _loraModem.ino

void stateMachine();													// _stateMachine.ino

bool connectUdp();														// _udpSemtech.ino
int readUdp(int packetSize);											// _udpSemtech.ino
int sendUdp(IPAddress server, int port, uint8_t *msg, uint16_t length);		// _udpSemtech.ino
void sendstat();														// _udpSemtech.ino
void pullData();														// _udpSemtech.ino

#if _MUTEX==1
void ICACHE_FLASH_ATTR CreateMutux(int *mutex);
bool ICACHE_FLASH_ATTR GetMutex(int *mutex);
void ICACHE_FLASH_ATTR ReleaseMutex(int *mutex);
#endif


void init_ST7735();
void ST7735_Clear();
void ST7735_SHOW_TEST(int x, int y, int w, int h, int text_x, int text_y, String str, int str_font, int str_size);   //显示字符
void ROLL_Text(int x, int y, int w, int h, int text_x, int text_y, String str, int str_font, int str_size);
bool ST7735_GET_TOUCH();

Preferences prefs;
const char* AP_NAME = "ELECROW-GW1C";//Web配网模式下的AP-wifi名字
const char* AP_PASSWORD = "aaaabbbb";//Web配网模式下的AP-wifi密码

//暂时存储Gateway配置信息
char sta_gatewayMode[32] = {0};
char sta_ssid[64] = {0};
char sta_password[64] = {0};
char sta_gatewayid[64] = {0};
char sta_serverAddr[64] = {0};
char sta_serverPort[64] = {0};
char sta_region[32] = {0};
char sta_getway_channel[32] = {0};
char sta_getway_sf[32] = {0};
char sta_getway_utc[32] = {0};

String gatewayMode = "", wifiid = "", wifipass = "", gatewayid = "", serverAddr = "", serverPort = "", region = "", getway_channel = "", getway_sf = "", getway_utc = "";
//配网页面代码
String page_html = R"(
<!DOCTYPE html>
<html lang='en'>
<head>
    <meta charset='UTF-8'>
    <meta http-equiv='X-UA-Compatible' content='IE=edge'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>Document</title>
    <style>
        *{
            margin: 0;
            padding: 0;
        }
        body{
            background-color:#DCDCDC;
        }
        .esp_warp{
            width: 500px;
            margin: auto;
            background-color: #fff;
            padding: 50px 0;
            text-align: center;
            font-family: Arial;
        }
        .esp_warp h4{
            font-size: 24px;
            font-weight: bold;
            line-height: 30px;
            margin-bottom: 30px;
        }
        .espConfig{
            width: 65%;
            margin: 0 auto;
        }
        .espConfig input,.espConfig select{
            width: 100%;
            font-size: 16px;
            line-height: 24px;
            border: 1px solid #000;
            padding: 5px 0;
        }
        .espConfig input{
            border-radius: 10px;
            text-align: center;
            outline: none;
        }
        .espConfig select{
            text-indent: 0.5em;
            border-radius: 10px;
        }
        .espConfig option{
            font-size: 14px;
        }
        .espConfig_com{
            margin: 15px 0;
        }
        .espConfig_com label{
            font-size: 16px;
            line-height: 24px;
        }
        .gateway_default{
            font-size: 16px;
            line-height: 24px;
            padding: 8px 20px;
            color: #fff;
            background-color: #000;
            border-radius: 15px;
        }
        .espConfig_button{
            width: 100%;
            font-size: 16px;
            line-height: 24px;
            padding: 8px 0;
            color: #fff;
            background-color: #000;
            border-radius: 15px;
            margin-top: 35px;
        }
        .espConfig_button:hover{
                background-color: #242222;
            cursor:pointer;
        }
        @media (max-width: 759px){
            .esp_warp{
                width: 100%;
            }
        }
    </style>
</head>
<body>
    <div class='esp_warp'>
        <h4>ESP32-Gateway Config</h4>
        <form action='/' method='post' class='espConfig' id='espConfig'>
            <div class='espConfig_com'>
                <label for='netMode'>NET MODE</label>
                <select name='netMode' id='netMode'>
                    <option value='wifi' selected>WIFI</option>
                    <option value='net'>NET</option>
                </select>
            </div>
            <div class='espConfig_com'>
                <label for='wifiSsid'>WiFi SSID</label>
                <input type='text' value='' placeholder='' id='wifiSsid' name='wifiSsid'>
            </div>
            <div class='espConfig_com'>
                <label for='wifiPass'>WiFi PASS</label>
                <input type='text' value='' placeholder='' id='wifiPass' name='wifiPass'>
            </div>
            <div class='espConfig_com'>
                <label for='wifiPass'>Gateway ID</label>
                <input type='text' value='' placeholder='' id='gatewayid' name='gatewayid'>
            </div>
            <button class='gateway_default' id='idDefault' type='button' onclick='handleDefault()'>GatewayID Default</button>
            <div class='espConfig_com'>
                <label for='serverAddr'>SERVER ADDR</label>
                <input type='text' value='eu1.cloud.thethings.network' placeholder='' id='serverAddr' name='serverAddr'>
            </div>
            <div class='espConfig_com'>
                <label for='serverPort'>SERVER PORT</label>
                <input type='text' value='1700' placeholder='' id='serverPort' name='serverPort'>
            </div>
            <div class='espConfig_com'>
                <label for='region'>REGION</label>
                <select name='region' id='region'>
                    <option value='EU868' selected>EU868</option>
                    <option value='US915'>US915</option>
                </select>
            </div>
            <div class='espConfig_com'>
                <label for='channel'>CHANNEL</label>
                <select name='channel' id='channel'>
                    <option value='0' selected>0</option>
                    <option value='1'>1</option>
                    <option value='2'>2</option>
                    <option value='3'>3</option>
                    <option value='4'>4</option>
                    <option value='5'>5</option>
                    <option value='6'>6</option>
                    <option value='7'>7</option>
                </select>
            </div>
            <div class='espConfig_com'>
                <label for='sf'>SF</label>
                <select name='sf' id='sf'>
                    <option value='7' selected>7</option>
                    <option value='8'>8</option>
                    <option value='9'>9</option>
                    <option value='10'>10</option>
                    <option value='11'>11</option>
                    <option value='12'>12</option>
                </select>
            </div>
            <div class='espConfig_com'>
                <label for='utc'>TIME ZONE</label>
                <select name='utc' id='utc'>
                    <option value='UTC' selected>UTC</option>
                    <option value='UTC+1' selected>UTC+1</option>
                    <option value='UTC+2' selected>UTC+2</option>
                    <option value='UTC+3' selected>UTC+3</option>
                    <option value='UTC+4' selected>UTC+4</option>
                    <option value='UTC+5' selected>UTC+5</option>
                    <option value='UTC+6' selected>UTC+6</option>
                    <option value='UTC+7' selected>UTC+7</option>
                    <option value='UTC+8' selected>UTC+8</option>
                    <option value='UTC+9' selected>UTC+9</option>
                    <option value='UTC+10' selected>UTC+10</option>
                    <option value='UTC+11' selected>UTC+11</option>
                    <option value='UTC+12' selected>UTC+12</option>
                    <option value='UTC-1' selected>UTC-1</option>
                    <option value='UTC-2' selected>UTC-2</option>
                    <option value='UTC-3' selected>UTC-3</option>
                    <option value='UTC-4' selected>UTC-4</option>
                    <option value='UTC-5' selected>UTC-5</option>
                    <option value='UTC-6' selected>UTC-6</option>
                    <option value='UTC-7' selected>UTC-7</option>
                    <option value='UTC-8' selected>UTC-8</option>
                    <option value='UTC-9' selected>UTC-9</option>
                    <option value='UTC-10' selected>UTC-10</option>
                    <option value='UTC-11' selected>UTC-11</option>
                    <option value='UTC-12' selected>UTC-12</option>
                </select>
            </div>
            <button type='submit' class='espConfig_button'>Submit</button>
        </form>
    </div>
</body>
<script type='text/javascript'>
    function handleDefault(){
        var ajax = new XMLHttpRequest();
        ajax.onreadystatechange = function(){
            if(ajax.readyState == 4 && ajax.status == 200){
                document.getElementById('gatewayid').value = ajax.responseText;
            }
        }
        ajax.open('get','/text');
        ajax.send();
    }
</script>
</html>
)";

//const byte DNS_PORT = 53;//DNS端口号
IPAddress apIP(192, 168, 4, 1);//esp32-AP-IP地址
//DNSServer dnsServer;//创建dnsServer实例
WebServer gateway_server(80);//创建WebServer
uint8_t    MAC_array[8];
  char MAC_char[19];                    // XXX Unbelievable
  char hostname[12];                    // hostname space
  String mode_getwag;
  
void handleRoot() 
{
  gateway_server.send(200, "text/html", page_html);
}

void handleAjax()
{
  gateway_server.send(200, "text/html", gatewayid);
}

void handleRootPost()
{
  //Post回调函数
  
  Serial.println("handleRootPost");

  if (gateway_server.hasArg("netMode")) {//判断是否有账号参数
    Serial.print("got netMode:");
    strcpy(sta_gatewayMode, gateway_server.arg("netMode").c_str());//将账号参数拷贝到sta_ssid中
    Serial.println(sta_gatewayMode);
  } 
  
  if (gateway_server.hasArg("wifiSsid")) {//判断是否有账号参数
    Serial.print("got ssid:");
    strcpy(sta_ssid, gateway_server.arg("wifiSsid").c_str());//将账号参数拷贝到sta_ssid中
    Serial.println(sta_ssid);
  } 
  
  if (gateway_server.hasArg("wifiPass")) {
    Serial.print("got password:");
    strcpy(sta_password, gateway_server.arg("wifiPass").c_str());
    Serial.println(sta_password);
  } 

  if (gateway_server.hasArg("gatewayid")) {
    Serial.print("got gatewayid:");
    strcpy(sta_gatewayid, gateway_server.arg("gatewayid").c_str());
    Serial.println(sta_gatewayid);
  } 

  if (gateway_server.hasArg("serverAddr")) {
    Serial.print("got serverAddr:");
    strcpy(sta_serverAddr, gateway_server.arg("serverAddr").c_str());
    Serial.println(sta_serverAddr);
  } 


  if (gateway_server.hasArg("serverPort")) {
    Serial.print("got serverPort:");
    strcpy(sta_serverPort, gateway_server.arg("serverPort").c_str());
    Serial.println(sta_serverPort);
  } 

  if (gateway_server.hasArg("region")) {
    Serial.print("got region:");
    strcpy(sta_region, gateway_server.arg("region").c_str());
    Serial.println(sta_region);
  } 

    if (gateway_server.hasArg("channel")) {
    Serial.print("got channel:");
    strcpy(sta_getway_channel, gateway_server.arg("channel").c_str());
    Serial.println(sta_getway_channel);
  } 

  if (gateway_server.hasArg("sf")) {
    Serial.print("got sf:");
    strcpy(sta_getway_sf, gateway_server.arg("sf").c_str());
    Serial.println(sta_getway_sf);
  } 

  if (gateway_server.hasArg("utc")) {
    Serial.print("got utc:");
    strcpy(sta_getway_utc, gateway_server.arg("utc").c_str());
    Serial.println(sta_getway_utc);
  } 

  gatewayMode = sta_gatewayMode;
  wifiid = sta_ssid;
  wifipass = sta_password;
  gatewayid = sta_gatewayid;
  serverAddr = sta_serverAddr;
  serverPort = sta_serverPort;
  region = sta_region;
  getway_channel = sta_getway_channel;
  getway_sf = sta_getway_sf;
  getway_utc = sta_getway_utc;
  
  prefs.putString( "netMode" ,gatewayMode);
  prefs.putString( "wifiSsid" ,wifiid);
  prefs.putString( "wifiPass", wifipass);
  prefs.putString( "gatewayid", gatewayid);
  prefs.putString( "serverAddr", serverAddr);
  prefs.putString( "serverPort", serverPort);
  prefs.putString( "region", region);
  prefs.putString( "getway_channel", getway_channel);
  prefs.putString( "getway_sf", getway_sf);
  prefs.putString( "getway_utc", getway_utc);
  prefs.end();

  gateway_server.send(200, "text/html", "<meta charset='UTF-8'><h1>Saved successfully. being restarted...</h1>");//返回保存成功页面
  delay(2000);
  ST7735_Clear();
  ESP.restart(); //重启ESP32
}

void initWebServer(void){//初始化WebServer
  //上面那行必须以下面这种格式去写否则无法强制门户
  gateway_server.on("/text", HTTP_GET, handleAjax); //注册网页中ajax发送的get方法的请求和回调函数
  gateway_server.on("/", HTTP_GET, handleRoot);//设置主页回调函数
  gateway_server.onNotFound(handleRoot);//设置无法响应的http请求的回调函数
  gateway_server.on("/", HTTP_POST, handleRootPost);//设置Post请求回调函数
  
  gateway_server.begin();//启动WebServer
  Serial.println("WebServer started!");
}

void initSoftAP(void){//初始化AP模式
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  Serial.println(WiFi.softAPIP());
//  Serial.print("本地IP： ");
//  Serial.println(WiFi.localIP());
  if(WiFi.softAP(AP_NAME, AP_PASSWORD)){
    Serial.println("ELECROW-GW1C SoftAP is right");
  }

    WiFi.macAddress(MAC_array);
    sprintf(MAC_char,"%02x:%02x:%02x:%02x:%02x:%02x",
    MAC_array[0],MAC_array[1],MAC_array[2],MAC_array[3],MAC_array[4],MAC_array[5]);
    Serial.println("MAC: " + String(MAC_char) + ", len=" + String(strlen(MAC_char)) );
    String response= "";
    printHexDigit(MAC_array[0], response);
    printHexDigit(MAC_array[1], response);
    printHexDigit(MAC_array[2], response);
    printHexDigit(0xF4,     response);
    printHexDigit(0xF4,     response);
    printHexDigit(MAC_array[3], response);
    printHexDigit(MAC_array[4], response);
    printHexDigit(MAC_array[5], response);
    gatewayid = response;
}

struct tm {
  int tm_sec;    /* Seconds (0-60) */
  int tm_min;    /* Minutes (0-59) */
  int tm_hour;   /* Hours (0-23) */
  int tm_mday;   /* Day of the month (1-31) */
  int tm_mon;    /* Month (0-11) */
  int tm_year;   /* Year - 1900 */
  int tm_wday;   /* Day of the week (0-6, Sunday = 0) */
  int tm_yday;   /* Day in the year (0-365, 1 Jan = 0) */
  int tm_isdst;  /* Daylight saving time */
};

struct tm timeInfo;

static bool eth_connected = false;
void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}
// ============================================================================
// MAIN PROGRAM CODE (SETUP AND LOOP)



void hexCharacterStringToBytes(byte *byteArray, const char *hexString)
{
    bool oddLength = strlen(hexString) & 1;

    byte currentByte = 0;
    byte byteIndex = 0;

    for (byte charIndex = 0; charIndex < strlen(hexString); charIndex++)
    {
        bool oddCharIndex = charIndex & 1;

        if (oddLength)
        {
            // If the length is odd
            if (oddCharIndex)
            {
                // odd characters go in high nibble
                currentByte = nibble(hexString[charIndex]) << 4;
            }
            else
            {
                // Even characters go into low nibble
                currentByte |= nibble(hexString[charIndex]);
                byteArray[byteIndex++] = currentByte;
                currentByte = 0;
            }
        }
        else
        {
            // If the length is even
            if (!oddCharIndex)
            {
                // Odd characters go into the high nibble
                currentByte = nibble(hexString[charIndex]) << 4;
            }
            else
            {
                // Odd characters go into low nibble
                currentByte |= nibble(hexString[charIndex]);
                byteArray[byteIndex++] = currentByte;
                currentByte = 0;
            }
        }
    }
}

byte nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0; // Not a valid hexadecimal character
}


// ----------------------------------------------------------------------------
// Setup code (one time)
// _state is S_INIT
// ----------------------------------------------------------------------------



void setup() {

  Serial.begin(_BAUDRATE);              // As fast as possible for bus
  delay(50);
  Serial.flush();

  pinMode(GATEWAY_REST_PIN, INPUT); //设置IO0模式
  pinMode(GATEWAY_RXTX_PIN, OUTPUT); //设置IO0模式
  digitalWrite(GATEWAY_RXTX_PIN, HIGH);

  init_ST7735();     
  
  prefs.begin("gatewayConfig");

  if(prefs.isKey("netMode")) 
  {
    mode_getwag =  prefs.getString("netMode");
    wifiid =  prefs.getString("wifiSsid");
    wifipass =  prefs.getString("wifiPass");
    gatewayid =  prefs.getString("gatewayid");
    serverAddr =  prefs.getString("serverAddr");
    serverPort =  prefs.getString("serverPort");
    region =  prefs.getString("region");
    getway_channel = prefs.getString( "getway_channel");
    getway_sf = prefs.getString( "getway_sf");
    getway_utc = prefs.getString( "getway_utc");
    prefs.end();

    Serial.println("netMode ---> " + mode_getwag);
    Serial.println("wifiid ---> " + wifiid);
    Serial.println("wifipass ---> " + wifipass);
    Serial.println("gatewayid ---> " + gatewayid);
    Serial.println("serverAddr ---> " + serverAddr);
    Serial.println("serverPort ---> " + serverPort);
    Serial.println("region ---> " + region);
    Serial.println("getway_channel ---> " + getway_channel);
    Serial.println("getway_sf ---> " + getway_sf);
    Serial.println("getway_utc ---> " + getway_utc);
    #define _TTNSERVER serverAddr   // TTN  Server 
    #define _TTNPORT serverPort.toInt()          // UDP port of gateway! Often 1700 or 1701 is used for upstream comms
    sf = (sf_t)getway_sf.toInt();   
//    region = "EU868";
//     region = "US915";
//    gatewayid = "e05a1bf4f55a8784"; 
      
    if(region == "EU868")
    {
        freqs [0] ={ 868100000, 125, 7, 12, 868100000, 125, 7, 12 };
        freqs [1] ={ 868300000, 125, 7, 12, 868300000, 125, 7, 12 };
        freqs [2] ={ 868500000, 125, 7, 12, 868500000, 125, 7, 12 };
        freqs [3] ={ 867100000, 125, 7, 12, 867100000, 125, 7, 12 };
        freqs [4] ={ 867300000, 125, 7, 12, 867300000, 125, 7, 12 };
        freqs [5] ={ 867500000, 125, 7, 12, 867500000, 125, 7, 12 };
        freqs [6] ={ 867700000, 125, 7, 12, 867700000, 125, 7, 12 };
        freqs [7] ={ 867900000, 125, 7, 12, 867900000, 125, 7, 12 };
    }
    else
    if(region == "US915")
    {
        freqs [0] ={ 902300000, 125, 7, 12, 923300000, 500, 7, 12 };
        freqs [1] ={ 902500000, 125, 7, 12, 923900000, 500, 7, 12 };
        freqs [2] ={ 902700000, 125, 7, 12, 924500000, 500, 7, 12 };
        freqs [3] ={ 902900000, 125, 7, 12, 925100000, 500, 7, 12 };
        freqs [4] ={ 903100000, 125, 7, 12, 925700000, 500, 7, 12 };
        freqs [5] ={ 903300000, 125, 7, 12, 926300000, 500, 7, 12 };
        freqs [6] ={ 903500000, 125, 7, 12, 926900000, 500, 7, 12 };
        freqs [7] ={ 903700000, 125, 7, 12, 927500000, 500, 7, 12 };
    }
  
    initConfig(&gwayConfig);

  if (SPIFFS.begin()) {
#    if _MONITOR>=1
    if ((debug >= 1) && (pdebug & P_MAIN)) {
      mPrint("SPIFFS begin");
    }
#   endif //_MONITOR
  }
  else {                          // SPIFFS not found
    if (pdebug & P_MAIN) {
      mPrint("SPIFFS.begin: not found, formatting");
    }
    SPIFFS.format();
    delay(500);
    initConfig(&gwayConfig);              // After a format reinit variables
  }

  // If we set SPIFFS_FORMAT in
# if _SPIFFS_FORMAT>=1
  SPIFFS.format();                    // Normally disabled. Enable only when SPIFFS corrupt
  delay(500);
  initConfig(&gwayConfig);
  gwayConfig.formatCntr++;
  if ((debug >= 1) && (pdebug & P_MAIN)) {
    mPrint("SPIFFS Format Done");
  }
# endif //_SPIFFS_FORMAT>=1

# if _MONITOR>=1
  initMonitor(monitor);

#   if defined CFG_noassert
  mPrint("No Asserts");
#   else
  mPrint("Do Asserts");
#   endif //CFG_noassert
# endif //_MONITOR

  delay(50);

  // Read the config file for all parameters not set in the setup() or configGway.h file
  // This file should be read just after SPIFFS is initializen and before
  // other configuration parameters are used.
  // It will overwrite any settings given by initConfig
  if (readGwayCfg(_CONFIGFILE, &gwayConfig) > 0) {      // read the Gateway Config
#   if _MONITOR>=1
    if (debug >= 0) {
      mPrint("readGwayCfg:: return OK");
    }
#   endif
  }
  else {
#   if _MONITOR>=1
    if (debug >= 0) {
      mPrint("setup:: readGwayCfg: ERROR readGwayCfg Failed");
    }
#   endif
  };

  uint32_t net_cont = 0;
  if (mode_getwag == "net")
  {

    Serial.println("start net...");
    ST7735_Clear();
    ST7735_SHOW_TEST(0, 75, 128, 20, 20, 0, "NET CONFIG ...", 3, 2);
    WiFi.onEvent(WiFiEvent);
    ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
    net_cont = 0;

    while (!eth_connected)
    {
        int cont_rest = 0;
        // 检测按键是否按下，当按键按下时将返回低电平
        if (digitalRead(GATEWAY_REST_PIN) == LOW) {
          delay(50);
          // 按键按下 计时
          while(digitalRead(GATEWAY_REST_PIN) == LOW)
          {
            delay(100);
            cont_rest++;
            Serial.println("rest cont = " + String(cont_rest));
            if(cont_rest == 30)
            {
              ST7735_Clear();
              prefs.begin("gatewayConfig",false);//为false才能删除键值
              Serial.println(prefs.freeEntries());//查询清除前的剩余空间
              
              prefs.remove("netMode"); 
              prefs.remove("wifiid"); 
              prefs.remove("wifipass"); 
              prefs.remove("gatewayid"); 
              prefs.remove("serverAddr"); 
              prefs.remove("serverPort"); 
              prefs.remove("region"); 
              prefs.remove("getway_channel"); 
              prefs.remove("getway_sf"); 
              prefs.clear();
              
              delay(500);
              Serial.println(prefs.freeEntries());//查询清除后的剩余空间
              prefs.end();
        
              Serial.println("remove config --> ESP32 restart ");
              ESP.restart(); //重启ESP32
            }
          }
          
        } 
      Serial.println("network connect...");
      delay(1000);
      
      if (net_cont > 15)
        break ;
      net_cont++;
    }

    if (net_cont <= 15)
    {
//      ETH.macAddress(MAC_array);

    //              3  4  5  6  7
      //e05a1b f4 f4 5c 64 20
//              5  6  
//      MAC_array[0] = gatewayid.substring(0, 2)
      hexCharacterStringToBytes(MAC_array, gatewayid.c_str());
      uint8_t MAC_array_swp[8];
      MAC_array_swp[0] =  MAC_array[0];
      MAC_array_swp[1] =  MAC_array[1];
      MAC_array_swp[2] =  MAC_array[2];
      MAC_array_swp[3] =  MAC_array[3];
      MAC_array_swp[4] =  MAC_array[4];
      MAC_array_swp[5] =  MAC_array[5];
      MAC_array_swp[6] =  MAC_array[6];
      MAC_array_swp[7] =  MAC_array[7];
      
      MAC_array[3] =  MAC_array_swp[5];
      MAC_array[4] =  MAC_array_swp[6];
      MAC_array[5] =  MAC_array_swp[7];
      MAC_array[6] =  MAC_array_swp[3];
      MAC_array[7] =  MAC_array_swp[4];
      
    }
    else {
      Serial.println("net error... ESP32 restart ");
      ST7735_Clear();
      ST7735_SHOW_TEST(0, 75, 128, 20, 3, 0, "NET CONFIG ERROR", 3, 2);
      delay(3000);
      ST7735_Clear();
      ESP.restart(); //重启ESP32
      return ;
    }

  }
  else
  {
      WiFi.mode(WIFI_STA);                  // WiFi settings for connections
      WiFi.begin(wifiid.c_str(), wifipass.c_str());
      Serial.println("wifi start..." + wifiid);
      ST7735_Clear();
      ST7735_SHOW_TEST(0, 75, 128, 20, 20, 0, "WIFI CONFIG ...", 3, 2);
      int  net_cont = 0;
      while (WiFi.status() != WL_CONNECTED) {
       
       int cont_rest = 0;
        // 检测按键是否按下，当按键按下时将返回低电平
        if (digitalRead(GATEWAY_REST_PIN) == LOW) {
          delay(50);
          // 按键按下 计时
          while(digitalRead(GATEWAY_REST_PIN) == LOW)
          {
            delay(100);
            cont_rest++;
            Serial.println("rest cont = " + String(cont_rest));
            if(cont_rest == 30)
            {
              ST7735_Clear();
              prefs.begin("gatewayConfig",false);//为false才能删除键值
              Serial.println(prefs.freeEntries());//查询清除前的剩余空间
              
              prefs.remove("netMode"); 
              prefs.remove("wifiid"); 
              prefs.remove("wifipass"); 
              prefs.remove("gatewayid"); 
              prefs.remove("serverAddr"); 
              prefs.remove("serverPort"); 
              prefs.remove("region"); 
              prefs.remove("getway_channel"); 
              prefs.remove("getway_sf"); 
              prefs.clear();
              
              delay(500);
              Serial.println(prefs.freeEntries());//查询清除后的剩余空间
              prefs.end();
        
              Serial.println("remove config --> ESP32 restart ");
              ESP.restart(); //重启ESP32
            }
          }
          
        } 
        if (net_cont > 15)
          break ;
        
        net_cont++;
        delay(1000);
        Serial.println("wifi start..." + wifiid);
      }

      if (net_cont > 15)
      {
          Serial.println("wifi error or no ssid... ESP32 restart ");
          ST7735_Clear();
          ST7735_SHOW_TEST(0, 75, 128, 20, 2, 0, "WIFI CONFIG ERROR", 3, 2);
          Serial.println("WIFI CONNECT ERROR" );
          delay(3000);
          ST7735_Clear();
          ESP.restart(); //重启ESP32
          return ;
      }

      Serial.println("wifi connect ok");

//              3  4  5  6  7
      //e05a1b f4 f4 5c 64 20
//              5  6  
//      MAC_array[0] = gatewayid.substring(0, 2)

      hexCharacterStringToBytes(MAC_array, gatewayid.c_str());

      uint8_t MAC_array_swp[8];
      MAC_array_swp[0] =  MAC_array[0];
      MAC_array_swp[1] =  MAC_array[1];
      MAC_array_swp[2] =  MAC_array[2];
      MAC_array_swp[3] =  MAC_array[3];
      MAC_array_swp[4] =  MAC_array[4];
      MAC_array_swp[5] =  MAC_array[5];
      MAC_array_swp[6] =  MAC_array[6];
      MAC_array_swp[7] =  MAC_array[7];
      
      MAC_array[3] =  MAC_array_swp[5];
      MAC_array[4] =  MAC_array_swp[6];
      MAC_array[5] =  MAC_array_swp[7];
      MAC_array[6] =  MAC_array_swp[3];
      MAC_array[7] =  MAC_array_swp[4];
      
  }

      delay(500);
    // If we are here we are connected to WLAN

    #  if defined(_UDPROUTER)
    // So now test the UDP function
    if (!connectUdp()) {
    #     if _MONITOR>=1
      mPrint("Error connectUdp");
    #     endif //_MONITOR
    }
    # elif defined(_TTNROUTER)
    if (!connectTtn()) {
    #     if _MONITOR>=1
      mPrint("Error connectTtn");
    #     endif //_MONITOR
    }
    # else
    #   if _MONITOR>=1
    mPrint(F("Setup:: ERROR, No UDP or TCP Connection"));
    #   endif //_MONITOR  
    # endif //_UDPROUTER
    
    delay(200);
    
    // Pins are defined and set in loraModem.h
    pinMode(pins.ss, OUTPUT);
    pinMode(pins.rst, OUTPUT);
    pinMode(pins.dio0, INPUT);                // This pin is interrupt
    pinMode(pins.dio1, INPUT);                // This pin is interrupt
    //pinMode(pins.dio2, INPUT);                // XXX future expansion
    
    // Init the SPI pins
    #if defined(ESP32_ARCH)
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    #else
    SPI.begin();
    #endif //ESP32_ARCH
    
    delay(500);
    
    // We choose the Gateway ID to be the Ethernet Address of our Gateway card
    // display results of getting hardware address
    //
    # if _MONITOR>=1
    if (debug >= 0) {
    String response = "Gateway ID: " + gatewayid;
      response += ", Listening at SF" + String(sf) + " on ";
      response += String((double)freqs[gwayConfig.ch].upFreq / 1000000) + " MHz.";
      mPrint(response);
    }
    # endif //_MONITOR
    
//    // ---------- TIME -------------------------------------
//    ntpServer = resolveHost(NTP_TIMESERVER, 15);
//    if (ntpServer.toString() == "0:0:0:0")  {         // MMM Experimental
//    #   if _MONITOR>=1
//      mPrint("setup:: NTP Server not found, found=" + ntpServer.toString());
//    #   endif
//      delay(10000);                     // Delay 10 seconds
//      ntpServer = resolveHost(NTP_TIMESERVER, 10);
//    }
//    
//    // Set the NTP Time
//    // As long as the time has not been set we try to set the time.
//    # if NTP_INTR==1
//    setupTime();                      // Set NTP time host and interval
//    
//    # else //NTP_INTR
//    {
//      // If not using the standard libraries, do manual setting
//      // of the time. This method works more reliable than the
//      // interrupt driven method.
//      String response = ".";
//      while (timeStatus() == timeNotSet) {          // time still 1/1/1970 and 0:00 hrs
//    
//        time_t newTime;
//        if (getNtpTime(&newTime) <= 0) {
//    #       if _MONITOR>=1
//          if (debug >= 0) {
//            mPrint("setup:: ERROR Time not set (yet). Time=" + String(newTime) );
//          }
//    #       endif //_MONITOR
//          response += ".";
//          delay(800);
//          continue;
//        }
//        response += ".";
//        delay(1000);
//        setTime(newTime);
//      }
    
//      // When we are here we succeeded in getting the time
//      startTime = now();                    // Time in seconds
//    #   if _MONITOR>=1
//      if (debug >= 0) {
//        String response = "Time set=";
//        stringTime(now(), response);
//        mPrint(response);
//      }
//    #   endif //_MONITOR
//    
//      writeGwayCfg(_CONFIGFILE, &gwayConfig );
//    }
//    # endif //NTP_INTR
    
    delay(100);
    
    
    // ---------- TTN SERVER -------------------------------
    #ifdef _TTNSERVER
    ttnServer = resolveHost(_TTNSERVER, 10);          // Use DNS to get server IP
    if (ttnServer.toString() == "0:0:0:0") {          // Experimental
    #   if _MONITOR>=1
      if (debug >= 1) {
        mPrint("setup:: TTN Server not found");
      }
    #   endif
      delay(10000);                     // Delay 10 seconds
      ttnServer = resolveHost(_TTNSERVER, 10);
    }
    delay(100);
    #endif //_TTNSERVER
    
  
//    #ifdef _THINGSERVER
//    thingServer = resolveHost(_THINGSERVER, 10);        // Use DNS to get server IP
//    delay(100);
//    #endif //_THINGSERVER
    
    readSeen(_SEENFILE, listSeen);              // read the seenFile records
    
    delay(100);                       // Wait after setup
    
    // Setup and initialise LoRa state machine of _loraModem.ino
    _state = S_INIT;
    initLoraModem();
    
    if (gwayConfig.cad) {
    _state = S_SCAN;
    sf = SF7;
    cadScanner();                   // Always start at SF7
    }
    else {
      _state = S_RX;
      rxLoraModem();
    }
    LoraUp.payLoad[0] = 0;
    LoraUp.payLength = 0;                 // Init the length to 0
    
    // init interrupt handlers, which are shared for GPIO15 / D8,
    // we switch on HIGH interrupts
    if (pins.dio0 == pins.dio1) {
    attachInterrupt(pins.dio0, Interrupt_0, RISING);  // Share interrupts
    }
    // Or in the traditional Comresult case
    else {
      attachInterrupt(pins.dio0, Interrupt_0, RISING);  // Separate interrupts
      attachInterrupt(pins.dio1, Interrupt_1, RISING);  // Separate interrupts
      //attachInterrupt(pins.dio2, Interrupt_2, RISING);  // Separate interrupts
    }
    
    # if _MONITOR>=1
    if ((debug >= 2) && (pdebug & P_TX)) {
      mPrint("sendPacket:: STRICT=" + String(_STRICT_1CH) );
    }
    # endif //_MONITOR
    
    writeConfig(_CONFIGFILE, &gwayConfig);          // Write config
    writeSeen(_SEENFILE, listSeen);             // Write the last time record  is seen
    
    
    mPrint(" --- Setup() ended, Starting loop() ---");
    
//    configTime(8 * 3600, 0, "cn.pool.ntp.org");pool.ntp.org

     if(getway_utc == "UTC")
        configTime(0 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+1")
        configTime(1 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+2")
        configTime(2 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+3")
        configTime(3 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+4")
        configTime(4 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+5")
        configTime(5 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+6")
        configTime(6 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+7")
        configTime(7 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+8")
        configTime(8 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+9")
        configTime(9 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+10")
        configTime(10 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+11")
        configTime(11 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC+12")
        configTime(12 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-1")
        configTime(-1 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-2")
        configTime(-2 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-3")
        configTime(-3 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-4")
        configTime(-4 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-5")
        configTime(-5 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-6")
        configTime(-6 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-7")
        configTime(-7 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-8")
        configTime(-8 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-9")
        configTime(-9 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-10")
        configTime(-10 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-11")
        configTime(-11 * 3600, 0, "pool.ntp.org");
     else if(getway_utc == "UTC-12")
        configTime(-12 * 3600, 0, "pool.ntp.org");
     
     ST7735_Clear();
     
     if(mode_getwag == "net") 
     {
        IPAddress ip = ETH.localIP();
        ST7735_SHOW_TEST(0, 75, 128, 12, 10, 0, "IP:" + ip.toString() , 3, 1);  
     }
     else
      if(mode_getwag == "wifi") 
     {
        IPAddress ip =  WiFi.localIP();
        ST7735_SHOW_TEST(0, 75, 128, 12, 10, 0, "IP:" + ip.toString(), 3, 1);
     }

     ST7735_SHOW_TEST(0, 90, 128, 12, 10, 0, "ID:" + gatewayid, 3, 1); 
     ST7735_SHOW_TEST(0, 105, 128, 12, 10, 0, "REGIN:" + region, 3, 1);
     ST7735_SHOW_TEST(0, 120, 128, 12, 10, 0, "RX: 0", 3, 1);
     ST7735_SHOW_TEST(0, 135, 128, 12, 10, 0, "TX: 0", 3, 1);

//
//     Serial.println("txLoraModem start...");
//
//      while (1)
//      {
//        txLoraModem(&LoraDown);
//        Serial.println("txLoraModem ...");
//        delay(2000);
//      }

}
else
{

  ST7735_Clear();
  ST7735_SHOW_TEST(0, 40, 128, 15, 20, 0, "GATEWAY INIT", 3, 2);
  ST7735_SHOW_TEST(0, 70, 128, 11, 10, 0, "SSID: ELECROW-GW1C", 3, 1);
  ST7735_SHOW_TEST(0, 90, 128, 11, 10, 0, "PASSWORD: aaaabbbb", 3, 1);
  ST7735_SHOW_TEST(0, 110, 128, 11, 10, 0, "IP: 192.168.4.1", 3, 1);
  
  initSoftAP();
  initWebServer();
}

}//setup



// ----------------------------------------------------------------------------
// LOOP
// This is the main program that is executed time and time again.
// We need to give way to the backend WiFi processing that
// takes place somewhere in the ESP8266 firmware and therefore
// we include yield() statements at important points.
//
// Note: If we spend too much time in user processing functions
// and the backend system cannot do its housekeeping, the watchdog
// function will be executed which means effectively that the
// program crashes.
// We use yield() to avoid ANY watch dog activity of the program.
//
// NOTE2: For ESP make sure not to do large array declarations in loop();
// ----------------------------------------------------------------------------

void loop ()
{

  int cont_rest = 0;
  // 检测按键是否按下，当按键按下时将返回低电平
  if (digitalRead(GATEWAY_REST_PIN) == LOW) {
    delay(50);
    // 按键按下 计时
    while(digitalRead(GATEWAY_REST_PIN) == LOW)
    {
      delay(100);
      cont_rest++;
      Serial.println("rest cont = " + String(cont_rest));
      if(cont_rest == 30)
      {        
        prefs.begin("gatewayConfig",false);//为false才能删除键值
        Serial.println(prefs.freeEntries());//查询清除前的剩余空间
        
        prefs.remove("netMode"); 
        prefs.remove("wifiid"); 
        prefs.remove("wifipass"); 
        prefs.remove("gatewayid"); 
        prefs.remove("serverAddr"); 
        prefs.remove("serverPort"); 
        prefs.remove("region"); 
        prefs.remove("getway_channel"); 
        prefs.remove("getway_sf"); 
        prefs.clear();

        ST7735_Clear();
        delay(500);
        Serial.println(prefs.freeEntries());//查询清除后的剩余空间
        prefs.end();
    
        Serial.println("remove config --> ESP32 restart ");
        ESP.restart(); //重启ESP32
      }
    }
    
  } 

  int packetSize;
  uint32_t nowSeconds = now();

  gateway_server.handleClient();

   if(mode_getwag == "wifi" || mode_getwag == "net") 
  {

      ST7735_GET_TOUCH();
    
      // So if we are connected
      // Receive UDP PUSH_ACK messages from server. (*2, par. 3.3)
      // This is important since the TTN broker will return confirmation
      // messages on UDP for every message sent by the gateway. So we have to consume them.
      // As we do not know when the server will respond, we test in every loop.
      while ( (packetSize = Udp.parsePacket()) > 0) {
    #		if _MONITOR>=1
        if ((debug >= 3) && (pdebug & P_TX)) {
          mPrint("loop:: readUdp available");
        }
    #		endif //_MONITOR
    
        // DOWNSTREAM
        // Packet may be PKT_PUSH_ACK (0x01), PKT_PULL_ACK (0x03) or PKT_PULL_RESP (0x04)
        // This command is found in byte 4 (buffer[3])
    
        if (readUdp(packetSize) < 0) {
    #			if _MONITOR>=1
          if (debug >= 0)
            mPrint("Dwn readUdp ERROR, returning < 0");
    #			endif //_MONITOR
          break;
        }
        // Now we know we succesfully received message from host
        // If return value is 0, we received a NTP message,
        // otherwise a UDP message with other TTN content, all ACKs are 4 bytes long
        else {
          //_event=1;									// Could be done double if more messages received
          //mPrint("Dwn udp received="+String(micros())+", packetSize="+String(packetSize));
        }
      }
    
      yield();
    
      // check for event value, which means that an interrupt has arrived.
      // In this case we handle the interrupt ( e.g. message received)
      // in userspace in loop().
      //
      stateMachine();											// do the state machine
    
      // After a quiet period, make sure we reinit the modem and state machine.
      // The interval is in seconds (about 15 seconds) as this re-init
      // is a heavy operation.
      // So it will kick in if there are not many messages for the gateway.
      // Note: Be careful that it does not happen too often in normal operation.
      //
      if ( ((nowSeconds - statr[0].time) > _MSG_INTERVAL) &&
           (msgTime <= statr[0].time) )
      {
    #		if _MONITOR>=1
        if ((debug >= 2) && (pdebug & P_MAIN)) {
          String response = "";
          response += "REINIT:: ";
          response += String( _MSG_INTERVAL );
          response += (" ");
          mStat(0, response);
          mPrint(response);
        }
    #		endif //_MONITOR
    
        yield();											// Allow buffer operations to finish
    
        if ((gwayConfig.cad) || (gwayConfig.hop)) {
          _state = S_SCAN;
          sf = SF7;
          cadScanner();
        }
        else {
          _state = S_RX;
          rxLoraModem();
        }
        writeRegister(REG_IRQ_FLAGS_MASK, (uint8_t) 0x00);
        writeRegister(REG_IRQ_FLAGS, (uint8_t) 0xFF);		// Reset all interrupt flags
        msgTime = nowSeconds;
      }
    
      // If event is set, we know that we have a (soft) interrupt.
      // After all necessary web/OTA services are scanned, we will
      // reloop here for timing purposes.
      // Do as less yield() as possible.
      if (_event == 1) {
        return;
      }
      else yield();
       
      // stat PUSH_DATA message (*2, par. 4)
      //
      if ((nowSeconds - statTime) >= _STAT_INTERVAL) {		// Wake up every xx seconds
        yield();											// on 26/12/2017
        sendstat();											// Show the status message and send to server
    #		if _MONITOR>=1
        if ((debug >= 2) && (pdebug & P_MAIN)) {
          mPrint("Send Pushdata sendstat");
        }
    #		endif //_MONITOR
    
        // If the gateway behaves like a node, we do from time to time
        // send a node message to the backend server.
        // The Gateway node message has nothing to do with the STAT_INTERVAL
        // message but we schedule it in the same frequency.
        //
    #		if _GATEWAYNODE==1
        if (gwayConfig.isNode) {
          // Give way to internal some Admin if necessary
          yield();
    
          // If the 1ch gateway is a sensor itself, send the sensor values
          // could be battery but also other status info or sensor info
    
          if (sensorPacket() < 0) {
    #				if _MONITOR>=1
            if ((debug >= 1) || (pdebug & P_MAIN)) {
              mPrint("sensorPacket: Error");
            }
    #				endif //_MONITOR
          }
        }
    #		endif//_GATEWAYNODE
        statTime = nowSeconds;
      }
    
    
      // send PULL_DATA message (*2, par. 4)
      //
      // This message will also restart the server which taken approx. 3 ms.
      nowSeconds = now();
      if ((nowSeconds - pullTime) >= _PULL_INTERVAL) {		// Wake up every xx seconds
    
        yield();
        pullData();											// Send PULL_DATA message to server
        //startReceiver();
        pullTime = nowSeconds;
    
    #		if _MONITOR>=1
        if ((debug >= 2) && (pdebug & P_RX)) {
          String response = "UP ESP-sc-gway:: PKT_PULL_DATA message sent: micr=";
          printInt(micros(), response);
          mPrint(response);
        }
    #		endif //_MONITOR
      }
    
    
      // send RESET_DATA message (*2, par. 4)
      //
      // This message will also restart the server which taken approx. 3 ms.
      nowSeconds = now();
      if ((nowSeconds - rstTime) >= _RST_INTERVAL) {			// Wake up every xx seconds
    
        yield();
        startReceiver();
        rstTime = nowSeconds;
    
    #		if _MONITOR>=1
        if ((debug >= 2) && (pdebug & P_MAIN)) {
          String response = "UP ESP-sc-gway:: RST_DATA message sent: micr=";
          printInt(micros(), response);
          mPrint(response);
        }
    #		endif //_MONITOR
      }
    
    
      // If we do our own NTP handling (advisable)
      // We do not use the timer interrupt but use the timing
      // of the loop() itself which is better for SPI
//    #	if NTP_INTR==0
//      // Set the time in a manual way. Do not use setSyncProvider
//      //  as this function may collide with SPI and other interrupts
//      // Note: It can be that we do not receive a time this loop (no worries)
//      yield();
//      if (nowSeconds - ntptimer >= _NTP_INTERVAL) {
//        yield();
//        time_t newTime;
//        if (getNtpTime(&newTime) <= 0) {
//    #				if _MONITOR>=1
//          if (debug >= 2) {
//            mPrint("loop:: WARNING Time not set (yet). Time=" + String(newTime) );
//          }
//    #				endif //_MONITOR
//        }
//        else {
//          setTime(newTime);
//          if (year(now()) != 1970) {						// Do not when year(now())=="1970"
//            ntptimer = nowSeconds;						// beacause of "FORMAT"
//          }
//        }
//      }
//    #	endif//NTP_INTR
    
//    #	if _MAXSEEN>=1
//      if ((nowSeconds - fileTime) >= _FILE_INTERVAL) {
//        writeSeen(_SEENFILE, listSeen);
//        fileTime = nowSeconds;
//      }
//    #	endif //_MAXSEEN
    
  }
  else
  {
     #if _MONITOR>=1
        Serial.println("Config Network ....");
        delay(1000);
    #endif //_MONITOR
  }

}//loop
