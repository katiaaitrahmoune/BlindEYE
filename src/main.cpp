#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP_I2S.h>

const char* ssid = "katia";
const char* password = "katia2005";

// ==== BUTTON ====
#define BUTTON 21

// ==== I2S MIC PINS ====
#define MIC_SCK  4
#define MIC_WS   5
#define MIC_SD   6

I2SClass I2Smic(I2S_NUM_1);

// ==== RECORDING CONFIG ====
#define SAMPLE_RATE       16000
#define MAX_RECORD_SEC    5
#define BYTES_PER_SAMPLE  2
#define MAX_SAMPLES       (SAMPLE_RATE * MAX_RECORD_SEC)

// ==== YOUR PC'S LOCAL SERVER ====
// Replace with your PC's actual local IP (run "ipconfig" on your PC, look for IPv4 Address)
const char* serverHost = "172.31.146.154";  // <-- current PC WiFi IP, check ipconfig if this stops working
const int   serverPort = 5000;
const char* uploadPath = "/upload/audio";

int16_t* recordBuffer = nullptr;
size_t recordedSamples = 0;
bool isRecording = false;

// ==== Build a 44-byte WAV header for mono 16-bit PCM ====
void writeWavHeader(uint8_t* h, uint32_t dataSize, uint32_t sampleRate) {
  uint32_t byteRate = sampleRate * 1 * 16 / 8;
  uint16_t blockAlign = 1 * 16 / 8;
  uint32_t chunkSize = 36 + dataSize;

  memcpy(h, "RIFF", 4);
  h[4] = chunkSize & 0xFF; h[5] = (chunkSize >> 8) & 0xFF;
  h[6] = (chunkSize >> 16) & 0xFF; h[7] = (chunkSize >> 24) & 0xFF;
  memcpy(h + 8, "WAVE", 4);
  memcpy(h + 12, "fmt ", 4);
  h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0;
  h[20] = 1; h[21] = 0;
  h[22] = 1; h[23] = 0;
  h[24] = sampleRate & 0xFF; h[25] = (sampleRate >> 8) & 0xFF;
  h[26] = (sampleRate >> 16) & 0xFF; h[27] = (sampleRate >> 24) & 0xFF;
  h[28] = byteRate & 0xFF; h[29] = (byteRate >> 8) & 0xFF;
  h[30] = (byteRate >> 16) & 0xFF; h[31] = (byteRate >> 24) & 0xFF;
  h[32] = blockAlign; h[33] = 0;
  h[34] = 16; h[35] = 0;
  memcpy(h + 36, "data", 4);
  h[40] = dataSize & 0xFF; h[41] = (dataSize >> 8) & 0xFF;
  h[42] = (dataSize >> 16) & 0xFF; h[43] = (dataSize >> 24) & 0xFF;
}

// ==== Start recording ====
void startRecording() {
  if (!recordBuffer) {
    recordBuffer = (int16_t*)malloc(MAX_SAMPLES * sizeof(int16_t));
    if (!recordBuffer) {
      Serial.println("Failed to allocate recording buffer!");
      return;
    }
  }
  recordedSamples = 0;

  I2Smic.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);
  if (!I2Smic.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO)) {
    Serial.println("Failed to start I2S mic");
    return;
  }

  isRecording = true;
  Serial.println("Recording started... speak into the mic now.");
}

// ==== Stream the recording to the local Python server ====
void uploadRecording() {
  if (recordedSamples == 0) {
    Serial.println("Nothing recorded, skipping upload.");
    return;
  }

  uint32_t dataSize = recordedSamples * BYTES_PER_SAMPLE;
  uint8_t wavHeader[44];
  writeWavHeader(wavHeader, dataSize, SAMPLE_RATE);

  String boundary = "BlindEyeAudioBoundary";
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"audio\"; filename=\"rec.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t bodyLen = head.length() + 44 + dataSize + tail.length();

  Serial.printf("Connecting to %s:%d...\n", serverHost, serverPort);
  WiFiClient client; // plain HTTP, no TLS needed for local testing

  if (!client.connect(serverHost, serverPort)) {
    Serial.println("Connection to server failed -- check IP/port and that server.py is running");
    return;
  }

  client.print(String("POST ") + uploadPath + " HTTP/1.1\r\n");
  client.print(String("Host: ") + serverHost + "\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(bodyLen) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  client.print(head);
  client.write(wavHeader, 44);

  const size_t chunkSize = 1024;
  uint8_t* raw = (uint8_t*)recordBuffer;
  uint32_t sent = 0;
  while (sent < dataSize) {
    size_t toSend = min((uint32_t)chunkSize, dataSize - sent);
    client.write(raw + sent, toSend);
    sent += toSend;
  }

  client.print(tail);

  Serial.println("Upload sent, waiting for response...");

  unsigned long timeout = millis();
  while (client.connected() && !client.available()) {
    if (millis() - timeout > 10000) {
      Serial.println("Response timeout");
      client.stop();
      return;
    }
    delay(10);
  }

  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }

  client.stop();
  Serial.println("Upload complete -- check your PC, it should be playing now.");
}

// ==== Stop recording, then upload ====
void stopRecordingAndUpload() {
  isRecording = false;
  I2Smic.end();
  Serial.printf("Recording stopped. %u samples captured (%.2f sec)\n",
                (unsigned)recordedSamples, (float)recordedSamples / SAMPLE_RATE);

  uploadRecording();
}

// ==== Called continuously while recording, pulls samples from I2S ====
void recordChunk() {
  const int chunkFrames = 256;
  int32_t raw[chunkFrames];

  size_t bytesRead = I2Smic.readBytes((char*)raw, chunkFrames * sizeof(int32_t));
  int framesRead = bytesRead / sizeof(int32_t);

  for (int i = 0; i < framesRead; i++) {
    if (recordedSamples >= MAX_SAMPLES) {
      Serial.println("Max recording length reached, auto-stopping.");
      stopRecordingAndUpload();
      return;
    }
    int16_t sample = (int16_t)(raw[i] >> 16);
    recordBuffer[recordedSamples++] = sample;
  }
}

// ==== Button handling (toggle start/stop) ====
bool lastButtonState = HIGH;

void handleButton() {
  bool current = digitalRead(BUTTON);
  if (lastButtonState == HIGH && current == LOW) {
    if (!isRecording) {
      startRecording();
    } else {
      stopRecordingAndUpload();
    }
    delay(300); // simple debounce
  }
  lastButtonState = current;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUTTON, INPUT_PULLUP);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  Serial.println("Ready. Press button to start/stop recording.");
}

void loop() {
  handleButton();
  if (isRecording) {
    recordChunk();
  }
}