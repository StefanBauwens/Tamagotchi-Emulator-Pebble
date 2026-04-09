# Tamagotchi Emulator Pebble
Tamagotchi P1 emulator for the Pebble watch (Time (Steel), Time Round, Pebble 2 (Duo), Time 2, Round 2).
Created for the Spring 2026 Pebble Contest.

![Tamagotchi watchapp screenshot](Tamagotchi/TODO.png)
![Tamagotchi monochrome watchapp screenshot](Tamagotchi/TODO.png)

## Features & Updates:
v1.0:
- TODO

## Getting a ROM
TODO

## Server for running in background (optional)
TODO

## Credits
This project implements [TamaLib](https://github.com/jcrona/tamalib/) by [jcrona](https://github.com/jcrona). I also want to point out that jcrona has created [PebbleGotchi](https://github.com/jcrona/pebblegotchi) about 5 years ago. I believe my implementation to be significantly different to warrant my own publication in the store. In fact PebbleGotchi was never uploaded to the Pebble store as it required the end user to recompile and supply it with a ROM. My implementation allows the user to supply a URL to a ROM file in the configuration page. I've also done the work of porting [TamaLib to Javascript](https://github.com/StefanBauwens/tamalib-js) which has allowed me to create a hostable server tool which can let you keep your Tamagotchi running in the background while the Tamagotchi app is not running on your Pebble.