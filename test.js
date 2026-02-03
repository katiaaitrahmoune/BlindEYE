const say = require('say');
const path = require('path');
const OUTPUT_FILE = path.join(__dirname, 'windows_voice.wav');
const text = "hi how are you ? its been a while we didnt met how are you doing whats the new !!!";
const voice = "zira";
const speed = 1.0;
say.export(text, voice, speed, OUTPUT_FILE, (err) => {
  if (err) {
    return console.error("Error generating WAV file:", err);
  }
  console.log(`Audio saved successfully to ${OUTPUT_FILE}`);
});
