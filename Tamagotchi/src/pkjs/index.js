var messageKeys = require('message_keys');
var screenArray = new Uint8Array(74).fill(128);

var stateTest = true;
var runningXHRRequests = 0;
//var lastButtonState = "0";

const TIMEOUT_MS = 1000;
const ROM_KEY = "ROM";
const LAST_STATE_KEY = "LAST_STATE";
const APISERVER_KEY = "APISERVER";
const APIKEY_KEY = "APIKEY";
const ROMURL_KEY = "ROMURL";


// Import the Clay package
var Clay = require('@rebble/clay');
// Load our Clay configuration file
var clayConfig = require('./config');
// Initialize Clay
var clay = new Clay(clayConfig, null, {autoHandleEvents: false});

var xhrRequest = function (url, type, callback, errorCallback) {
  var xhr = new XMLHttpRequest();

  xhr.onload = function () {
    if (xhr.status >= 200 && xhr.status < 300) {
      // Success
      callback(xhr.responseText);
    } else {
      // Server responded but with an error status
      if (errorCallback) {
        errorCallback(xhr.status, xhr.responseText);
      }
    }
  };

  xhr.onerror = function () {
    // Network-level error (no response)
    if (errorCallback) {
      errorCallback('network_error', null);
    }
  };

  xhr.open(type, url);
  xhr.send();
};

function FetchROM()
{
    if(localStorage.getItem(ROMURL_KEY) === null)
    {
        Pebble.sendAppMessage({'JSMessage': "No ROM url found!"});
        return;
    }

    console.log("Fetching ROM..."); //TODO error handling timeout

    if (localStorage.getItem(ROM_KEY) === null)
    {
        console.log("Does not yet exist in local storage so fetching...");
        xhrRequest(localStorage.getItem(ROMURL_KEY), 'GET', OnReceiveRomText, OnErrorFetchingRom);
        //xhrRequest('https://pastebin.com/raw/iN0pfyr7', 'GET', OnReceiveRomText); //TODO let user input this rom
    }
    else
    {
        console.log("Has ROM stored in local storage");
        let arr = JSON.parse(localStorage.getItem(ROM_KEY));
        let buffer = new Uint8Array(arr);
        SendROM(buffer);
    }

    function OnErrorFetchingRom(error, response)
    {
        console.log("Error fetching rom: " + error + " response: " + response);
        Pebble.sendAppMessage({'JSMessage': "Error fetching ROM!"});
        return;
    }

    function OnReceiveRomText(ROMText)
    {
        console.log("Received ROM text and parsing to array...");

        let stringArray = ROMText.split(", ");
        let values = stringArray.map(s => parseInt(s, 16)); // convert to integers
        console.log("values length: " + values.length);
        if (values.length !== 6144) // exact length of P1 rom
        {
            console.log("Incorrect ROM!");
            Pebble.sendAppMessage({'JSMessage': "Incorrect ROM!"});
            return;
        }

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
            SendSaveState();
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
            setTimeout(sendNextChunk(data), 100); 
        }
        );
    }
}

function SendSaveState()
{
    // for testing on emulator
    //let saveFileDict = {"STATEpc":26,"STATEx":125,"STATEy":525,"STATEa":0,"STATEb":1,"STATEnp":0,"STATEsp":242,"STATEflags":3,"STATEtick_counter":32674418,"STATEclk_timer_timestamp":32669696,"STATEprog_timer_timestamp":32674396,"STATEprog_timer_enabled":1,"STATEprog_timer_data":3,"STATEprog_timer_rld":7,"STATEcall_depth":3,"STATEinterrupts":[0,1,0,12,0,0,0,10,0,0,0,8,7,0,0,6,0,0,0,4,0,8,0,2],"STATEmemory":[48,0,15,18,136,0,0,0,57,20,20,0,0,0,0,0,0,0,0,0,0,0,0,81,62,174,8,125,6,148,15,12,196,0,0,5,0,240,0,0,0,0,0,16,240,5,16,17,0,1,203,0,20,177,20,21,12,16,15,168,1,240,15,6,1,5,8,0,0,0,0,0,255,28,255,28,29,255,29,255,0,127,80,43,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,119,113,23,119,113,23,125,112,23,125,119,1,134,4,216,144,248,46,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,122,110,110,122,60,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,1,255,6,0,0,16,2,51,192,80,31,0,0,0,0,0,0,0,0,0,0,0,0,255,255,3,4,45,192,5,5,60,240,128,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,122,110,110,122,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,1,33,0,0,0],"STATEselected_icon":-1,"STATEshowing_attention_icon":0};
    //localStorage.setItem(LAST_STATE_KEY, JSON.stringify(saveFileDict));

    if (localStorage.getItem(LAST_STATE_KEY) !== null)
    {
        console.log("Save file found js. Sending...");
        Pebble.sendAppMessage(JSON.parse(localStorage.getItem(LAST_STATE_KEY))); //TODO handle failed to send
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
  });

// We need to implement this since we are overriding events in webviewclosed
Pebble.addEventListener('showConfiguration', 
    function(e) {
        clay.config = clayConfig;
        Pebble.openURL(clay.generateUrl());
    }
);

// Listen for when web view is closed
Pebble.addEventListener('webviewclosed',
    function(e) {
        if (e && !e.response) { return; }

        let prevRomUrl = localStorage.getItem(ROMURL_KEY);
    
        var dict = clay.getSettings(e.response);
        //console.log("dict stingified: " + JSON.stringify(dict));

        localStorage.setItem(APISERVER_KEY, dict[messageKeys.APIServerUrl]);
        localStorage.setItem(APIKEY_KEY, dict[messageKeys.APIServerKey]);
        localStorage.setItem(ROMURL_KEY, dict[messageKeys.ROMUrl]);

        if (prevRomUrl !== dict[messageKeys.ROMUrl])
        {
            console.log("ROM url changed");
            localStorage.removeItem(ROM_KEY); // remove cached ROM
            FetchROM();
        }
        //console.log("Getting values: " + localStorage.getItem("APISERVER") + " " + localStorage.getItem("APIKEY") + " " + localStorage.getItem("ROMURL"));
    }
);