# Tamagotchi Emulator 4 Pebble
Tamagotchi P1 emulator for the Pebble watch, created for the Spring 2026 Pebble Contest.
Powered by [TamaLib](https://github.com/jcrona/tamalib/).

![Tamagotchi watchapp screenshot basalt](Tamagotchi/screenshots/basalt.png)
![Tamagotchi watchapp screenshot basalt](Tamagotchi/screenshots/basalt1.png)
![Tamagotchi watchapp screenshot monochrome](Tamagotchi/screenshots/Duo2.png)
![Tamagotchi watchapp screenshot monochrome](Tamagotchi/screenshots/Duo3.png)

![Tamagotchi watchapp screenshot chalk](Tamagotchi/screenshots/chalk.png)
![Tamagotchi watchapp screenshot emery](Tamagotchi/screenshots/emery.png)
![Tamagotchi watchapp screenshot gabbro](Tamagotchi/screenshots/gabbro1.png)

## Features & Updates:
v1.0:
- Tamagotchi P1 Emulation
- Support for external ROM integration (via Settings page)
- State saving & loading on closing/opening watchapp
- Support for [Tamagotchi API Server](https://github.com/StefanBauwens/Tamagotchi-API) 
- Support for Time (Steel), Time Round, Pebble 2 (Duo), Time 2 and Round 2.

## Getting a ROM url
To run this Emulator it will need a Tamagotchi P1 rom in u12_t form in text format. I am not allowed to distribute this with the app, but it is possible for you to add a link to this in the app settings. Luckily for you it seems a link like that already has been created: https://pastebin.com/raw/iN0pfyr7
Thank you to the kind person for creating this!

## Server for running in background (optional)
By default the Tamagotchi will save its state when quitting the app and restore it the next time you use it. 

It works fine like this but if you would like your tamagotchi to continue working in the backround you can make use of my [Tamagotchi API Service](https://github.com/StefanBauwens/Tamagotchi-API) and run it on your own server. 

Once set up, when you close the app your save state is sent to your server and continues to live on there. When you open the watch app next time, the save state is fetched from the server and runs on the Pebble again. 

## How to use
You can look up the original instructions for the Tamagotchi P1 toy. 
A, B and C buttons are mapped to UP, SELECT and DOWN buttons respectively. Pressing the BACK button saves the state and quits the app.

## Issues
I've created this with support for most Pebble watches, but have only tested this on a Pebble 2 Duo so far. The emulator on CloudPebble seemed to work fine and so I hope it's also the case with the actual watches. If any major bugs are noticed feel free to create an issue here on Github!

## Credits
This project implements [TamaLib](https://github.com/jcrona/tamalib/) by [jcrona](https://github.com/jcrona). I also want to point out that jcrona has created [PebbleGotchi](https://github.com/jcrona/pebblegotchi) about 5 years ago. 

I believe my implementation to be significantly different to warrant my own publication in the store. In fact PebbleGotchi was never uploaded to the Pebble store as it required the end user to supply it with a ROM and recompile.