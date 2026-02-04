const fs = require('fs');
const https = require('https');
const googleTTS = require('google-tts-api');

async function main() {
    try {
        const text = 'hi this is blind eye';

        
        const url = googleTTS.getAudioUrl(text, {
            lang: 'en',
            slow: false,
            host: 'https://translate.google.com',
        });

        
        const file = fs.createWriteStream('output.wav');
        https.get(url, (res) => {
            res.pipe(file);
            file.on('finish', () => {
                file.close();
                console.log(' Audio saved as output.mp3');
            });
        });

    } catch (err) {
        console.error(' TTS error:', err);
    }
}

main();
