#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESP_I2S.h>

const char* ssid     = "katia";
const char* password = "katia2005";

// ==== NGROK (host only, no https://) ====
const char* serverHost = "spry-envelope-angular.ngrok-free.dev"; 
const char* sessionId  = "liveSession1";

#define BUTTON   21

// Mic (INMP441)
#define MIC_SCK  4
#define MIC_WS   5
#define MIC_SD   6

// Speaker (MAX98357A)
#define SPK_BCLK 15
#define SPK_LRC  16
#define SPK_DOUT 17

I2SClass I2S;                 // speaker
I2SClass I2Smic(I2S_NUM_1);   // mic

#define SAMPLE_RATE      16000
#define MAX_RECORD_SEC   5
#define BYTES_PER_SAMPLE 2
#define MAX_SAMPLES      (SAMPLE_RATE * MAX_RECORD_SEC)

int16_t* recordBuffer = nullptr;
size_t recordedSamples = 0;
bool isRecording = false;

void writeWavHeader(uint8_t* h, uint32_t dataSize, uint32_t sampleRate) {
  uint32_t byteRate = sampleRate * 2;
  uint32_t chunkSize = 36 + dataSize;
  memcpy(h, "RIFF", 4);
  h[4] = chunkSize; h[5] = chunkSize >> 8; h[6] = chunkSize >> 16; h[7] = chunkSize >> 24;
  memcpy(h + 8, "WAVE", 4);
  memcpy(h + 12, "fmt ", 4);
  h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0;
  h[20] = 1; h[21] = 0;
  h[22] = 1; h[23] = 0;
  h[24] = sampleRate; h[25] = sampleRate >> 8; h[26] = sampleRate >> 16; h[27] = sampleRate >> 24;
  h[28] = byteRate; h[29] = byteRate >> 8; h[30] = byteRate >> 16; h[31] = byteRate >> 24;
  h[32] = 2; h[33] = 0;
  h[34] = 16; h[35] = 0;
  memcpy(h + 36, "data", 4);
  h[40] = dataSize; h[41] = dataSize >> 8; h[42] = dataSize >> 16; h[43] = dataSize >> 24;
}

String extractUrl(const String& body) {
  int k = body.indexOf("\"url\"");
  if (k < 0) return "";
  int colon = body.indexOf(':', k);
  if (colon < 0) return "";
  int q1 = body.indexOf('"', colon + 1);
  if (q1 < 0) return "";
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return "";
  return body.substring(q1 + 1, q2);
}

String uploadRecording() {
  if (recordedSamples == 0) { Serial.println("Nothing recorded"); return ""; }

  uint32_t dataSize = recordedSamples * BYTES_PER_SAMPLE;
  uint8_t wavHeader[44];
  writeWavHeader(wavHeader, dataSize, SAMPLE_RATE);

  String boundary = "BlindEyeAudioBoundary";
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"audio\"; filename=\"rec.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";
  uint32_t bodyLen = head.length() + 44 + dataSize + tail.length();

  WiFiClientSecure client;
  client.setInsecure();
  Serial.printf("Free heap before connect: %u\n", ESP.getFreeHeap());

  if (!client.connect(serverHost, 443)) {
    Serial.println("Connection to server failed");
    return "";
  }

  client.print(String("POST /upload/audio?session_id=") + sessionId + " HTTP/1.1\r\n");
  client.print(String("Host: ") + serverHost + "\r\n");
  client.print("ngrok-skip-browser-warning: 1\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(bodyLen) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  client.print(head);
  client.write(wavHeader, 44);

  uint8_t* raw = (uint8_t*)recordBuffer;
  uint32_t sent = 0;
  while (sent < dataSize) {
    size_t n = min((uint32_t)1024, dataSize - sent);
    client.write(raw + sent, n);
    sent += n;
  }
  client.print(tail);

  Serial.println("Upload sent, waiting for server (Whisper+Gemini+TTS)...");

  unsigned long t0 = millis();
  while (client.connected() && !client.available()) {
    if (millis() - t0 > 75000) {
      Serial.println("Response timeout");
      client.stop();
      return "";
    }
    delay(50);
  }

  String resp;
  unsigned long last = millis();
  while ((client.connected() || client.available()) && millis() - last < 3000) {
    while (client.available()) { resp += (char)client.read(); last = millis(); }
    delay(5);
  }
  client.stop();

  String url = extractUrl(resp);
  if (url.length() == 0) Serial.println(resp);   // show server error for debugging
  else Serial.println(resp.substring(0, resp.indexOf('\n')));
  return url;
}

struct WavInfo { uint16_t numChannels; uint32_t sampleRate; uint16_t bitsPerSample; };

bool parseWavHeader(const uint8_t* h, WavInfo& info) {
  if (memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0) {
    Serial.println("Not a valid WAV");
    return false;
  }
  info.numChannels   = h[22] | (h[23] << 8);
  info.sampleRate    = h[24] | (h[25] << 8) | (h[26] << 16) | ((uint32_t)h[27] << 24);
  info.bitsPerSample = h[34] | (h[35] << 8);
  return true;
}

void playWavFromUrl(const char* url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  int code = http.GET();
  Serial.printf("Audio GET: %d\n", code);
  if (code != HTTP_CODE_OK) { http.end(); return; }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t header[44];
  size_t got = 0;
  unsigned long w = millis();
  while (got < 44) {
    if (stream->available()) got += stream->readBytes(header + got, 44 - got);
    else if (millis() - w > 10000) { Serial.println("Header timeout"); http.end(); return; }
  }

  WavInfo info;
  if (!parseWavHeader(header, info)) { http.end(); return; }

  i2s_data_bit_width_t bw;
  switch (info.bitsPerSample) {
    case 8:  bw = I2S_DATA_BIT_WIDTH_8BIT;  break;
    case 16: bw = I2S_DATA_BIT_WIDTH_16BIT; break;
    case 24: bw = I2S_DATA_BIT_WIDTH_24BIT; break;
    case 32: bw = I2S_DATA_BIT_WIDTH_32BIT; break;
    default: http.end(); return;
  }
  i2s_slot_mode_t slot = (info.numChannels == 1) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO;

  I2S.setPins(SPK_BCLK, SPK_LRC, SPK_DOUT);
  if (!I2S.begin(I2S_MODE_STD, info.sampleRate, bw, slot)) {
    Serial.println("I2S speaker start failed");
    http.end();
    return;
  }

  int len = http.getSize();
  int remaining = (len > 0) ? (len - 44) : -1;
  uint8_t buf[512];
  Serial.println("Playing...");
  while (http.connected() && (remaining > 0 || remaining == -1)) {
    size_t avail = stream->available();
    if (avail) {
      int n = stream->readBytes(buf, min(avail, sizeof(buf)));
      if (n > 0) { I2S.write(buf, n); if (remaining > 0) remaining -= n; }
    } else delay(1);
  }
  Serial.println("Playback finished");
  I2S.end();
  http.end();
}

void startRecording() {
  if (!recordBuffer) {
    size_t bytes = MAX_SAMPLES * sizeof(int16_t);
    recordBuffer = psramFound() ? (int16_t*)ps_malloc(bytes) : (int16_t*)malloc(bytes);
    if (!recordBuffer) { Serial.println("Buffer alloc failed"); return; }
  }
  recordedSamples = 0;
  I2Smic.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);
  if (!I2Smic.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("Mic start failed");
    return;
  }
  isRecording = true;
  Serial.println("Recording... speak now (press again to stop)");
}

void stopRecordingAndRun() {
  isRecording = false;
  I2Smic.end();
  Serial.printf("Captured %.2f sec\n", (float)recordedSamples / SAMPLE_RATE);

  String url = uploadRecording();

  free(recordBuffer);
  recordBuffer = nullptr;

  if (url.length()) {
    Serial.println("Audio URL: " + url);
    playWavFromUrl(url.c_str());
  } else {
    Serial.println("No audio URL received");
  }
}

void recordChunk() {
  const int frames = 256;
  int32_t raw[frames];
  size_t bytesRead = I2Smic.readBytes((char*)raw, frames * sizeof(int32_t));
  int n = bytesRead / sizeof(int32_t);
  for (int i = 0; i < n; i++) {
    if (recordedSamples >= MAX_SAMPLES) {
      Serial.println("Max length reached");
      stopRecordingAndRun();
      return;
    }
    recordBuffer[recordedSamples++] = (int16_t)(raw[i] >> 16);
  }
}

bool lastButtonState = HIGH;
void handleButton() {
  bool cur = digitalRead(BUTTON);
  if (lastButtonState == HIGH && cur == LOW) {
    if (!isRecording) startRecording();
    else stopRecordingAndRun();
    delay(300);
    cur = digitalRead(BUTTON);
  }
  lastButtonState = cur;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(BUTTON, INPUT_PULLUP);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  Serial.println("Ready. Press button to record.");
}

void loop() {
  handleButton();
  if (isRecording) recordChunk();
}