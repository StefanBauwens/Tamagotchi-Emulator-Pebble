var messageKeys = require('message_keys');
var screenArray = new Uint8Array(74).fill(128);
var dictScreen = {
    'Screen': Array.from(screenArray)
};
//dictScreen.Screen.fill(128); //TODO may want to do this every time

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

function SendScreen() 
{
    stateTest = !stateTest;

    // TODO: temp fill screen
    for (var i = 0; i < 74; i++) {
        //dictScreen.Screen[i] = 128;
        screenArray[i] = 128 + i%128;
    }
    dictScreen.Screen = Array.from(screenArray);
    //dictScreen.Screen = String.fromCharCode(...screenArray);
    //console.log("screen: " + dictScreen.Screen);
    //console.log("first byte: " + dictScreen.Screen.codePointAt(0) + " second byte: " + dictScreen.Screen.codePointAt(1));

    // Send to Pebble
    Pebble.sendAppMessage(dictScreen,
        function(e) {
            console.log('Screen sent to Pebble successfully!');
        },
        function(e) {
            console.log('Error sending screen to Pebble!');
        }
    );

    console.log("Send xhr request");
    xhrRequest('http://localhost:1821/sentFromPebbleLocal', 'GET', function(responseText) { console.log("Response text: " + responseText)}); // this works to communicate with tasker HTTP Request event
    console.log("after request");
}

function SendButtonState()
{
    setTimeout(() => {
        runningXHRRequests++;
        xhrRequest('http://localhost:1821/button/' + lastButtonState, 'GET', function(responseText) { 
            console.log("Response text: " + responseText);
            runningXHRRequests--;
            if(responseText != "boop")
            {
                console.log("Expecteed boop. Resending buttons");
                SendButtonState();
            }
        }); // this works to communicate with tasker HTTP Request event
    }, 200 * runningXHRRequests); // prevents pebble app crash from having too many xhr requests
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
    function(e) {
        console.log('PebbleKit JS ready!');

        // Update s_js_ready on watch
        Pebble.sendAppMessage({'JSReady': 1});

        //setInterval(SendScreen, 500);
        SendScreen();
      
    }   
);

// Listen for appmessage
Pebble.addEventListener('appmessage', function(e) {
    var dict = e.payload;
    console.log("Got message: " + JSON.stringify(dict));

    if ('Button' in dict)
    {
        lastButtonState = dict['Button'];
        SendButtonState();
    }
  });