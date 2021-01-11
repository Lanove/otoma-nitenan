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
#include <StreamUtils.h>
#include "FS.h"
#include "SPIFFS.h"
#include <nitenan_config.h>
#include <Arduino.h>
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"

DynamicJsonDocument json(2048);

WiFiClient client;

unsigned long lastHTTPRequest = 0; // last time image was sent

String sendPhoto();
bool loadConfig();
bool writeConfig();

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  WiFi.mode(WIFI_STA);
  log_d(
      "buildVer : %s\nsdkVer : %s\nchipRev : %lu\nfreeSketch : %lu\nsketchSize : %lu\nflashChipSize : %lu\nsketchMD5 : %s\ncpuFreq : %luMHz\nmacAddr : %s\n",
      BUILD_VERSION,
      ESP.getSdkVersion(),
      ESP.getChipRevision(),
      ESP.getFreeSketchSpace(),
      ESP.getSketchSize(),
      ESP.getFlashChipSize(),
      ESP.getSketchMD5().c_str(),
      ESP.getCpuFreqMHz(),
      WiFi.macAddress().c_str());
  log_d("Mounting FS...");
  delay(1000);
  if (!SPIFFS.begin())
  {
    log_d("Failed to mount file system");
    delay(1000);
    ESP.restart();
  }
  delay(1000);
  if (!loadConfig())
  {
    log_d("Failed to load config");
    delay(1000);
    ESP.restart();
  }
  else
    log_d("Config loaded");

  log_d("Connecting to %s", json["WIFI_SSID"].as<const char *>());
  WiFi.begin(json["WIFI_SSID"].as<const char *>(), json["WIFI_PASS"].as<const char *>());
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
  }
  log_d("ESP32-CAM IP Address: %d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);

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
    log_d("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }
}

void loop()
{
  unsigned long currentMillis = millis();
  if (currentMillis - lastHTTPRequest >= HTTP_REQUEST_INTERVAL)
  {
    sendPhoto();
    lastHTTPRequest = currentMillis;
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
    log_d("Camera capture failed");
    return "Failed";
  }

  if (client.connect(baseUri, serverPort))
  {
    log_d("Connection successful!");
    String head = "--Otoma\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"" +json["DEVICE_TOKEN"].as<String>()+".jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--Otoma--\r\n";

    uint32_t imageLen = fb->len;
    uint32_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;

    client.printf("POST %s HTTP/1.1\r\n", requestURL);
    client.printf("Host: %s\r\n", baseUri);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=Otoma");
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

    long startTimer = millis();
    boolean state = false;

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
    client.stop();
    log_d("%s", getBody.c_str());
  }
  else
  {
    getBody = "Failed";
    log_d("Connection to %s failed.", baseUri);
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
  log_d("Hasil Baca config.json : \nIS_CONNECTED : %s\nWIFI_ERROR_FLAG1 : %s\nWIFI_ERROR_FLAG2 : %s\nWIFI_SSID : %s\nWIFI_PASS : %s\nAP_SSID : %s\nAP_PASS : %s\nUSERNAME : %s\nDEVICE_TOKEN : %s\n",
        (json["IS_CONNECTED"].as<bool>()) ? "true" : "false",
        (json["WIFI_ERROR_FLAG1"].as<bool>()) ? "true" : "false",
        (json["WIFI_ERROR_FLAG2"].as<bool>()) ? "true" : "false",
        json["WIFI_SSID"].as<const char *>(),
        json["WIFI_PASS"].as<const char *>(),
        json["AP_SSID"].as<const char *>(),
        json["AP_PASS"].as<const char *>(),
        json["USERNAME"].as<const char *>(),
        json["DEVICE_TOKEN"].as<const char *>());
  configFile.close();
  return true;
}