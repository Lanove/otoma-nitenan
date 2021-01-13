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
#include <SPIFFS.h>
#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"
#include <nitenan_config.h>

DynamicJsonDocument json(2048);
const char *ssid = "aefocs";
const char *password = "000354453000";

String serverName = "192.168.2.119";

String serverPath = "/otoma/api/nitenanControllerRequest.php"; // The default serverPath should be upload.php

WiFiClient client;
const int serverPort = 8080;
const int timerInterval = 1000;   // time between each HTTP POST image
unsigned long previousMillis = 0; // last time image was sent
// config.json API
bool loadConfig();
bool writeConfig();
const char *sendPOST(const char *requestHead, const char *contentType, const char *requestUri, uint8_t *payload, size_t payloadSize);
String sendPhoto();
// const String
// buildVer = BUILD_VERSION,
//  sdkVer = ESP.getSdkVersion(),
//  chipRev = String(ESP.getChipRevision()),
//  freeSketch = String(ESP.getFreeSketchSpace()),
//  sketchSize = String(ESP.getSketchSize()),
//  chipSize = String(ESP.getFlashChipSize()),
//  sketchMD5 = ESP.getSketchMD5();
//  cpuFreq = String(ESP.getCpuFreqMHz());
//  macAddr = WiFi.macAddress();

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.printf(
      "buildVer : %s\nsdkVer : %s\nchipRev : %lu\nfreeSketch : %lu\nsketchSize : %lu\nflashChipSize : %lu\nsketchMD5 : %s\ncpuFreq : %lu\nmacAddr : %s\n",
      BUILD_VERSION,
      ESP.getSdkVersion(),
      ESP.getChipRevision(),
      ESP.getFreeSketchSpace(),
      ESP.getSketchSize(),
      ESP.getFlashChipSize(),
      ESP.getSketchMD5().c_str(),
      ESP.getCpuFreqMHz(),
      WiFi.macAddress().c_str());
  Serial.println("Mounting FS...");

  if (!SPIFFS.begin())
  {
    Serial.println("Failed to mount file system");
    delay(1000);
    ESP.restart();
  }

  if (!loadConfig())
  {
    Serial.println("Failed to load config");
    delay(1000);
    ESP.restart();
  }
  else
    Serial.println("Config loaded");
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

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
  if (psramFound())
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10; //0-63 lower number means higher quality
    config.fb_count = 2;
  }
  else
  {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12; //0-63 lower number means higher quality
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }
}

void loop()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= timerInterval)
  {
    Serial.println("Sending photo");
    sendPhoto();
    Serial.println("Done sending photo");
    previousMillis = currentMillis;
  }
}

String sendPhoto()
{
  String getAll;
  String getBody;

  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }
  Serial.println("Connecting to server: " + serverName);

  if (client.connect(serverName.c_str(), serverPort))
  {
    Serial.println("Connected to server");
    String head = "--AAA\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    Serial.println(sendPOST(head.c_str(), "multipart/form-data; boundary=AAA", requestURL, fb->buf, fb->len));
    esp_camera_fb_return(fb);
    client.stop();
  }
  else
  {
    getBody = "Connection to " + serverName + " failed.";
    Serial.println(getBody);
  }
  return getBody;
}

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