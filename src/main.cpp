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
#include <nitenan_config.h>
#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"

const char *ssid = "aefocs";
const char *password = "000354453000";

String serverName = "192.168.2.130";

String serverPath = "/otoma/api/nitenanControllerRequest.php"; // The default serverPath should be upload.php

const int serverPort = 8080;

WiFiClient client;

const int timerInterval = 1000;   // time between each HTTP POST image
unsigned long previousMillis = 0; // last time image was sent

String sendPhoto();
bool loadConfig();
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
    sendPhoto();
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
    Serial.println("Connection successful!");
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();
    client.print(head);

    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n = n + 1024)
    {
      if (n + 1024 < fbLen)
      {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen % 1024 > 0)
      {
        size_t remainder = fbLen % 1024;
        client.write(fbBuf, remainder);
      }
    }
    client.print(tail);

    esp_camera_fb_return(fb);

    int timoutTimer = 10000;
    long startTimer = millis();
    boolean state = false;

    while ((startTimer + timoutTimer) > millis())
    {
      Serial.print(".");
      delay(100);
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
    Serial.println();
    client.stop();
    Serial.println(getBody);
  }
  else
  {
    getBody = "Connection to " + serverName + " failed.";
    Serial.println(getBody);
  }
  return getBody;
}
bool loadConfig()
{
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile)
  {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024)
  {
    Serial.println("Config file size is too large");
    return false;
  }
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();
  Serial.printf("config.json content : %s \n\n", buf.get());
  StaticJsonDocument<1024> json;
  size_t len = measureJson(json);
  Serial.printf("JSON Size : %lu\n", len);
  auto error = deserializeJson(json, buf.get());
  if (error)
  {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(error.c_str());
    return false;
  }
  Serial.printf("Hasil Baca config.json : \nIS_CONNECTED : %s\nWIFI_ERROR_FLAG1 : %s\nWIFI_ERROR_FLAG2 : %s\nWIFI_SSID : %s\nWIFI_PASS : %s\nAP_SSID : %s\nAP_PASS : %s\nUSERNAME : %s\nDEVICE_TOKEN : %s\n",
                (json["IS_CONNECTED"].as<bool>()) ? "true" : "false",
                (json["WIFI_ERROR_FLAG1"].as<bool>()) ? "true" : "false",
                (json["WIFI_ERROR_FLAG2"].as<bool>()) ? "true" : "false",
                json["WIFI_SSID"].as<const char *>(),
                json["WIFI_PASS"].as<const char *>(),
                json["AP_SSID"].as<const char *>(),
                json["AP_PASS"].as<const char *>(),
                json["USERNAME"].as<const char *>(),
                json["DEVICE_TOKEN"].as<const char *>());
  return true;
}