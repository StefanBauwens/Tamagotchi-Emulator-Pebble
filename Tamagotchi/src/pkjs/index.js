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

var xhrRequest = function (url, type, data, callback, errorCallback) {
  var xhr = new XMLHttpRequest();

  xhr.onload = function () {
    if (xhr.status >= 200 && xhr.status < 300) {
      callback(xhr.responseText);
    } else {
      if (errorCallback) {
        errorCallback(xhr.status, xhr.responseText);
      }
    }
  };

  xhr.onerror = function () {
    if (errorCallback) {
      errorCallback('network_error', null);
    }
  };

  xhr.open(type, url);

  // Set JSON header if sending data
  if (data) {
    xhr.setRequestHeader('Content-Type', 'application/json;charset=UTF-8');
    xhr.send(JSON.stringify(data));
  } else {
    xhr.send();
  }
};

function FetchROM()
{
    if(localStorage.getItem(ROMURL_KEY) === null)
    {
        Pebble.sendAppMessage({'JSMessage': "No ROM url found!"});
        return;
    }

    console.log("Fetching ROM...");

    if (localStorage.getItem(ROM_KEY) === null)
    {
        console.log("Does not yet exist in local storage so fetching...");
        xhrRequest(localStorage.getItem(ROMURL_KEY), 'GET', null, OnReceiveRomText, OnErrorFetchingRom);
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
            SendSaveStateToWatch();
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

function SendSaveStateToWatch() // Send last save state back to watch
{
    if(localStorage.getItem(APISERVER_KEY) !== null)
    {
        xhrRequest(localStorage.getItem(APISERVER_KEY) + "/state", 'GET', null, 
        (responseText) => { // success
            console.log("Successfully fetched save state from server: " + responseText);
            //TODO parse it
            SendSaveFromLocalStorage(); //TODO TODO temp temp

        }, 
        (error, response) => { // fail
            console.log("Failed to fetch from server. Using last save as backup. Error: " + error);
            SendSaveFromLocalStorage();
        });
    }
    else
    {
       SendSaveFromLocalStorage();
    }
}

 function SendSaveFromLocalStorage()
 {
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
        SaveStateAfterClosingApp(dict);
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

function SaveStateAfterClosingApp(saveStateDict)
{
    localStorage.setItem(LAST_STATE_KEY, JSON.stringify(saveStateDict));
    console.log("Saved last state to js localstorage...");

    if(localStorage.getItem(APISERVER_KEY) !== null)
    {
        console.log("Sending save to server as well..."); //TODO
        let payload = {
            'pc': saveStateDict.STATEpc,
            'x': saveStateDict.STATEx,
            'y': saveStateDict.STATEy,
            'a': saveStateDict.STATEa,
            'b': saveStateDict.STATEb,
            'np': saveStateDict.STATEnp,
            'sp': saveStateDict.STATEsp,
            'flags': saveStateDict.STATEflags,
            'tick_counter': saveStateDict.STATEtick_counter,
            'clk_timer_timestamp': saveStateDict.STATEclk_timer_timestamp,
            'prog_timer_timestamp': saveStateDict.STATEprog_timer_timestamp,
            'prog_timer_enabled': saveStateDict.STATEprog_timer_enabled,
            'prog_timer_data': saveStateDict.STATEprog_timer_data,
            'prog_timer_rld': saveStateDict.STATEprog_timer_rld,
            'call_depth': saveStateDict.STATEcall_depth,
            'interrupts': [
                {"factor_flag_reg":saveStateDict.STATEinterrupts[0],"mask_reg":saveStateDict.STATEinterrupts[1],"triggered":saveStateDict.STATEinterrupts[2],"vector":saveStateDict.STATEinterrupts[3]},
                {"factor_flag_reg":saveStateDict.STATEinterrupts[4],"mask_reg":saveStateDict.STATEinterrupts[5],"triggered":saveStateDict.STATEinterrupts[6],"vector":saveStateDict.STATEinterrupts[7]},
                {"factor_flag_reg":saveStateDict.STATEinterrupts[8],"mask_reg":saveStateDict.STATEinterrupts[9],"triggered":saveStateDict.STATEinterrupts[10],"vector":saveStateDict.STATEinterrupts[11]},
                {"factor_flag_reg":saveStateDict.STATEinterrupts[12],"mask_reg":saveStateDict.STATEinterrupts[13],"triggered":saveStateDict.STATEinterrupts[14],"vector":saveStateDict.STATEinterrupts[15]},
                {"factor_flag_reg":saveStateDict.STATEinterrupts[16],"mask_reg":saveStateDict.STATEinterrupts[17],"triggered":saveStateDict.STATEinterrupts[18],"vector":saveStateDict.STATEinterrupts[19]},
                {"factor_flag_reg":saveStateDict.STATEinterrupts[20],"mask_reg":saveStateDict.STATEinterrupts[21],"triggered":saveStateDict.STATEinterrupts[22],"vector":saveStateDict.STATEinterrupts[23]}
            ],
            'memory': saveStateDict.STATEmemory
        };

        xhrRequest(localStorage.getItem(APISERVER_KEY) + "/state", 'POST', payload,
        (responseText) => { // success
            console.log("Successfully sent save state to server: " + responseText);
        }, 
        (error, response) => { // fail
            console.log("Failed to send data to server. Error: " + error);
        });
    }
}