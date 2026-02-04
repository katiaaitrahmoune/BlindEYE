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



const gtts = require('gtts');
const ffmpeg = require('fluent-ffmpeg');
const ffmpegInstaller = require('@ffmpeg-installer/ffmpeg');
const fs = require('fs');
ffmpeg.setFfmpegPath(ffmpegInstaller.path);

async function textToWav(text, language = 'en', outputFile = 'output.wav') {
    return new Promise((resolve, reject) => {
        try {
            const tempMp3 = `temp_${Date.now()}.mp3`;
            const gttsObj = new gtts(text, language);
            
        
            gttsObj.save(tempMp3, (err) => {
                if (err) {
                    reject(err);
                    return;
                }
                
                console.log('MP3 created, converting to WAV...');
                
              
                ffmpeg(tempMp3)
                    .toFormat('wav')
                    .audioChannels(1)  
                    .audioFrequency(16000)  
                    .on('end', () => {
                        console.log(`WAV file saved as: ${outputFile}`);
                        
                  
                        fs.unlinkSync(tempMp3);
                        resolve(outputFile);
                    })
                    .on('error', (err) => {
                        console.error('Conversion error:', err);
                        fs.unlinkSync(tempMp3);  
                        reject(err);
                    })
                    .save(outputFile);
            });
        } catch (error) {
            reject(error);
        }
    });
}
textToWav('hi blind eye i want you to help me  ', 'en', 'hello.wav')
    .then(filename => {
        console.log(`Successfully saved: ${filename}`);
    })
    .catch(err => console.error(err));