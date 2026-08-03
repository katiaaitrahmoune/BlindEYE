#include "esp_camera.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

const char* ssid = "katia";
const char* password = "katia2005";

// ==== CAMERA PINS ====
#define PWDN_GPIO_NUM    26
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    32
#define SIOD_GPIO_NUM    13
#define SIOC_GPIO_NUM    12
#define Y9_GPIO_NUM      39
#define Y8_GPIO_NUM      36
#define Y7_GPIO_NUM      23
#define Y6_GPIO_NUM      18
#define Y5_GPIO_NUM      15
#define Y4_GPIO_NUM      4
#define Y3_GPIO_NUM      14
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM   27
#define HREF_GPIO_NUM    25
#define PCLK_GPIO_NUM    19

#define BUTTON 21

const char* imageUrl = "https://hosted-server-vufd.onrender.com/upload/image";

// ==== SEND IMAGE ====
void sendSnapshot(String sessionId) {
  // Discard one stale frame first, so what we actually send is genuinely fresh
  camera_fb_t* stale = esp_camera_fb_get();
  if (stale) esp_camera_fb_return(stale);

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(imageUrl) + "?session_id=" + sessionId;
    http.begin(client, url);
    http.setTimeout(60000);
    http.setConnectTimeout(60000);

    String boundary = "BlindEyeBoundary";
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

    String head = "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"image\"; filename=\"snap.jpg\"\r\n";
    head += "Content-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--" + boundary + "--\r\n";

    uint32_t totalLen = head.length() + fb->len + tail.length();
    http.addHeader("Content-Length", String(totalLen));

    uint8_t* body = (uint8_t*)malloc(totalLen);
    if (body) {
      memcpy(body, head.c_str(), head.length());
      memcpy(body + head.length(), fb->buf, fb->len);
      memcpy(body + head.length() + fb->len, tail.c_str(), tail.length());

      int httpCode = http.POST(body, totalLen);
      free(body);

      Serial.printf("Image upload code: %d\n", httpCode);
      if (httpCode > 0) {
        Serial.println(http.getString());
      }
    } else {
      Serial.println("malloc failed — not enough memory");
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }

  esp_camera_fb_return(fb);
}

// ==== SETUP ====
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // ---- Fixes for stale/old frames ----
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }

  // ---- Sensor tuning for brightness/exposure ----
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 1);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_gainceiling(s, (gainceiling_t)6);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_vflip(s, 1);   // flip vertically -- try 1 first, set back to 0 if it looks wrong
    s->set_hmirror(s, 0); // mirror horizontally if left/right also look swapped
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  // ---- DNS FIX ----
  Serial.print("DNS before override: ");
  Serial.println(WiFi.dnsIP());

  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);

  Serial.print("DNS after override: ");
  Serial.println(WiFi.dnsIP());

  pinMode(BUTTON, INPUT_PULLUP);
}

// ==== LOOP ====
void loop() {
  if (digitalRead(BUTTON) == LOW) {
    Serial.println("Button pressed!");

    String sessionId = "esp32-" + String(millis());
    Serial.println("Session ID: " + sessionId);

    sendSnapshot(sessionId);

    delay(5000);
  }
  delay(1);
}