#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define FPS 30
#define FPS_DELAY 1000/FPS //ms
#define STEP_DELAY 1 //ms
#define STEPS_PER_DELAY 300//55  //TODO

//#undef PBL_COLOR // only used for testing B&W

#include <pebble.h>
#include "tamalib/tamalib.h"
#include "rom.h"

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

//static const uint8_t LCD_WIDTH  = 32;
//static const uint8_t LCD_HEIGHT = 16;

static int8_t s_selectedIcon = -1; // -1 is none, 0-6 says what icon
static bool s_showingAttentionIcon = true;
static bool s_js_ready;
static bool s_lastSaveSendSucceeded = true;
static bool s_pixelsChanged = false;
static uint32_t s_saveStateKey = 32; 

static bool_t s_screen_buffer[LCD_HEIGHT][LCD_WIDTH] = {{0}};

//ticks
static AppTimer *milli_tick_handler;
static AppTimer *screen_tick_handler;
static AppTimer *delayed_input_handler;

/*****************************/
/*   START HAL T FUNCTIONS   */
/*****************************/
static void * hal_malloc(u32_t size) { return 0; } // unused
static void hal_free(void *ptr) { } // unused

static void hal_halt(void)
{
	//Not yet implemented
  //TODO exit program?
}

static bool_t hal_is_log_enabled(log_level_t level) { return false; } // unused
static void hal_log(log_level_t level, char *buff, ...) { } // unused

static timestamp_t hal_get_timestamp(void) //TODO test
{
  time_t seconds;
  uint16_t milliseconds;
  time_ms(&seconds, &milliseconds);

  //return microseconds
  return (int)seconds * 1000000 + (int)milliseconds * 1000; 
}

static void hal_sleep_until(timestamp_t ts) //this makes the time be accurate
{
  while((int) (ts - hal_get_timestamp()) > 0);
}

static void hal_update_screen(void) //since we're not using tamalib_mainloop we must call this ourselves
{
  layer_mark_dirty(s_screen_layer); //Tell the system to redraw screen
}

static void hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val)
{
  s_screen_buffer[y][x] = val; //TTODO
  s_pixelsChanged = true;
}

static void hal_set_lcd_icon(u8_t icon, bool_t val)
{
  if (icon == 7)
  {
    s_showingAttentionIcon = val;
  }
  else
  {
    if (!val && s_selectedIcon == icon)
    {
      s_selectedIcon = -1;
    }
    else if (val)
    {
      s_selectedIcon = icon;
    }
  }
  layer_mark_dirty(s_icons_layer);
}

static void hal_set_frequency(u32_t freq) { } //TODO later for pebbles with speaker?
static void hal_play_frequency(bool_t en) { } //TODO later for pebbles with speaker?

static int hal_handler(void)
{
  return 0;
  //Not implemented as we're not using the tamalib_mainloop()
}

static hal_t hal = {
	.malloc = &hal_malloc,
	.free = &hal_free,
	.halt = &hal_halt,
	.is_log_enabled = &hal_is_log_enabled,
	.log = &hal_log,
	.sleep_until = &hal_sleep_until,
	.get_timestamp = &hal_get_timestamp,
	.update_screen = &hal_update_screen,
	.set_lcd_matrix = &hal_set_lcd_matrix,
	.set_lcd_icon = &hal_set_lcd_icon,
	.set_frequency = &hal_set_frequency,
	.play_frequency = &hal_play_frequency,
	.handler = &hal_handler,
};

/***************************/
/*   END HAL T FUNCTIONS   */
/***************************/

static void milli_tick() //runs once every ms. //TODO can run better I think. Check how I did it with pebble client
{
  for (size_t i = 0; i < STEPS_PER_DELAY; i++) //TODO figure out how much to get an exact value
  {
      tamalib_step();
  }  
  milli_tick_handler = app_timer_register(STEP_DELAY, milli_tick, NULL); //calls itself in 1ms
}

static void screen_tick() //runs every 33 ms for about 30fps
{
  hal_update_screen();
  screen_tick_handler = app_timer_register(FPS_DELAY, screen_tick, NULL);
}

static void SendSaveToPhone(int8_t value) //TODO to implement //TODO TODO TODO
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
     dict_write_int(out_iter, MESSAGE_KEY_Button, &value, sizeof(int8_t), true); //TODO TODO TODO //change key name, change type since this should be the save file only + timestamp?

    // Send this message
    result = app_message_outbox_send();

    // Check the result
    if(result != APP_MSG_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result);
      s_lastSaveSendSucceeded = false;
    }
    else
    {
      s_lastSaveSendSucceeded = true;
    }
  } else {
    // The outbox cannot be used right now
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result);
    s_lastSaveSendSucceeded = false;
  }
}

// Button presses
static void on_button_up_press(ClickRecognizerRef recognizer, void *context) //1
{
  tamalib_set_button(BTN_LEFT, BTN_STATE_PRESSED);
}
static void on_button_select_press(ClickRecognizerRef recognizer, void *context) //2
{
  tamalib_set_button(BTN_MIDDLE, BTN_STATE_PRESSED);
}
static void on_button_down_press(ClickRecognizerRef recognizer, void *context) //3
{
  tamalib_set_button(BTN_RIGHT, BTN_STATE_PRESSED);
}

// Button releases
static void on_button_up_release(ClickRecognizerRef recognizer, void *context) //4
{
  tamalib_set_button(BTN_LEFT, BTN_STATE_RELEASED);
}
static void on_button_select_release(ClickRecognizerRef recognizer, void *context) //5
{
  tamalib_set_button(BTN_MIDDLE, BTN_STATE_RELEASED);
}
static void on_button_down_release(ClickRecognizerRef recognizer, void *context) //6
{
  tamalib_set_button(BTN_RIGHT, BTN_STATE_RELEASED);
}

static void click_config_provider(void *context) {
  // subscribe to button presses here
  window_raw_click_subscribe(BUTTON_ID_UP, on_button_up_press, on_button_up_release, NULL);
  window_raw_click_subscribe(BUTTON_ID_SELECT, on_button_select_press, on_button_select_release, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, on_button_down_press, on_button_down_release, NULL);
}

static void delayed_input() // makes input not work immediately
{
  // Listen for button events
  window_set_click_config_provider(s_main_window, click_config_provider);
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
  // draw new screen
  graphics_context_set_fill_color(ctx, GColorBlack);

  //draw pixels
  for (size_t h = 0; h < LCD_HEIGHT; h++)
  {
    for (size_t w = 0; w < LCD_WIDTH; w++)
    {
      if (s_screen_buffer[h][w])
      {
        graphics_fill_rect(ctx, GRect(w * 4, h * 4, 3, 3), 0, GCornerNone);
      }
    }
  }
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "done drawing...");
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox received");

  //TODO handle save file incoming

  Tuple *ready_tuple_t = dict_find(iter, MESSAGE_KEY_JSReady);

  if(ready_tuple_t) {
    // PebbleKit JS is ready! Safe to send messages
    s_js_ready = true;
  }

  // Handle screen //TODO use same logic for copying over save file?
  /*if (screen_t)
  {
    uint8_t *data = screen_t->value->data;
    size_t length = screen_t->length;
    
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "screen data: %s", data);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "screen length: %d", (int)length);
    memcpy(s_screen_buffer, data, 74);
    
    layer_mark_dirty(s_screen_layer);
  }*/
}

// loop called every second //TODO will thi still work correctly since this gets called when we quit our app?
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) 
{
  // resend button change if unsuccessful
  if (!s_lastSaveSendSucceeded)
  {
    SendSaveToPhone(0); //TODO change this to send actual save file
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

  // Sub to ticks
  milli_tick_handler = app_timer_register(STEP_DELAY, milli_tick, NULL);
  screen_tick_handler = app_timer_register(FPS_DELAY, screen_tick, NULL);
  delayed_input_handler = app_timer_register(3000, delayed_input, NULL);

  //TODO: handle screen when app starts, show pixelated loading screen? send message that pebble app is open? i guess js know that
}

static void main_window_unload(Window *window) { //TODO save state when exiting
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

  // Unsubscribe to ticks
  app_timer_cancel(milli_tick_handler);
  app_timer_cancel(screen_tick_handler);
  app_timer_cancel(delayed_input_handler);
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Listen for seconds
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Open AppMessage connection
  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(256, 128); 

  // Register HAL
  tamalib_register_hal(&hal);

  // Initialization tamalib

  //persist_delete(s_saveStateKey); //TODO temp
  // Check for save file
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Checking for save file");
  if (persist_exists(s_saveStateKey)) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Save state found.Loading...");
    // Read persisted value

    // create instance
    flat_state_t saveState = {0}; 
    persist_read_data(s_saveStateKey, &saveState, sizeof(flat_state_t));
    //cpu_set_state(&saveState);
    cpu_init_from_state(g_program, &saveState, NULL, 1000000);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Save state loaded!");
  }
  else
  {
    tamalib_init(g_program, NULL, 1000000);
  }
}

static void deinit() {
  window_destroy(s_main_window);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Saving state file to persistant storage...");
  //TODO handle sending save file + timestamp to phone
  flat_state_t saveState = cpu_get_flat_state();
  persist_write_data(s_saveStateKey, &saveState, sizeof(flat_state_t));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done saving... %d", saveState.pc);

  // Release tamalib
  tamalib_release();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}