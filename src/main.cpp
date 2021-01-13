/*
isi config.json
{
  "WIFI_SSID":"aefocs",
  "WIFI_PASS":"000354453000",
  "AP_SSID":"Otoma-Nitenan",
  "AP_PASS":"lpkojihu",
  "USERNAME": "gold",
  "DEVICETOKEN": "4rPN0sNk69"
}
*/
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"
#include <nitenan_config.h>
#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <WebServer.h>

DynamicJsonDocument json(2048);
WiFiClient client;
WebServer server(80);

unsigned long lastHTTPRequest = 0;

// Camera API
String sendPhoto(); // This function sends image and receive response from server about flashlight,servo,etc
void initializeCamera();

// config.json API
bool loadConfig();
bool writeConfig();

// WiFi API
const char *sendPOST(const char *requestHead, const char *contentType, const char *requestUri, uint8_t *payload, size_t payloadSize);
void initializeWiFi();
bool initiateSoftAP();
bool initiateClient(const char *ssid, const char *pass);
void closeSoftAP();
void closeClient();
void deployWebServer();
void closeServer();
bool isWifiConnected();

// Web server API
void pgRoot();
void pgAccInfo();
void pgReqStatus();
void handleNotFound();
void pgRestart();

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  log_d("buildVer : %s\nsdkVer : %s\nchipRev : %lu\nfreeSketch : %lu\nsketchSize : %lu\nflashChipSize : %lu\nsketchMD5 : %s\ncpuFreq : %luMHz\nmacAddr : %s\n", BUILD_VERSION, ESP.getSdkVersion(), ESP.getChipRevision(), ESP.getFreeSketchSpace(), ESP.getSketchSize(), ESP.getFlashChipSize(), ESP.getSketchMD5().c_str(), ESP.getCpuFreqMHz(), WiFi.macAddress().c_str());
  log_d("Mounting FS...");
  delay(1000);
  if (!SPIFFS.begin())
  {
    log_d("Failed to mount file system");
    delay(1000);
    ESP.restart();
  }
  delay(100);
  if (!loadConfig())
  {
    log_d("Failed to load config");
    delay(1000);
    ESP.restart();
  }
  else
    log_d("Config loaded");

  initializeCamera();
  initializeWiFi();
}

bool disconnectFlag;
unsigned long disconnectStamp;
void loop()
{
  const unsigned long currentMillis = millis();
  if (serverAvailable)
  {
    server.handleClient(); // Listen for HTTP requests from clients
  }
  else
  {
    if (isWifiConnected()) // If WiFi is connected
    {
      if (currentMillis - lastHTTPRequest >= HTTP_REQUEST_INTERVAL)
      {
        const String responsePayload = sendPhoto();
        log_d("Response : %s", responsePayload.c_str());
        lastHTTPRequest = currentMillis;
        if(responsePayload != "Failed"){
        }else{
          if (!disconnectFlag)
          {
            disconnectFlag = true;
            disconnectStamp = millis();
          }
        }
      }
    }
    else
    {
      if (!disconnectFlag)
      {
        disconnectFlag = true;
        disconnectStamp = millis();
      }
    }
  }
  if (disconnectFlag && millis() - disconnectStamp >= MAXIMUM_DISCONNECT_TIME)
  {
    json["WIFI_SSID"] = "KONEKSI TERPUTUS";
    json["WIFI_PASS"] = "";
    writeConfig();
    delay(500);
    ESP.restart();
  }
}

/////////////////// Camera API ///////////////////////////
String sendPhoto()
{
  String response;
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb)
  {
    log_d("Camera capture failed");
    return "Failed";
  }
  log_d("Connecting to server: %s", baseUri);

  if (client.connect(baseUri, serverPort))
  {
    log_d("Connected to server");
    String head = "--AAA\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    response = sendPOST(head.c_str(), "multipart/form-data; boundary=AAA", requestURL, fb->buf, fb->len);
    log_d("%s", response.c_str());
    esp_camera_fb_return(fb);
    client.stop();
  }
  else{
    log_d("Connection to %s failed.", baseUri);
    response = "Failed";
  }
  return response;
}

void initializeCamera()
{
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // init with high specs to pre-allocate larger buffers
  log_d("psramfound : %s\npsram_size : %lu\n", (psramFound()) ? "true" : "false", ESP.getPsramSize());
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 10; //0-63 lower number means higher quality
  config.fb_count = 2;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    log_d("Camera init failed with error 0x%x", err);
    delay(3000);
    ESP.restart();
  }
}
/////////////////////////////////////////////////////////

/////////////////// config.json API /////////////////////
bool writeConfig()
{
  File configFile = SPIFFS.open("/config.json", FILE_WRITE);
  if (!configFile)
  {
    log_d("Failed to open config file");
    return false;
  }
  serializeJson(json, configFile);
  configFile.close();
  return true;
}

bool loadConfig()
{
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile)
  {
    log_d("Failed to open config file");
    return false;
  }
  size_t size = configFile.size();
  if (size > 1024)
  {
    log_d("Config file size is too large");
    return false;
  }
  size_t len = measureJson(json);
  log_d("JSON Size : %lu\n", len);
  auto error = deserializeJson(json, configFile);
  if (error)
  {
    log_d("deserializeJson() failed with code %s", error.c_str());
    return false;
  }
  log_d("Hasil Baca config.json : \nIS_CONNECTED : %s\nWIFI_ERROR_FLAG1 : %s\nWIFI_ERROR_FLAG2 : %s\nWIFI_SSID : %s\nWIFI_PASS : %s\nAP_SSID : %s\nAP_PASS : %s\nUSERNAME : %s\nUSERPASS : %s\nDEVICE_TOKEN : %s\nAP_IP_ADDRESS : %d.%d.%d.%d",
        (json["IS_CONNECTED"].as<bool>()) ? "true" : "false",
        (json["WIFI_ERROR_FLAG1"].as<bool>()) ? "true" : "false",
        (json["WIFI_ERROR_FLAG2"].as<bool>()) ? "true" : "false",
        json["WIFI_SSID"].as<const char *>(),
        json["WIFI_PASS"].as<const char *>(),
        json["AP_SSID"].as<const char *>(),
        json["AP_PASS"].as<const char *>(),
        json["USERNAME"].as<const char *>(),
        json["USERPASS"].as<const char *>(),
        json["DEVICE_TOKEN"].as<const char *>(),
        json["AP_IP_ADDRESS"][0].as<int>(),
        json["AP_IP_ADDRESS"][1].as<int>(),
        json["AP_IP_ADDRESS"][2].as<int>(),
        json["AP_IP_ADDRESS"][3].as<int>());
  configFile.close();
  return true;
}
/////////////////////////////////////////////////

/////////////////////// WIFI API /////////////////////////////
bool serverAvailable; // Variable to store esp's web server status, true when web server is online
const char *sendPOST(const char *requestHead, const char *contentType, const char *requestUri, uint8_t *payload, size_t payloadSize)
{
  const char *tail = "\r\n--AAA--\r\n";
  const uint32_t requestLen = payloadSize + strlen(requestHead) + strlen(tail);
  String getBody;
  String getAll;
  long startTimer = millis();
  boolean state = false;
  client.printf("POST %s HTTP/1.1\r\n", requestUri);
  client.printf("Host: %s\r\n", baseUri);
  client.printf("HTTP_DEVICE_TOKEN: %s\r\n", json["DEVICE_TOKEN"].as<const char *>());
  client.printf("HTTP_ESP32_BUILD_VERSION: %s\r\n", BUILD_VERSION);
  client.printf("HTTP_ESP32_SDK_VERSION: %s\r\n", ESP.getSdkVersion());
  client.printf("HTTP_ESP32_CHIP_VERSION: %lu\r\n", ESP.getChipRevision());
  client.printf("HTTP_ESP32_FREE_SKETCH: %lu\r\n", ESP.getFreeSketchSpace());
  client.printf("HTTP_ESP32_SKETCH_SIZE: %lu\r\n", ESP.getSketchSize());
  client.printf("HTTP_ESP32_FLASH_SIZE: %lu\r\n", ESP.getFlashChipSize());
  client.printf("HTTP_ESP32_SKETCH_MD5: %s\r\n", ESP.getSketchMD5().c_str());
  client.printf("HTTP_ESP32_CPU_FREQ: %lu\r\n", ESP.getCpuFreqMHz());
  client.printf("HTTP_ESP32_MAC: %s\r\n", WiFi.macAddress().c_str());
  client.printf("HTTP_ESP32_USERNAME: %s\r\n", json["USERNAME"].as<const char *>());
  client.printf("HTTP_ESP32_WIFI_SSID: %s\r\n", json["WIFI_SSID"].as<const char *>());
  client.printf("HTTP_ESP32_AP_SSID: %s\r\n", json["AP_SSID"].as<const char *>());
  client.printf("HTTP_ESP32_AP_PASS: %s\r\n", json["AP_PASS"].as<const char *>());
  client.printf("HTTP_ESP32_AP_IP: %d.%d.%d.%d\r\n", json["AP_IP_ADDRESS"][0].as<int>(), json["AP_IP_ADDRESS"][1].as<int>(), json["AP_IP_ADDRESS"][2].as<int>(), json["AP_IP_ADDRESS"][3].as<int>());
  client.printf("Content-Length: %lu\r\n", requestLen);
  client.printf("Content-Type: %s\r\n", contentType);
  client.println();
  client.printf("%s", requestHead);

  uint8_t *p = payload;
  for (size_t n = 0; n < payloadSize; n = n + 1024)
  {
    if (n + 1024 < payloadSize)
    {
      client.write(p, 1024);
      p += 1024;
    }
    else if (payloadSize % 1024 > 0)
    {
      size_t remainder = payloadSize % 1024;
      client.write(p, remainder);
    }
  }
  client.printf("%s", tail);
  while ((startTimer + HTTP_REQUEST_TIMEOUT) > millis())
  {
    delay(10);
    while (client.available())
    {
      char c = client.read();
      if (c == '\n')
      {
        if (getAll.length() == 0)
        {
          state = true;
        }
        getAll = "";
      }
      else if (c != '\r')
      {
        getAll += String(c);
      }
      if (state == true)
      {
        getBody += String(c);
      }
      startTimer = millis();
    }
    if (getBody.length() > 0)
    {
      break;
    }
  }
  return getBody.c_str();
}

void initializeWiFi()
{
  closeClient();
  closeSoftAP();
  closeServer();
  // If device is not connected yet to internet, we initiate with AP
  if (json["IS_CONNECTED"].as<bool>() == false)
  {
    initiateSoftAP();
    deployWebServer(); // Deploy web server
  }
  else // If it has been connected, then just start as client and connect to stored WiFi
  {
    int timeOutCounter = 0;    // Variable to store wifi failure connect attempts
    while (timeOutCounter < 2) // Try for maximum of 5 attempts
    {
      if (initiateClient(json["WIFI_SSID"].as<const char *>(), json["WIFI_PASS"].as<const char *>()))
      {
        // If the wifi is successfully connected, reset the error flag if one of them is set (hence the or)
        if (json["WIFI_ERROR_FLAG1"].as<bool>() || json["WIFI_ERROR_FLAG2"].as<bool>())
        {
          json["WIFI_ERROR_FLAG1"] = false;
          json["WIFI_ERROR_FLAG2"] = false;
          if (!writeConfig())
            log_d("Failed writing config"); // Something is wrong, print on debug port
          delay(10);
        }
        break; // Break the while loop, because wifi is successfully connected
      }
      timeOutCounter++;
    }

    if (timeOutCounter >= 2) // If even after 2 attempts wifi cannot connected, then
    {
      log_d("Fail reconnect WiFi");
      if (json["WIFI_ERROR_FLAG1"].as<bool>() == false) // If first flag is still reset, we try to reboot first
      {
        log_d("WiFi Failure reboot attempt 1");
        json["WIFI_ERROR_FLAG1"] = true;
        if (!writeConfig())
          log_d("Failed writing config"); // Something is wrong, print on debug port
        delay(500);
        ESP.restart();
      }
      else
      {
        if (json["WIFI_ERROR_FLAG2"].as<bool>() == false) // It seems first try is failure aswell, let's reboot again and try once more
        {
          log_d("WiFi Failure reboot attempt 2");
          json["WIFI_ERROR_FLAG2"] = true;
          if (!writeConfig())
            log_d("Failed writing config"); // Something is wrong, print on debug port
          delay(500);
          ESP.restart();
        }
        else // Seems like we still failed to connect to WiFi after 2 reboot, let's just start with AP and reset the whole IS_CONNECTED and WIFI_ERROR flags
        {
          delay(1000);
          initiateSoftAP();
          deployWebServer();
          json["IS_CONNECTED"] = false;
          json["WIFI_ERROR_FLAG1"] = false;
          json["WIFI_ERROR_FLAG2"] = false;
          if (!writeConfig())
            log_d("Failed writing config"); // Something is wrong, print on debug port
          delay(10);
        }
      }
    }
  }
}

bool isWifiConnected()
{
  return (WiFi.status() == WL_CONNECTED);
}

bool initiateSoftAP()
{
  bool timeOutFlag = 1;
  IPAddress local_IP(json["AP_IP_ADDRESS"][0].as<int>(), json["AP_IP_ADDRESS"][1].as<int>(), json["AP_IP_ADDRESS"][2].as<int>(), json["AP_IP_ADDRESS"][3].as<int>());
  IPAddress gateway(json["AP_IP_ADDRESS"][0].as<int>(), json["AP_IP_ADDRESS"][1].as<int>(), json["AP_IP_ADDRESS"][2].as<int>(), json["AP_IP_ADDRESS"][3].as<int>());
  IPAddress subnet(255, 255, 255, 0);
  delay(100);
  for (int i = 0; i < 20; i++)
  {
    if (WiFi.softAPConfig(local_IP, gateway, subnet))
    {
      log_d("softAP configured");
      timeOutFlag = 0;
      break;
    }
    else
    {
      log_d("Configuring AP...");
      delay(500);
    }
  }
  if (timeOutFlag == 0) // Only attempt to open AP if AP config is successful
  {
    for (int i = 0; i < 20; i++)
    {
      if (WiFi.softAP(json["AP_SSID"].as<const char *>(), json["AP_PASS"].as<const char *>()))
      {
        log_d("soft AP started");
        timeOutFlag = 0;
        break;
      }
      else
      {
        log_d("Starting AP...");
        delay(500);
      }
    }
  }
  return timeOutFlag;
}

bool initiateClient(const char *ssid, const char *pass)
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  bool successFlag = false;
  for (int i = 0; i < 30; i++)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      successFlag = true;
      break;
    }
    else
    {
      log_d("Attempting to connect wifi...");
      delay(500);
    }
  }
  return successFlag;
}

void closeSoftAP()
{
  WiFi.softAPdisconnect(true);
}

void closeClient()
{
  WiFi.disconnect(true);
}

void deployWebServer()
{
  // Call the 'handleRoot' function when a client requests URI "/"
  server.on("/", HTTP_GET, pgRoot); // HTML document

  server.on("/reqStatus", HTTP_POST, pgReqStatus); // Request is called when page is requesting config.json informations to display to web server
  server.on("/accInfo", HTTP_POST, pgAccInfo);     // Request is called when user press submit button from web
  server.on("/restart", HTTP_POST, pgRestart);     // Request is called when user press restart button from web

  // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
  server.onNotFound(handleNotFound);
  server.begin();
  serverAvailable = true;
}

void closeServer()
{
  server.close();
  serverAvailable = false;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////// WEB SERVER REQUEST HANDLER API ////////////////////////////////////////
String responseStatus = "empty";

void pgRoot() // Root HTML document callback
{
  server.send(200, "text/html", FPSTR(htmlDoc));
}

void pgRestart() // Request is called when user press restart button from web
{
  server.send(200, "text/plain", "restart");
  delay(500);
  ESP.restart();
}

void pgAccInfo() // Request is called when user press submit button from web
{
  bool writeConfigFlag = false;
  bool failFetch = true;
  String usrn = server.arg(F("usrn"));
  String unpw = server.arg(F("unpw"));
  String ssid = server.arg(F("ssid"));
  String wfpw = server.arg(F("wfpw"));

  if (usrn.length() > 32)
    usrn = "overlength";
  if (unpw.length() > 64)
    unpw = "overlength";
  if (ssid.length() > 32)
    ssid = "overlength";
  if (wfpw.length() > 64)
    wfpw = "overlength";

  log_d("Username : %s\nPassword : %s\nSSID : %s\nWiFiPW : %s\n", usrn.c_str(), unpw.c_str(), ssid.c_str(), wfpw.c_str());

  if (initiateClient(ssid.c_str(), wfpw.c_str()))
  {
    // This WiFi seems legit, let's save to config.json
    json["WIFI_SSID"] = ssid.c_str();
    json["WIFI_PASS"] = ssid.c_str();
    writeConfigFlag = true;
    if (client.connect(baseUri, serverPort)) // Try to connect to web
    {
      log_d("Connected to server, begin post request");
      // Initiate HTTP Request to identifyDevice.php
      StaticJsonDocument<360> doc;
      String jsonString;
      doc[F("username")] = usrn.c_str();
      doc[F("password")] = unpw.c_str();
      doc[F("softssid")] = json["AP_SSID"].as<const char *>();
      doc[F("softpswd")] = json["AP_PASS"].as<const char *>();
      serializeJson(doc, jsonString);
      log_d("JSON Size : %d", doc.memoryUsage());
      log_d("Transferred JSON : %s", jsonString.c_str());
      String head = "--AAA\r\nContent-Disposition: form-data; name=\"requestJSON\"; filename=\"request.json\"\r\nContent-Type: application/json\r\n\r\n";
      const String response = String(sendPOST(head.c_str(), "multipart/form-data; boundary=AAA", identifyURL, (uint8_t *)jsonString.c_str(), jsonString.length()));
      log_d("Response : %s", response.c_str());
      if (response == "success" || response == "recon")
      {
        json["USERNAME"] = usrn;
        json["USERPASS"] = unpw;
        json["IS_CONNECTED"] = true;
        writeConfigFlag = true;
        delay(10);
      }
      client.stop();
    }
    else // Unable to connect to web, probably it's a wifi with no internet
      responseStatus = "nocon";
  }
  else // Cannot connect to WiFi, report invalid SSID or Password!
    responseStatus = "invwifi";
  // Reinitiate softAP and re deploy the web server to 192.168.4.1
  if (writeConfigFlag)
    writeConfig();
  delay(100);
  closeClient();
  closeServer();
  closeSoftAP();
  initiateSoftAP();
  deployWebServer();
}

void pgReqStatus() // Request is called when page is requesting config.json informations to display to web server
{
  StaticJsonDocument<360> doc;
  String jsonString;
  doc[F("usrn")] = json["USERNAME"].as<const char *>();
  doc[F("unpw")] = json["USERPASS"].as<const char *>();
  doc[F("ssid")] = json["WIFI_SSID"].as<const char *>();
  doc[F("wfpw")] = json["WIFI_PASS"].as<const char *>();
  doc[F("message")] = responseStatus.c_str();
  serializeJson(doc, jsonString);
  log_d("JSON Size : %d\n", doc.memoryUsage());
  log_d("Transferring JSON : %s\n", jsonString.c_str());
  server.send(200, "application/json", jsonString);
}

void handleNotFound() // Invalid page callback
{
  server.send(404, "text/plain", F("404: Not found"));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
