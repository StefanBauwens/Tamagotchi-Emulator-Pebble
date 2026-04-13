module.exports = [
  { 
    "type": "heading", 
    "defaultValue": "Tamagotchi Emu 4 Pebble" 
  }, 
  { 
    "type": "text", 
    "defaultValue": "Created by Stefan Bauwens for the Spring 2026 Pebble App Contest." 
  },
  { 
    "type": "text", 
    "defaultValue": "For more info check the readme at <a href='https://github.com/StefanBauwens/Tamagotchi-Emulator-Pebble'>https://github.com/StefanBauwens/Tamagotchi-Emulator-Pebble</a>" 
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Settings"
      },
      {
        "type": "text",
        "defaultValue": "Link to correctly formatted raw P1 ROM file"
      },
      {
        "type": "input",
        "messageKey": "ROMUrl",
        "label": "ROM URL",
        "defaultValue": "",
        "attributes": {
          "placeholder": "See github readme!"
        }
      },  
      {
        "type": "text",
        "defaultValue": "(Optional) Tamagotchi Server for running in background. Leave blank if not used."
      },
      {
        "type": "input",
        "messageKey": "APIServerUrl",
        "label": "Server",
        "defaultValue": "",
        "attributes": {
          "placeholder": "e.g. http://192.168.0.100:5000"
        }
      },
      {
        "type": "toggle",
        "messageKey": "reset_tamagotchi",
        "label": "Reset Tamagotchi on Save",
        "defaultValue": false
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
]