// TODO
// Fix pebble round
// Improve loading to maybe pixelated progress bar?
// Show also error if no connection with watch!!
// Show error if bad rom url
// Make clay settings page
// Tell user fetching save file from server/ syncing with server
// Make source public + add readme

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define FPS 30
#define FPS_DELAY 1000/FPS //ms
#define STEP_DELAY 1 //ms
#define STEPS_PER_DELAY 600//55  

#define VRAM_SIZE (64 + 13)
#define BYTES_PER_LINE 32
#define BITS_PER_BYTE 8

//#undef PBL_COLOR // only used for testing B&W

#include <pebble.h>
#include "tamalib/tamalib.h"
//#include "rom.h" 

static void initTamalib(void); 

static Window *s_main_window;
static BitmapLayer *s_background_layer;
static Layer *s_screen_layer;
static Layer *s_icons_layer;
static TextLayer *s_text_layer;

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
static bool s_showingAttentionIcon = false;
static bool s_js_ready;
static bool s_pixelsChanged = false;
static uint32_t s_saveStateKey = 32; 

static bool_t s_screen_buffer[LCD_HEIGHT][LCD_WIDTH] = {{0}};
static u12_t g_program[6144] = {0};
static bool s_hasReceivedRom = false;
static bool s_hasReceivedSaveFile = false;
static bool s_clearTextLayerOnScreenRefresh = false;
static flat_state_t stateToLoad = {0};

//ticks
static AppTimer *milli_tick_handler;
static AppTimer *screen_tick_handler;

/*****************************/
/*   START HAL T FUNCTIONS   */
/*****************************/
static void * hal_malloc(u32_t size) { return 0; } // unused
static void hal_free(void *ptr) { } // unused

static void hal_halt(void)
{
	//Not yet implemented
}

static bool_t hal_is_log_enabled(log_level_t level) { return false; } // unused
static void hal_log(log_level_t level, char *buff, ...) { } // unused

static timestamp_t hal_get_timestamp(void)
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
  s_screen_buffer[y][x] = val;
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

void set_screen_to_last_state(uint8_t *fullRam) { // gets screen data from memory and sets it to the screen
    uint8_t vram[VRAM_SIZE];

    memcpy(vram, fullRam + 320, VRAM_SIZE);

    uint8_t adjustedVram[VRAM_SIZE];
    int idx = 0;

    // Helper macro to copy forward
    #define COPY_RANGE(start, end) \
        for (int i = (start); i < (end); i++) { \
            adjustedVram[idx++] = vram[i]; \
        }

    // Helper macro to copy reversed
    #define COPY_RANGE_REVERSE(start, end) \
        for (int i = (end) - 1; i >= (start); i--) { \
            adjustedVram[idx++] = vram[i]; \
        }

    // Replicating your JS slices
    COPY_RANGE(0, 8);
    COPY_RANGE(9, 17);
    COPY_RANGE_REVERSE(29, 37);
    COPY_RANGE_REVERSE(20, 28);
    COPY_RANGE(40, 48);
    COPY_RANGE(49, 57);
    COPY_RANGE_REVERSE(69, 77);
    COPY_RANGE_REVERSE(60, 68);

    int totalBytes = idx;

    for (int i = 0; i < totalBytes; i++) {
        uint8_t byte = adjustedVram[i];

        int x = (i % BYTES_PER_LINE);
        int baseY = (i / BYTES_PER_LINE) * BITS_PER_BYTE;

        for (int bitIndex = 0; bitIndex < BITS_PER_BYTE; bitIndex++) {
          int bit = (byte >> bitIndex) & 1;  
          //int bit = (byte >> (7 - bitIndex)) & 1;

            int y = baseY + bitIndex;

            hal.set_lcd_matrix(x, y, bit);
        }
    }

    #undef COPY_RANGE
    #undef COPY_RANGE_REVERSE
}

static void milli_tick() //runs once every ms.
{
  if (s_hasReceivedRom && s_hasReceivedSaveFile)
  {
    for (size_t i = 0; i < STEPS_PER_DELAY; i++)
    {
        tamalib_step();
    } 
  } 
  milli_tick_handler = app_timer_register(STEP_DELAY, milli_tick, NULL); //calls itself in 1ms
}

static void screen_tick() //runs every 33 ms for about 30fps
{
  if (s_hasReceivedRom && s_hasReceivedSaveFile)
  {
    hal_update_screen();
  }

  if (s_clearTextLayerOnScreenRefresh)
  {
    text_layer_set_text(s_text_layer, " ");
  }
  screen_tick_handler = app_timer_register(FPS_DELAY, screen_tick, NULL);
}

// Button presses
static void on_button_up_press(ClickRecognizerRef recognizer, void *context) //1
{
  if (!s_hasReceivedRom || !s_hasReceivedSaveFile) return;
  tamalib_set_button(BTN_LEFT, BTN_STATE_PRESSED);
}
static void on_button_select_press(ClickRecognizerRef recognizer, void *context) //2
{
  if (!s_hasReceivedRom || !s_hasReceivedSaveFile) return;
  tamalib_set_button(BTN_MIDDLE, BTN_STATE_PRESSED);
}
static void on_button_down_press(ClickRecognizerRef recognizer, void *context) //3
{
  if (!s_hasReceivedRom || !s_hasReceivedSaveFile) return;
  tamalib_set_button(BTN_RIGHT, BTN_STATE_PRESSED);
}

// Button releases
static void on_button_up_release(ClickRecognizerRef recognizer, void *context) //4
{
  if (!s_hasReceivedRom || !s_hasReceivedSaveFile) return;
  tamalib_set_button(BTN_LEFT, BTN_STATE_RELEASED);
}
static void on_button_select_release(ClickRecognizerRef recognizer, void *context) //5
{
  if (!s_hasReceivedRom || !s_hasReceivedSaveFile) return;
  tamalib_set_button(BTN_MIDDLE, BTN_STATE_RELEASED);
}
static void on_button_down_release(ClickRecognizerRef recognizer, void *context) //6
{
  if (!s_hasReceivedRom || !s_hasReceivedSaveFile) return;
  tamalib_set_button(BTN_RIGHT, BTN_STATE_RELEASED);
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
    #if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    uint8_t xPos = 12 + ((s_selectedIcon%4) * 40); 
    uint8_t yPos = (s_selectedIcon > 3 ? 120 : 0);
    #else
    uint8_t xPos = 12 + ((s_selectedIcon%4) * 32);
    uint8_t yPos = (s_selectedIcon > 3 ? 100 : 0);
    #endif

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
    #if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    graphics_draw_bitmap_in_rect(ctx, selected_icon, GRect(xPos, yPos, 27, 22));
    #else
    graphics_draw_bitmap_in_rect(ctx, selected_icon, GRect(xPos, yPos, 22, 18));
    #endif
  }

  // Handle attention icon
  if(s_showingAttentionIcon)
  {
    #if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    graphics_draw_bitmap_in_rect(ctx, s_bitmap_icon8, GRect(12+(40*3), 120, 27, 22)); 
    #else
    graphics_draw_bitmap_in_rect(ctx, s_bitmap_icon8, GRect(108, 100, 22, 18));
    #endif
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
        #if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
        graphics_fill_rect(ctx, GRect(w * 5, h * 5, 4, 4), 0, GCornerNone);
        #else
        graphics_fill_rect(ctx, GRect(w * 4, h * 4, 3, 3), 0, GCornerNone);
        #endif
      }
    }
  }
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "inbox received");

  Tuple *ready_tuple_t = dict_find(iter, MESSAGE_KEY_JSReady);

  if(ready_tuple_t) {
    // PebbleKit JS is ready! Safe to send messages
    s_js_ready = true;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "js ready received");
  }

  // handle incoming rom
  Tuple *offset_t = dict_find(iter, MESSAGE_KEY_ROMOffset);
  Tuple *chunk_t = dict_find(iter, MESSAGE_KEY_ROMChunk);

  if(offset_t && chunk_t) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "chunk received");
    int offset = offset_t->value->int16;
    uint8_t *chunk = chunk_t->value->data;
    int index = 0;

    // Convert bytes → u12_t values
    for (int i = 0; i < chunk_t->length; i += 2) {
        index = (offset + i) / 2;

        if (index >= 6144) break; // safety

        u12_t value = chunk[i] | (chunk[i + 1] << 8);

        g_program[index] = value & 0x0FFF; // ensure 12-bit

        static char progress_text[25];
        int percentage = (offset * 100)/12288;
        snprintf(progress_text, sizeof(progress_text), "Loading ROM %d%%", percentage);
        text_layer_set_text(s_text_layer, progress_text);
    }
    if (index == 6143)
    {
      // we reached the end and can safely start now
      text_layer_set_text(s_text_layer, "Loading ROM 100%");
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Reached end of ROM!");
      s_hasReceivedRom = true;
      // wait for save file from js
    }
  }

  // Handle error messages
  Tuple *JSMessage_t = dict_find(iter, MESSAGE_KEY_JSMessage);
  if (JSMessage_t)
  {
    char *jsMessage = JSMessage_t->value->cstring;
    text_layer_set_text(s_text_layer, jsMessage);
  }
  
  // Handle incoming save state
  Tuple *STATEnone_t = dict_find(iter, MESSAGE_KEY_STATEnone);

  Tuple *STATEpc_t = dict_find(iter, MESSAGE_KEY_STATEpc);
  Tuple *STATEx_t = dict_find(iter, MESSAGE_KEY_STATEx);
  Tuple *STATEy_t = dict_find(iter, MESSAGE_KEY_STATEy);
  Tuple *STATEa_t = dict_find(iter, MESSAGE_KEY_STATEa);
  Tuple *STATEb_t = dict_find(iter, MESSAGE_KEY_STATEb);
  Tuple *STATEnp_t = dict_find(iter, MESSAGE_KEY_STATEnp);
  Tuple *STATEsp_t = dict_find(iter, MESSAGE_KEY_STATEsp);
  Tuple *STATEflags_t = dict_find(iter, MESSAGE_KEY_STATEflags);

  Tuple *STATEtick_counter_t = dict_find(iter, MESSAGE_KEY_STATEtick_counter);
  Tuple *STATEclk_timer_timestamp_t = dict_find(iter, MESSAGE_KEY_STATEclk_timer_timestamp);
  Tuple *STATEprog_timer_timestamp_t = dict_find(iter, MESSAGE_KEY_STATEprog_timer_timestamp);
  Tuple *STATEprog_timer_enabled_t = dict_find(iter, MESSAGE_KEY_STATEprog_timer_enabled);
  Tuple *STATEprog_timer_data_t = dict_find(iter, MESSAGE_KEY_STATEprog_timer_data);
  Tuple *STATEprog_timer_rld_t = dict_find(iter, MESSAGE_KEY_STATEprog_timer_rld);
  Tuple *STATEcall_depth_t = dict_find(iter, MESSAGE_KEY_STATEcall_depth);

  Tuple *STATEinterrupts_t = dict_find(iter, MESSAGE_KEY_STATEinterrupts);
  Tuple *STATEmemory_t = dict_find(iter, MESSAGE_KEY_STATEmemory);

  Tuple *STATEselected_icon_t = dict_find(iter, MESSAGE_KEY_STATEselected_icon);
  Tuple *STATEshowing_attention_icon_t = dict_find(iter, MESSAGE_KEY_STATEshowing_attention_icon);

  if(STATEnone_t)
  {
    s_clearTextLayerOnScreenRefresh = true;
    initTamalib();
  }

  if (STATEpc_t && STATEmemory_t) // assume whole block
  {
    text_layer_set_text(s_text_layer, "Loading save state...");

    uint16_t state_pc = STATEpc_t->value->uint16;
    uint16_t state_x = STATEx_t->value->uint16;
    uint16_t state_y = STATEy_t->value->uint16;
    uint8_t state_a = STATEa_t->value->uint8;
    uint8_t state_b = STATEb_t->value->uint8;
    uint8_t state_np = STATEnp_t->value->uint8;
    uint8_t state_sp = STATEsp_t->value->uint8;
    uint8_t state_flags = STATEflags_t->value->uint8;

    uint32_t state_tick_counter = STATEtick_counter_t->value->uint32;
    uint32_t state_clk_timer_timestamp = STATEclk_timer_timestamp_t->value->uint32;
    uint32_t state_prog_timer_timestamp = STATEprog_timer_timestamp_t->value->uint32;
    uint8_t state_prog_timer_enabled = STATEprog_timer_enabled_t->value->uint8;
    uint8_t state_prog_timer_data = STATEprog_timer_data_t->value->uint8;
    uint8_t state_prog_timer_rld = STATEprog_timer_rld_t->value->uint8;
    uint32_t state_call_depth = STATEcall_depth_t->value->uint32;

    uint8_t *state_interrupts = STATEinterrupts_t->value->data;
    uint8_t *state_memory = STATEmemory_t->value->data;

    stateToLoad.pc = state_pc;
    stateToLoad.x = state_x;
    stateToLoad.y = state_y;
    stateToLoad.a = state_a;
    stateToLoad.b = state_b;
    stateToLoad.np = state_np;
    stateToLoad.sp = state_sp;
    stateToLoad.flags = state_flags;

    stateToLoad.tick_counter = state_tick_counter;
    stateToLoad.clk_timer_timestamp = state_clk_timer_timestamp;
    stateToLoad.prog_timer_timestamp = state_prog_timer_timestamp;
    stateToLoad.prog_timer_enabled = state_prog_timer_enabled;
    stateToLoad.prog_timer_data = state_prog_timer_data;
    stateToLoad.prog_timer_rld = state_prog_timer_rld;
    stateToLoad.call_depth = state_call_depth;

    stateToLoad.interrupts[0].factor_flag_reg = state_interrupts[0];
    stateToLoad.interrupts[0].mask_reg        = state_interrupts[1];  
    stateToLoad.interrupts[0].triggered       = state_interrupts[2];  
    stateToLoad.interrupts[0].vector          = state_interrupts[3];  
    stateToLoad.interrupts[1].factor_flag_reg = state_interrupts[4];
    stateToLoad.interrupts[1].mask_reg        = state_interrupts[5];  
    stateToLoad.interrupts[1].triggered       = state_interrupts[6];  
    stateToLoad.interrupts[1].vector          = state_interrupts[7];  
    stateToLoad.interrupts[2].factor_flag_reg = state_interrupts[8];
    stateToLoad.interrupts[2].mask_reg        = state_interrupts[9];  
    stateToLoad.interrupts[2].triggered       = state_interrupts[10];  
    stateToLoad.interrupts[2].vector          = state_interrupts[11]; 
    stateToLoad.interrupts[3].factor_flag_reg = state_interrupts[12];
    stateToLoad.interrupts[3].mask_reg        = state_interrupts[13];  
    stateToLoad.interrupts[3].triggered       = state_interrupts[14];  
    stateToLoad.interrupts[3].vector          = state_interrupts[15]; 
    stateToLoad.interrupts[4].factor_flag_reg = state_interrupts[16];
    stateToLoad.interrupts[4].mask_reg        = state_interrupts[17];  
    stateToLoad.interrupts[4].triggered       = state_interrupts[18];  
    stateToLoad.interrupts[4].vector          = state_interrupts[19];         
    stateToLoad.interrupts[5].factor_flag_reg = state_interrupts[20];
    stateToLoad.interrupts[5].mask_reg        = state_interrupts[21];  
    stateToLoad.interrupts[5].triggered       = state_interrupts[22];  
    stateToLoad.interrupts[5].vector          = state_interrupts[23]; 

    memcpy(stateToLoad.memory, state_memory, sizeof(stateToLoad.memory));

    s_hasReceivedSaveFile = true;
    s_clearTextLayerOnScreenRefresh = true;

    if (STATEselected_icon_t && STATEshowing_attention_icon_t)
    {
      s_selectedIcon = STATEselected_icon_t->value->int8;
      s_showingAttentionIcon = STATEshowing_attention_icon_t->value->int8;
      layer_mark_dirty(s_icons_layer);
    }

    initTamalib();
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
#if defined(PBL_PLATFORM_CHALK)
  s_background_layer = bitmap_layer_create(GRect(0, 0, 180, 180));
#elif defined(PBL_PLATFORM_GABBRO)
    s_background_layer = bitmap_layer_create(GRect(0, 0, 260, 260));
#elif defined(PBL_PLATFORM_EMERY)
    s_background_layer = bitmap_layer_create(GRect(0, 0, 200, 228));
#else
  s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
#endif
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
#if defined(PBL_PLATFORM_CHALK)
  s_icons_layer = layer_create(GRect(0+18, 24+6, 144, 146));
#elif defined(PBL_PLATFORM_GABBRO) 
  s_icons_layer = layer_create(GRect(0+45, 60, 180, 183)); 
#elif defined(PBL_PLATFORM_EMERY)
  s_icons_layer = layer_create(GRect(0+15, 44, 180, 183)); 
#else
  s_icons_layer = layer_create(GRect(0, 24, 144, 146));
#endif
  layer_set_update_proc(s_icons_layer, icons_update_proc);

  // Add to window
  layer_add_child(window_layer, s_icons_layer);

  // Create screen Layer
#if defined(PBL_PLATFORM_CHALK)
  s_screen_layer = layer_create(GRect(8+18, 51+6, 128, 64));
#elif defined(PBL_PLATFORM_GABBRO)
  s_screen_layer = layer_create(GRect(50, 92, 160, 80));
#elif defined(PBL_PLATFORM_EMERY)
  s_screen_layer = layer_create(GRect(20, 76, 160, 80));
#else
  s_screen_layer = layer_create(GRect(8, 51, 128, 64));
#endif
  layer_set_update_proc(s_screen_layer, screen_update_proc);

  // Add to window  
  layer_add_child(window_layer, s_screen_layer);

  // Create text layer
  #if defined(PBL_PLATFORM_CHALK)
  s_text_layer = text_layer_create(GRect(0, 60+6, bounds.size.w, 50));  //TODO check
  #elif defined(PBL_PLATFORM_GABBRO)
  s_text_layer = text_layer_create(GRect(0, 60+46, bounds.size.w, 50)); //TODO TODO
  #elif defined(PBL_PLATFORM_EMERY)
  s_text_layer = text_layer_create(GRect(0, 60+30, bounds.size.w, 50));  //TODO TODO
  #else   
  s_text_layer = text_layer_create(GRect(0, 60, bounds.size.w, 50)); 
  #endif
  text_layer_set_background_color(s_text_layer, GColorClear);
  text_layer_set_text(s_text_layer, "Loading ROM 0%");
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_text_layer, GTextOverflowModeWordWrap);
  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));

  // Sub to ticks
  milli_tick_handler = app_timer_register(STEP_DELAY, milli_tick, NULL);
  screen_tick_handler = app_timer_register(FPS_DELAY, screen_tick, NULL);

  //TODO: handle screen when app starts, show pixelated loading screen? send message that pebble app is open? i guess js know that
}

static void main_window_unload(Window *window) {
  // Destroy backrgound bitmap and its layer
  gbitmap_destroy(s_bitmap_bg);
  bitmap_layer_destroy(s_background_layer);

  // Destroy text layer
  text_layer_destroy(s_text_layer);

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
}

static void initTamalib() {
  // Register HAL
  tamalib_register_hal(&hal);

  // Check for save file
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Checking for save file");
  if (s_hasReceivedSaveFile) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Save state found.Loading...");

    cpu_init_from_state(g_program, &stateToLoad, NULL, 1000000);
    set_screen_to_last_state(stateToLoad.memory); 
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Save state loaded!");
  }
  else
  {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "No save file");
    s_hasReceivedSaveFile = true;
    tamalib_init(g_program, NULL, 1000000);
  }
}

static void init() { //TODO show error when no connection to phone can be made!
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Listen for seconds
  //tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Open AppMessage connection
  app_message_register_inbox_received(prv_inbox_received_handler);
  //app_message_open(256, 128); 
  app_message_open(2048, 2048); // tested on pebble 2 duo

  // Listen for button events
  window_set_click_config_provider(s_main_window, click_config_provider);
}

static void saveCurrentState()
{
  if (!s_hasReceivedRom) return; // no point in sending save if we don't even have the rom yet.
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Getting save file and sending to phone...");

  // send to phone? Is this possible since it's async and stuff?
  if(!s_js_ready)
  {
    APP_LOG(APP_LOG_LEVEL_WARNING, "JS is not yet ready to receive messages!"); // warn user!
    return;
  }

  // Send save file to phone 
  flat_state_t saveState = cpu_get_flat_state();

  // Declare the dictionary's iterator
  DictionaryIterator *out_iter;

  // Prepare the outbox buffer for this message
  AppMessageResult result = app_message_outbox_begin(&out_iter);
  if(result == APP_MSG_OK) {
    // Construct the message
    dict_write_int(out_iter, MESSAGE_KEY_STATEpc, &saveState.pc, sizeof(uint16_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEx, &saveState.x, sizeof(uint16_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEy, &saveState.y, sizeof(uint16_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEa, &saveState.a, sizeof(uint8_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEb, &saveState.b, sizeof(uint8_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEnp, &saveState.np, sizeof(uint8_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEsp, &saveState.sp, sizeof(uint8_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEflags, &saveState.flags, sizeof(uint8_t), false);
    
    dict_write_int(out_iter, MESSAGE_KEY_STATEtick_counter, &saveState.tick_counter, sizeof(uint32_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEclk_timer_timestamp, &saveState.clk_timer_timestamp, sizeof(uint32_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEprog_timer_timestamp, &saveState.prog_timer_timestamp, sizeof(uint32_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEprog_timer_enabled, &saveState.prog_timer_enabled, sizeof(uint8_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEprog_timer_data, &saveState.prog_timer_data, sizeof(uint8_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEprog_timer_rld, &saveState.prog_timer_rld, sizeof(uint8_t), false);
    dict_write_int(out_iter, MESSAGE_KEY_STATEcall_depth, &saveState.call_depth, sizeof(uint32_t), false);

    uint8_t interrupts[24] = {
      saveState.interrupts[0].factor_flag_reg, saveState.interrupts[0].mask_reg, saveState.interrupts[0].triggered, saveState.interrupts[0].vector,
      saveState.interrupts[1].factor_flag_reg, saveState.interrupts[1].mask_reg, saveState.interrupts[1].triggered, saveState.interrupts[1].vector,
      saveState.interrupts[2].factor_flag_reg, saveState.interrupts[2].mask_reg, saveState.interrupts[2].triggered, saveState.interrupts[2].vector,
      saveState.interrupts[3].factor_flag_reg, saveState.interrupts[3].mask_reg, saveState.interrupts[3].triggered, saveState.interrupts[3].vector,
      saveState.interrupts[4].factor_flag_reg, saveState.interrupts[4].mask_reg, saveState.interrupts[4].triggered, saveState.interrupts[4].vector,
      saveState.interrupts[5].factor_flag_reg, saveState.interrupts[5].mask_reg, saveState.interrupts[5].triggered, saveState.interrupts[5].vector,
    };

    dict_write_data(out_iter, MESSAGE_KEY_STATEinterrupts, interrupts, sizeof(interrupts));

    dict_write_data(out_iter, MESSAGE_KEY_STATEmemory, saveState.memory, sizeof(saveState.memory));

    // handle icons
    dict_write_int(out_iter, MESSAGE_KEY_STATEselected_icon, &s_selectedIcon, sizeof(int8_t), true);
    dict_write_int(out_iter, MESSAGE_KEY_STATEshowing_attention_icon, &s_showingAttentionIcon, sizeof(int8_t), true);

     dict_write_end(out_iter);
    // Send this message
    result = app_message_outbox_send();

    // Check the result
    if(result != APP_MSG_OK) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Error sending the outbox: %d", (int)result);
    }
    else
    {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Save file sent successfully to phone!");
    }
  } else {
    // The outbox cannot be used right now
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error preparing the outbox: %d", (int)result);
  }
}

static void deinit() {
  window_destroy(s_main_window);

  // Handle saving state
  saveCurrentState();

  // Release tamalib
  tamalib_release();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}