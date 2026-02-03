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
textToWav('hi !this is blind eye ,welcome here im excited to help you ', 'en', 'hello.wav')
    .then(filename => {
        console.log(`Successfully saved: ${filename}`);
    })
    .catch(err => console.error(err));