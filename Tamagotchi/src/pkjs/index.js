var messageKeys = require('message_keys');

const ROM_KEY = "ROM";
const LAST_STATE_KEY = "LAST_STATE";
const APISERVER_KEY = "APISERVER";
const APIKEY_KEY = "APIKEY";
const ROMURL_KEY = "ROMURL";
const SERVER_SAVE_FAILED_KEY = "SERVER_SAVE_FAILED";


// Import the Clay package
var Clay = require('@rebble/clay');
// Load our Clay configuration file
var clayConfig = require('./config');
// Initialize Clay
var clay = new Clay(clayConfig, null, {autoHandleEvents: false});

var xhrRequest = function (url, type, data, callback, errorCallback, timeout = 10000) {
  var xhr = new XMLHttpRequest();

  xhr.timeout = timeout; // in milliseconds (10s default)

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

  xhr.ontimeout = function () {
    if (errorCallback) {
      errorCallback('timeout', null);
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
    if(localStorage.getItem(ROMURL_KEY) === null || localStorage.getItem(ROMURL_KEY).trim().length === 0)
    {
        Pebble.sendAppMessage({'JSMessage': "No ROM url found! Check app settings!"});
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
            setTimeout(() => { sendNextChunk(data) }, 100); 
        }
        );
    }
}

function SendSaveStateToWatch() // Send last save state back to watch
{
    let serverSaveFailed = false;
    if (localStorage.getItem(SERVER_SAVE_FAILED_KEY) !== null)
    {
        serverSaveFailed = localStorage.getItem(SERVER_SAVE_FAILED_KEY);
        localStorage.setItem(SERVER_SAVE_FAILED_KEY, false); 
    }

    if(localStorage.getItem(APISERVER_KEY) !== null && localStorage.getItem(APISERVER_KEY).trim().length !== 0)
    {
        if (serverSaveFailed)
        {
            Pebble.sendAppMessage({'JSMessage': "Last server save failed so restoring from local storage..."});
            setTimeout(SendSaveFromLocalStorage, 2000); // add delay so we see error message
            return;
        }

        Pebble.sendAppMessage({'JSMessage': "Trying to sync with server..."});
        xhrRequest(localStorage.getItem(APISERVER_KEY) + "/state", 'GET', null, 
        (responseText) => { // success
            console.log("Successfully fetched save state from server: " + responseText);

            // parse it
            let serverState = JSON.parse(responseText);

            if(serverState.memory[0] === null)
            {
                Pebble.sendAppMessage({'JSMessage': "Empty state received. Restoring from local storage..."});
                setTimeout(SendSaveFromLocalStorage, 2000); // add delay so we see error message
                return;
            }

            let parsedDict = {
                'STATEpc': serverState.pc,
                'STATEx': serverState.x,
                'STATEy': serverState.y,
                'STATEa': serverState.a,
                'STATEb': serverState.b,
                'STATEnp': serverState.np,
                'STATEsp': serverState.sp,
                'STATEflags': serverState.flags,
                'STATEtick_counter': serverState.tick_counter,
                'STATEclk_timer_timestamp': serverState.clk_timer_timestamp,
                'STATEprog_timer_timestamp': serverState.prog_timer_timestamp,
                'STATEprog_timer_enabled': serverState.prog_timer_enabled,
                'STATEprog_timer_data': serverState.prog_timer_data,
                'STATEprog_timer_rld': serverState.prog_timer_rld,
                'STATEcall_depth': serverState.call_depth,
                'STATEinterrupts': [
                    serverState.interrupts[0].factor_flag_reg, serverState.interrupts[0].mask_reg, serverState.interrupts[0].triggered, serverState.interrupts[0].vector,
                    serverState.interrupts[1].factor_flag_reg, serverState.interrupts[1].mask_reg, serverState.interrupts[1].triggered, serverState.interrupts[1].vector,
                    serverState.interrupts[2].factor_flag_reg, serverState.interrupts[2].mask_reg, serverState.interrupts[2].triggered, serverState.interrupts[2].vector,
                    serverState.interrupts[3].factor_flag_reg, serverState.interrupts[3].mask_reg, serverState.interrupts[3].triggered, serverState.interrupts[3].vector,
                    serverState.interrupts[4].factor_flag_reg, serverState.interrupts[4].mask_reg, serverState.interrupts[4].triggered, serverState.interrupts[4].vector,
                    serverState.interrupts[5].factor_flag_reg, serverState.interrupts[5].mask_reg, serverState.interrupts[5].triggered, serverState.interrupts[5].vector,
                ],
                'STATEmemory': serverState.memory,
                'STATEselected_icon': serverState.selected_icon,
                'STATEshowing_attention_icon': serverState.showing_attention_icon
            };

            console.log("Sending save file from server to watch....");
            SendDictRetrying(parsedDict);

        }, 
        (error, response) => { // fail
            console.log("Failed to fetch from server. Using last save as backup. Error: " + error);
            Pebble.sendAppMessage({'JSMessage': "Sync failed! Restoring from local storage..."});
            setTimeout(SendSaveFromLocalStorage, 2000); // add delay so we see error message
        }, 5000);
    }
    else
    {
       SendSaveFromLocalStorage();
    }
}

function SendDictRetrying(dict)
{
    Pebble.sendAppMessage(dict, 
    () => { console.log("Success"), 
    () => { // on fail
        console.log("Retrying...");
        setTimeout(() => {
            SendDictRetrying(dict);
        }, 100); 
    }}); 
}

 function SendSaveFromLocalStorage()
 {
    if (localStorage.getItem(LAST_STATE_KEY) !== null)
    {
        console.log("Save file found js. Sending...");
        SendDictRetrying(JSON.parse(localStorage.getItem(LAST_STATE_KEY)));
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

        localStorage.setItem(APISERVER_KEY, dict[messageKeys.APIServerUrl]);
        localStorage.setItem(APIKEY_KEY, dict[messageKeys.APIServerKey]);
        localStorage.setItem(ROMURL_KEY, dict[messageKeys.ROMUrl]);

        if (prevRomUrl !== dict[messageKeys.ROMUrl])
        {
            console.log("ROM url changed");
            localStorage.removeItem(ROM_KEY); // remove cached ROM
            FetchROM();
        }

        if(dict[messageKeys.reset_tamagotchi] == true)
        {
            clay.setSettings("reset_tamagotchi", false);
            console.log("Reset tamagotchi requested"); 
            localStorage.removeItem(LAST_STATE_KEY); // delete save file
            Pebble.sendAppMessage({'reset_tamagotchi': 1}); // tell watch to reset
        }
    }
);

function SaveStateAfterClosingApp(saveStateDict)
{
    //TODO temp
    //saveStateDict = {"STATEpc":26,"STATEx":125,"STATEy":525,"STATEa":0,"STATEb":1,"STATEnp":0,"STATEsp":242,"STATEflags":3,"STATEtick_counter":32674418,"STATEclk_timer_timestamp":32669696,"STATEprog_timer_timestamp":32674396,"STATEprog_timer_enabled":1,"STATEprog_timer_data":3,"STATEprog_timer_rld":7,"STATEcall_depth":3,"STATEinterrupts":[0,1,0,12,0,0,0,10,0,0,0,8,7,0,0,6,0,0,0,4,0,8,0,2],"STATEmemory":[48,0,15,18,136,0,0,0,57,20,20,0,0,0,0,0,0,0,0,0,0,0,0,81,62,174,8,125,6,148,15,12,196,0,0,5,0,240,0,0,0,0,0,16,240,5,16,17,0,1,203,0,20,177,20,21,12,16,15,168,1,240,15,6,1,5,8,0,0,0,0,0,255,28,255,28,29,255,29,255,0,127,80,43,63,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,119,113,23,119,113,23,125,112,23,125,119,1,134,4,216,144,248,46,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,122,110,110,122,60,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,1,255,6,0,0,16,2,51,192,80,31,0,0,0,0,0,0,0,0,0,0,0,0,255,255,3,4,45,192,5,5,60,240,128,17,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,122,110,110,122,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,1,33,0,0,0],"STATEselected_icon":-1,"STATEshowing_attention_icon":0};

    if (saveStateDict.STATEpc === 0 || saveStateDict.STATEmemory[0] === null) return; // don't save bad saves and overwrite
    
    localStorage.setItem(LAST_STATE_KEY, JSON.stringify(saveStateDict));
    console.log("Saved last state to js localstorage...");

    if(localStorage.getItem(APISERVER_KEY) !== null)
    {
        console.log("Sending save to server as well...");
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
            'memory': saveStateDict.STATEmemory,
            'selected_icon': saveStateDict.STATEselected_icon,
            'showing_attention_icon': saveStateDict.STATEshowing_attention_icon
        };

        xhrRequest(localStorage.getItem(APISERVER_KEY) + "/state", 'POST', payload,
        (responseText) => { // success
            console.log("Successfully sent save state to server: " + responseText);
            Pebble.sendAppMessage({'JSMessage': "Saved to server!", 'JSFinishedSaving': 1}); // tell watch to finish quitting
        }, 
        (error, response) => { // fail
            localStorage.setItem(SERVER_SAVE_FAILED_KEY, true); // keep track that this failed!
            console.log("Failed to send data to server. Error: " + error + "Response: " + response);
            Pebble.sendAppMessage({'JSMessage': "Saving to server failed!", 'JSFinishedSaving': 1}); // tell watch to finish quitting //TODO retry?
        });
    }
    else
    {
        Pebble.sendAppMessage({'JSMessage': "Saved!", 'JSFinishedSaving': 1}); // tell watch we saved and it can quit now
    }
}