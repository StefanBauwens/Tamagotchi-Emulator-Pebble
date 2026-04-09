module.exports = [
  { 
    "type": "heading", 
    "defaultValue": "Tamagotchi" 
  }, 
  { 
    "type": "text", 
    "defaultValue": "Created by Stefan Bauwens for the Spring 2026 Pebble App Contest." 
  },
  { 
    "type": "text", 
    "defaultValue": "For more info check the readme at https://github.com/StefanBauwens/Tamagotchi-Emulator-Pebble" 
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
          "placeholder": "Try https://pastebin.com/raw/iN0pfyr7"
        }
      },  
      {
        "type": "text",
        "defaultValue": "(Optional) Tamagotchi Server for running in background"
      },
      {
        "type": "input",
        "messageKey": "APIServerUrl",
        "label": "Server",
        "defaultValue": "",
        "attributes": {
          "placeholder": "e.g. http://localhost:3535"
        }
      },  
      {
        "type": "input",
        "messageKey": "APIServerKey",
        "label": "API Key",
        "defaultValue": "",
      },  
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
]