#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP_I2S.h>

const char* ssid = "katia";
const char* password = "katia2005";

// ==== I2S / MAX98357A PINS ====
// Change these if they conflict with anything else on your board.
#define I2S_BCLK  15
#define I2S_LRC   16
#define I2S_DOUT  17

I2SClass I2S;

const char* speechUrl = "https://res.cloudinary.com/digsropir/video/upload/v1784405182/blindeye/tts/ty6swflclj8akm5lfvuy.wav";

// ==== Parse the 44-byte standard WAV header ====
struct WavInfo {
  uint16_t numChannels;
  uint32_t sampleRate;
  uint16_t bitsPerSample;
};

bool parseWavHeader(const uint8_t* h, WavInfo &info) {
  if (memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0) {
    Serial.println("Not a valid WAV file (missing RIFF/WAVE header)");
    return false;
  }
  info.numChannels   = h[22] | (h[23] << 8);
  info.sampleRate    = h[24] | (h[25] << 8) | (h[26] << 16) | ((uint32_t)h[27] << 24);
  info.bitsPerSample = h[34] | (h[35] << 8);
  return true;
}

// ==== Stream WAV from URL straight to I2S, no big buffers, no PSRAM needed ====
void playWavFromUrl(const char* url) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, url);
  int httpCode = http.GET();
  Serial.printf("HTTP GET code: %d\n", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.println("Failed to fetch audio file");
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();

  // Read the 44-byte WAV header
  uint8_t header[44];
  size_t got = 0;
  unsigned long startWait = millis();
  while (got < 44) {
    if (stream->available()) {
      int n = stream->readBytes(header + got, 44 - got);
      got += n;
    } else if (millis() - startWait > 10000) {
      Serial.println("Timed out waiting for WAV header");
      http.end();
      return;
    }
  }

  WavInfo info;
  if (!parseWavHeader(header, info)) {
    http.end();
    return;
  }

  Serial.printf("WAV info -> channels: %d, sampleRate: %lu, bitsPerSample: %d\n",
                info.numChannels, info.sampleRate, info.bitsPerSample);

  // Map bits per sample to I2S data width
  i2s_data_bit_width_t bitWidth;
  switch (info.bitsPerSample) {
    case 8:  bitWidth = I2S_DATA_BIT_WIDTH_8BIT;  break;
    case 16: bitWidth = I2S_DATA_BIT_WIDTH_16BIT; break;
    case 24: bitWidth = I2S_DATA_BIT_WIDTH_24BIT; break;
    case 32: bitWidth = I2S_DATA_BIT_WIDTH_32BIT; break;
    default:
      Serial.println("Unsupported bits-per-sample in WAV file");
      http.end();
      return;
  }

  i2s_slot_mode_t slotMode = (info.numChannels == 1) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO;

  if (!I2S.begin(I2S_MODE_STD, info.sampleRate, bitWidth, slotMode)) {
    Serial.println("Failed to start I2S");
    http.end();
    return;
  }

  // Stream the rest of the body straight to the speaker in small chunks
  int contentLength = http.getSize(); // total body size (includes the 44-byte header), -1 if unknown
  int remaining = (contentLength > 0) ? (contentLength - 44) : -1;

  uint8_t buf[512];
  Serial.println("Streaming audio...");

  while (http.connected() && (remaining > 0 || remaining == -1)) {
    size_t avail = stream->available();
    if (avail) {
      size_t toRead = min(avail, sizeof(buf));
      int n = stream->readBytes(buf, toRead);
      if (n > 0) {
        I2S.write(buf, n);
        if (remaining > 0) remaining -= n;
      }
    } else if (!http.connected()) {
      break;
    } else {
      delay(1);
    }
  }

  Serial.println("Playback finished");
  I2S.end();
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  I2S.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);

  Serial.println("Playing speech...");
  playWavFromUrl(speechUrl);
}

void loop() {
  // Replay every 10s for easy testing. Remove once confirmed working.
  static unsigned long lastPlay = 0;
  if (millis() - lastPlay > 10000) {
    lastPlay = millis();
    Serial.println("Replaying speech...");
    playWavFromUrl(speechUrl);
  }
}