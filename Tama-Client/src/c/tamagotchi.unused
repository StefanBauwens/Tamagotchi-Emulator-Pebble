#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

//#undef PBL_COLOR // only used for testing B&W

#include <pebble.h>

static Window *s_main_window;
static BitmapLayer *s_background_layer;
static Layer *s_screen_layer;
static Layer *s_icons_layer;

// Bitmaps
static GBitmap *s_bitmap_bg;
static GBitmap *s_bitmap_icon1;
static GBitmap *s_bitmap_icon2;
static GBitmap *s_bitmap_icon3;
static GBitmap *s_bitmap_icon4;
static GBitmap *s_bitmap_icon5;
static GBitmap *s_bitmap_icon6;
static GBitmap *s_bitmap_icon7;
static GBitmap *s_bitmap_icon8;

static const uint8_t LCD_WIDTH  = 32;
static const uint8_t LCD_HEIGHT = 16;

static int8_t s_selectedIcon = -1; // -1 is none, 0-6 says what icon
static bool s_showingAttentionIcon = true;
static bool s_js_ready;

// 0b000 = All buttons are released
// 0b100 = A button pressed, B and C are released
// 0b111 = All buttons are pressed.
// etc..
static int8_t s_buttonFlags = 0;
static bool s_lastButtonSendSucceeded = false;

/*
// Since there's issues with some characters being sent over we are only using characters @(dec:64) to DEL(dec:127).
// There are issues with the characters dec 91, 92, 93, 123 and 125. Since we're using only the least significant bits 91, 92, 93 can be replaced with 27, 28, 29 respectively (on the sender side), since there's no issues with these chars.
// We can't use the same for 123 and 125 since there's also issues with 59 and 61. So we will have to agree on a convention for those bytes. If we get dec. 0, we replace it with dec. 123 and 
// dec. 1 we replace with dec. 125 (on this device)
static char s_screen_buffer[86]; // 16*32 / 6 ≃ 86 (we use 6 bits, because issues with 7 and 8)
*/
static uint8_t s_screen_buffer[74]; // 7 bits - 75 = (16*32)/7

static void SendButtonEventToPhone(int8_t value)
{
  if(!s_js_ready)
  {
    APP_LOG(APP_LOG_LEVEL_WARNING, "JS is not yet ready to receive messages!");
    return;
  }

  // Declare the dictionary's iterator
  DictionaryIterator *out_iter;

  // Prepare the outbox buffer for this message
  AppMessageResult result = app_message_outbox_begin(&out_iter);
  if(result == APP_MSG_OK) {
    // Construct the message
     dict_write_int(out_iter, MESSAGE_KEY_Button, &value, sizeof(int8_t), true);

    // Send this message
    result = app_message_outbox_send();

    // Check the result
    if(result != APP_MSG_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result);
      s_lastButtonSendSucceeded = false;
    }
    else
    {
      s_lastButtonSendSucceeded = true;
    }
  } else {
    // The outbox cannot be used right now
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result);
    s_lastButtonSendSucceeded = false;
  }
}

// Button presses
static void on_button_up_press(ClickRecognizerRef recognizer, void *context) //1
{
  s_buttonFlags |= 4; // 0b100
  SendButtonEventToPhone(s_buttonFlags);
}
static void on_button_select_press(ClickRecognizerRef recognizer, void *context) //2
{
  s_buttonFlags |= 2; // 0b010
  SendButtonEventToPhone(s_buttonFlags);
}
static void on_button_down_press(ClickRecognizerRef recognizer, void *context) //3
{
  s_buttonFlags |= 1; // 0b001
  SendButtonEventToPhone(s_buttonFlags);
}

// Button releases
static void on_button_up_release(ClickRecognizerRef recognizer, void *context) //4
{
  s_buttonFlags &= 3; // 0b011
  SendButtonEventToPhone(s_buttonFlags);
}
static void on_button_select_release(ClickRecognizerRef recognizer, void *context) //5
{
  s_buttonFlags &= 5; // 0b101
  SendButtonEventToPhone(s_buttonFlags);
}
static void on_button_down_release(ClickRecognizerRef recognizer, void *context) //6
{
  s_buttonFlags &= 6; // 0b110
  SendButtonEventToPhone(s_buttonFlags);
}

static void click_config_provider(void *context) {
  // subscribe to button presses here
  window_raw_click_subscribe(BUTTON_ID_UP, on_button_up_press, on_button_up_release, NULL);
  window_raw_click_subscribe(BUTTON_ID_SELECT, on_button_select_press, on_button_select_release, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, on_button_down_press, on_button_down_release, NULL);
}

// Handles drawing icons layers
static void icons_update_proc(Layer *layer, GContext *ctx) {
  // Set the draw color
  graphics_context_set_fill_color(ctx, GColorBlack);

  // Set the compositing mode (GCompOpSet is required for transparency)
  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  if(s_selectedIcon >= 0)
  {
    uint8_t xPos = 12 + ((s_selectedIcon%4) * 32);
    uint8_t yPos = (s_selectedIcon > 3 ? 100 : 0);

    GBitmap* selected_icon = s_bitmap_icon7;
    switch(s_selectedIcon)
    {
      case 0:
        selected_icon = s_bitmap_icon1;
        break;
      case 1:
        selected_icon = s_bitmap_icon2;
        break;
      case 2:
        selected_icon = s_bitmap_icon3;
        break;
      case 3:
        selected_icon = s_bitmap_icon4;
        break;
      case 4:
        selected_icon = s_bitmap_icon5;
        break;
      case 5:
        selected_icon = s_bitmap_icon6;
        break;
      case 6:
        selected_icon = s_bitmap_icon7;
        break;
      default:
        selected_icon = s_bitmap_icon7;
        break;
    }

    // Draw selected icon if selected
    graphics_draw_bitmap_in_rect(ctx, selected_icon, GRect(xPos, yPos, 22, 18));
  }

  // Handle attention icon
  if(s_showingAttentionIcon)
  {
    graphics_draw_bitmap_in_rect(ctx, s_bitmap_icon8, GRect(108, 100, 22, 18)); 
  }
}

// Handles drawing screen layer
static void screen_update_proc(Layer *layer, GContext *ctx) { 

  APP_LOG(APP_LOG_LEVEL_DEBUG, "first byte %d second byte %d", s_screen_buffer[0] , s_screen_buffer[1]);

  // draw new screen
  graphics_context_set_fill_color(ctx, GColorBlack);

  //bool pixelOn = true;

  // use 6 least signifcant pixels per byte
  /*
  uint16_t pixelCount = 0; 
  uint8_t xPos = 0;
  uint8_t yPos = 0;
  for (uint8_t i = 0; i < 86; i++)
  {
    for(uint8_t b = 0; b < 6; b++)
    {
      if (pixelCount >= 512) // prevent excess since we're not working with the full amount of bytes
      {
        break;
      }

      xPos = pixelCount%32;
      yPos = pixelCount/32;

      pixelOn = (bitRead(s_screen_buffer[i], 5 - b) == 1); // only checks the 6 least significant bits
      if (pixelOn)
      {
        graphics_fill_rect(ctx, GRect((xPos) * 4, yPos * 4, 3, 3), 0, GCornerNone);
      }

      pixelCount++;
    }
  }*/

  // use 7 least significant pixels per byte
  for (uint8_t y = 0; y < LCD_HEIGHT; y++)
  {
    for (uint8_t x = 0; x < LCD_WIDTH; x++)
    {
      uint16_t charIndex = ((y * LCD_WIDTH) + x) / 7; // auto floored
      uint8_t rest = ((y * LCD_WIDTH) + x) % 7;
      uint8_t character = s_screen_buffer[charIndex];
      bool val = bitRead(character, 6 - rest) == 1;

      if (val) 
      {
        graphics_fill_rect(ctx, GRect(x * 4, y * 4, 3, 3), 0, GCornerNone);
      }
    }
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "done drawing...");
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox received");

  Tuple *screen_t = dict_find(iter, MESSAGE_KEY_Screen);
  Tuple *selectedIcon_t = dict_find(iter, MESSAGE_KEY_SelectedIcon);
  Tuple *showingAttentionIcon_t = dict_find(iter, MESSAGE_KEY_ShowingAttentionIcon);
  Tuple *ready_tuple_t = dict_find(iter, MESSAGE_KEY_JSReady);

  if(ready_tuple_t) {
    // PebbleKit JS is ready! Safe to send messages
    s_js_ready = true;
  }

  // Handle screen
  if (screen_t)
  {
    uint8_t *data = screen_t->value->data;
    size_t length = screen_t->length;
    
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "screen data: %s", data);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "screen length: %d", (int)length);
    memcpy(s_screen_buffer, data, 74);
    
    layer_mark_dirty(s_screen_layer);
  }

  // Handle selected icon
  if (selectedIcon_t)
  {
    s_selectedIcon = atoi(selectedIcon_t->value->cstring);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "selected icon changed %d", s_selectedIcon);
    layer_mark_dirty(s_icons_layer);
  }

  // Handle attention icon
  if (showingAttentionIcon_t)
  {
    s_showingAttentionIcon = strcmp(showingAttentionIcon_t->value->cstring, "true") == 0;
    layer_mark_dirty(s_icons_layer);
  }
}

// loop called every second
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) 
{
  // resend button change if unsuccessful
  if (!s_lastButtonSendSucceeded)
  {
    SendButtonEventToPhone(s_buttonFlags);
  }
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create GBitmap for background 
#if defined(PBL_COLOR)
  s_bitmap_bg = gbitmap_create_with_resource(RESOURCE_ID_BG_IMAGE);
#else
  s_bitmap_bg = gbitmap_create_with_resource(RESOURCE_ID_BG_IMAGE_BW);
#endif

  // Create background layer
  s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_compositing_mode(s_background_layer, GCompOpSet);
  bitmap_layer_set_bitmap(s_background_layer, s_bitmap_bg);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));

  // Create bitmaps for icons
  s_bitmap_icon1 = gbitmap_create_with_resource(RESOURCE_ID_ICON1);
  s_bitmap_icon2 = gbitmap_create_with_resource(RESOURCE_ID_ICON2);
  s_bitmap_icon3 = gbitmap_create_with_resource(RESOURCE_ID_ICON3);
  s_bitmap_icon4 = gbitmap_create_with_resource(RESOURCE_ID_ICON4);
  s_bitmap_icon5 = gbitmap_create_with_resource(RESOURCE_ID_ICON5);
  s_bitmap_icon6 = gbitmap_create_with_resource(RESOURCE_ID_ICON6);
  s_bitmap_icon7 = gbitmap_create_with_resource(RESOURCE_ID_ICON7);
  s_bitmap_icon8 = gbitmap_create_with_resource(RESOURCE_ID_ICON8);

  // Create icons layer
  s_icons_layer = layer_create(GRect(0, 24, 144, 140));
  layer_set_update_proc(s_icons_layer, icons_update_proc);

  // Add to window
  layer_add_child(window_layer, s_icons_layer);

  // Create screen Layer
  s_screen_layer = layer_create(GRect(8, 51, 128, 64));
  layer_set_update_proc(s_screen_layer, screen_update_proc);

  // Add to window  
  layer_add_child(window_layer, s_screen_layer);

  //TODO: handle screen when app starts, show pixelated loading screen? send message that pebble app is open? i guess js know that
}

static void main_window_unload(Window *window) {
  // Destroy backrgound bitmap and its layer
  gbitmap_destroy(s_bitmap_bg);
  bitmap_layer_destroy(s_background_layer);

  // Destroy icon bitmaps
  gbitmap_destroy(s_bitmap_icon1);
  gbitmap_destroy(s_bitmap_icon2);
  gbitmap_destroy(s_bitmap_icon3);
  gbitmap_destroy(s_bitmap_icon4);
  gbitmap_destroy(s_bitmap_icon5);
  gbitmap_destroy(s_bitmap_icon6);
  gbitmap_destroy(s_bitmap_icon7);
  gbitmap_destroy(s_bitmap_icon8);

  // Destory icons layer
  layer_destroy(s_icons_layer);

  // Destroy screen layer
  layer_destroy(s_screen_layer);
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Listen for button events
  window_set_click_config_provider(s_main_window, click_config_provider);

  // Listen for seconds
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Open AppMessage connection
  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(256, 128); 
}

static void deinit() {
  window_destroy(s_main_window);
}
// hello world

int main(void) {
  init();
  app_event_loop();
  deinit();
}