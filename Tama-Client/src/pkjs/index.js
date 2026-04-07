var messageKeys = require('message_keys');
var screenArray = new Uint8Array(74).fill(128);

var stateTest = true;
var runningXHRRequests = 0;
//var lastButtonState = "0";

const TIMEOUT_MS = 1000;
const ROM_KEY = "ROM";
const LAST_STATE_KEY = "LAST_STATE";

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
    console.log("Fetching ROM..."); //TODO error handling timeout
    if (localStorage.getItem(ROM_KEY) === null)
    {
        console.log("Does not yet exist in local storage so fetching...");
        xhrRequest('https://pastebin.com/raw/iN0pfyr7', 'GET', OnReceiveRomText); //TODO let user input this rom
    }
    else
    {
        console.log("Has ROM stored in local storage");
        let arr = JSON.parse(localStorage.getItem(ROM_KEY));
        let buffer = new Uint8Array(arr);
        SendROM(buffer);
    }

    function OnReceiveRomText(ROMText)
    {
        console.log("Received ROM text and parsing to array...");

        let stringArray = ROMText.split(", ");
        let values = stringArray.map(s => parseInt(s, 16)); // convert to integers

        let buffer = new Uint8Array(values.length * 2); // use 2 bytes for each value
        for (let i = 0; i < values.length; i++) {
            buffer[i * 2]     = values[i] & 0xFF;
            buffer[i * 2 + 1] = (values[i] >> 8) & 0xFF;
        }

        localStorage.setItem(ROM_KEY, JSON.stringify(Array.from(buffer)));
        console.log("Saved to localstorage");

        SendROM(buffer);
    }
}

function SendROM(buffer) {
    console.log("Trying to send ROM...");

    // send chunked to watch
    const CHUNK_SIZE = 2048; //TODO test
    let offset = 0;
    sendNextChunk(buffer);

    function sendNextChunk(data) {
        if (offset >= data.length) 
        {
            console.log("Finished sending ROM!");
            SendSaveState(); //TODO test
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

function SendSaveState()
{
    if (localStorage.getItem(LAST_STATE_KEY) !== null)
    {
        console.log("Save file found js. Sending...");
        Pebble.sendAppMessage(JSON.parse(localStorage.getItem(LAST_STATE_KEY)));
        console.log("Save file sent!");
    }
    else
    {
        console.log("No save file!");
        Pebble.sendAppMessage({'STATEnone': 1});
    }
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
    function(e) {
        console.log('PebbleKit JS ready!');

        // Update s_js_ready on watch
        Pebble.sendAppMessage({'JSReady': 1});

        //localStorage.clear(); //TODO temp
        //setTimeout(FetchROM, 3000); //TODO test
        FetchROM();
    }   
);

// Listen for appmessage
Pebble.addEventListener('appmessage', function(e) {
    var dict = e.payload;
    console.log("Got message: " + JSON.stringify(dict));

    if ('STATEpc' in dict)
    {
        localStorage.setItem(LAST_STATE_KEY, JSON.stringify(dict));
        console.log("Saved last state to js localstorage...");
    }
    /*if ('Button' in dict)
    {
        lastButtonState = dict['Button'];
        SendButtonState();
    }*/
  });