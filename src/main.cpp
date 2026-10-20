#include "esp_camera.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
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

// ==== NGROK SERVER (update when ngrok restarts) ====
const char* serverBase = "https://spry-envelope-angular.ngrok-free.dev";
const char* sessionId  = "liveSession1";

bool connectWithRetry(WiFiClientSecure& client, const char* host) {
  client.setInsecure();
  client.setHandshakeTimeout(30);   // seconds
  for (int i = 1; i <= 3; i++) {
    if (client.connect(host, 443)) return true;
    Serial.printf("TLS connect failed (try %d/3)\n", i);
    client.stop();
    delay(1500);
  }
  return false;
}

void sendSnapshot() {
  // Discard one stale frame so the sent one is fresh
  camera_fb_t* stale = esp_camera_fb_get();
  if (stale) esp_camera_fb_return(stale);

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    esp_camera_fb_return(fb);
    return;
  }

  Serial.printf("Free heap before HTTPS connect: %u bytes\n", ESP.getFreeHeap());

  String boundary = "BlindEyeBoundary";
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"image\"; filename=\"snap.jpg\"\r\n";
  head += "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t bodyLen = head.length() + fb->len + tail.length();

  // Clean host: strip scheme, spaces and trailing slash
  String hostOnly = String(serverBase);
  hostOnly.replace("https://", "");
  hostOnly.replace("http://", "");
  hostOnly.trim();
  if (hostOnly.endsWith("/")) hostOnly.remove(hostOnly.length() - 1);

  WiFiClientSecure client;
  if (!connectWithRetry(client, hostOnly.c_str())) {
    Serial.println("Connection to server failed");
    esp_camera_fb_return(fb);
    return;
  }

  String path = "/upload/image?session_id=" + String(sessionId);

  client.print(String("POST ") + path + " HTTP/1.1\r\n");
  client.print(String("Host: ") + hostOnly + "\r\n");
  client.print("ngrok-skip-browser-warning: 1\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(bodyLen) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  client.print(head);
  if (client.write(fb->buf, fb->len) != fb->len) {
    Serial.println("Image write failed");
    client.stop();
    esp_camera_fb_return(fb);
    return;
  }
  client.print(tail);

  Serial.println("Upload sent, waiting for response...");

  // If the S3 audio came first, the server runs the full pipeline before answering
  unsigned long timeout = millis();
  while (client.connected() && !client.available()) {
    if (millis() - timeout > 60000) {
      Serial.println("Response timeout");
      client.stop();
      esp_camera_fb_return(fb);
      return;
    }
    delay(10);
  }

  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }

  client.stop();
  Serial.println("Upload complete.");

  esp_camera_fb_return(fb);
}

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
  config.grab_mode    = CAMERA_GRAB_LATEST;

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
    s->set_vflip(s, 1);
    s->set_hmirror(s, 0);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                 // avoids dropped TLS connections
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  IPAddress dns1(8, 8, 8, 8);
  IPAddress dns2(1, 1, 1, 1);
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);

  pinMode(BUTTON, INPUT_PULLUP);
  Serial.println("Ready. Press button to capture and send photo.");
}

void loop() {
  if (digitalRead(BUTTON) == LOW) {
    Serial.println("Button pressed! Capturing photo...");
    sendSnapshot();
    delay(2000);   // debounce / cooldown
  }
  delay(1);
}