var messageKeys = require('message_keys');
var screenArray = new Uint8Array(74).fill(128);

var stateTest = true;
var runningXHRRequests = 0;
var lastButtonState = "0";

const TIMEOUT_MS = 1000;

var xhrRequest = function (url, type, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
      callback(this.responseText);
    };
    xhr.open(type, url);
    xhr.send();
};

function FetchROM()
{
    console.log("Fetching ROM"); //TODO error handling
    xhrRequest('https://pastebin.com/raw/iN0pfyr7', 'GET', SendROM); //TODO let user input this rom
}

function SendROM(ROMText) {
    //console.log("ROM we received: " + ROMText);
    let stringArray = ROMText.split(", ");
    let values = stringArray.map(s => parseInt(s, 16)); // convert to integers

    let buffer = new Uint8Array(values.length * 2); // use 2 bytes for each value
    for (let i = 0; i < values.length; i++) {
        buffer[i * 2]     = values[i] & 0xFF;
        buffer[i * 2 + 1] = (values[i] >> 8) & 0xFF;
    }

    // send chunked to watch
    const CHUNK_SIZE = 128; //TODO test
    let offset = 0;
    sendNextChunk(buffer);

    function sendNextChunk(data) {
        console.log("Trying to send ROM...");
        if (offset >= data.length) 
        {
            console.log("Finished sending ROM!");
            return;
        }

        let chunk = data.slice(offset, offset + CHUNK_SIZE);

        Pebble.sendAppMessage({
            'ROMOffset': offset,
            'ROMChunk': Array.from(chunk)
        },
        function() { // on success send next chunk
            console.log("Chunk sent! Progress: " + (offset/data.length));
            offset += CHUNK_SIZE;
            sendNextChunk(data);
        },
        function() { // on fail
            console.log("Failed to send chunk! Retrying...");
            setTimeout(sendNextChunk(data), 100); //TODO test
        }
        );
    }
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
    function(e) {
        console.log('PebbleKit JS ready!');

        // Update s_js_ready on watch
        Pebble.sendAppMessage({'JSReady': 1});

        //setInterval(SendScreen, 500);
        //SendScreen();
        setTimeout(FetchROM, 3000); //TODO test
    }   
);

// Listen for appmessage
Pebble.addEventListener('appmessage', function(e) {
    var dict = e.payload;
    console.log("Got message: " + JSON.stringify(dict));

    /*if ('Button' in dict)
    {
        lastButtonState = dict['Button'];
        SendButtonState();
    }*/
  });