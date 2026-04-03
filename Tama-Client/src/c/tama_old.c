#include <pebble.h>
#include "tamalib/tamalib.h"
#include "rom.h"

#define FPS 30
#define FPS_DELAY 1000/FPS //ms
#define STEP_DELAY 1 //ms
#define STEPS_PER_DELAY 30//55 //max is like 1000000 and blocks the watch pretty much. This also takes a bit for even an hour, so if you haven't checked in a day this will be too slow.

static Window *s_window;
static TextLayer *s_text_layer;
static Layer *s_canvas_layer; //for drawing pixels

//screen
static bool pixelsChanged = false;
static bool_t matrix_buffer[LCD_HEIGHT][LCD_WIDTH] = {{0}};
static bool_t icon_buffer[ICON_NUM] = {0};

//buttons
static bool_t isReleasingButton = false;

//ticks
static AppTimer *milli_tick_handler;
static AppTimer *screen_tick_handler;

/*HAL T FUNCTIONS*/

static void * hal_malloc(u32_t size)
{
  return 0;  //not used as we don't use BP's
}

static void hal_free(void *ptr)
{
	//not used as we don't use BP's
}

static void hal_halt(void)
{
	//Not yet implemented
  //TODO exit program?
}

static bool_t hal_is_log_enabled(log_level_t level)
{
  return false;
  //Not implemented
}

static void hal_log(log_level_t level, char *buff, ...)
{
  //Not implemented
}

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
  layer_mark_dirty(s_canvas_layer); //Tell the system to redraw screen
}

static void hal_set_lcd_matrix(u8_t x, u8_t y, bool_t val)
{
  matrix_buffer[y][x] = val;
  pixelsChanged = true;
}

static void hal_set_lcd_icon(u8_t icon, bool_t val)
{
  icon_buffer[icon] = val;
}

static void hal_set_frequency(u32_t freq)
{
  //Not implemented
}

static void hal_play_frequency(bool_t en)
{
  //Not implemented
}

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

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) { //gets called once a second (while the window is loaded)

}

static void milli_tick() //runs once every 
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
  //layer_mark_dirty(s_canvas_layer);
  screen_tick_handler = app_timer_register(FPS_DELAY, screen_tick, NULL);
}

static void select_press_click_handler(ClickRecognizerRef recognizer, void *context)
{
  tamalib_set_button(BTN_MIDDLE, BTN_STATE_PRESSED);
}

static void select_release_click_handler(ClickRecognizerRef recognizer, void *context)
{
  tamalib_set_button(BTN_MIDDLE, BTN_STATE_RELEASED);
}

static void up_press_click_handler(ClickRecognizerRef recognizer, void *context)
{
  tamalib_set_button(BTN_LEFT, BTN_STATE_PRESSED);
}

static void up_release_click_handler(ClickRecognizerRef recognizer, void *context)
{
  tamalib_set_button(BTN_LEFT, BTN_STATE_RELEASED);
}

static void down_press_click_handler(ClickRecognizerRef recognizer, void *context)
{
  tamalib_set_button(BTN_RIGHT, BTN_STATE_PRESSED);
}

static void down_release_click_handler(ClickRecognizerRef recognizer, void *context)
{
  tamalib_set_button(BTN_RIGHT, BTN_STATE_RELEASED);
}

static void prv_click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_SELECT, select_press_click_handler, select_release_click_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_UP, up_press_click_handler, up_release_click_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, down_press_click_handler, down_release_click_handler, NULL);
}

static void draw_tama_pixel(GContext *ctx, int x, int y)
{
  size_t resize = 4;
  for (size_t w = 0; w < resize; w++)
  {
    for (size_t h = 0; h < resize; h++)
    {
      graphics_draw_pixel(ctx, GPoint(w + x*resize, h + y *resize));
    }
  }
}

//draw screen
static void canvas_update_proc(Layer *layer, GContext *ctx) //gets called whenever the canvas should be updated
{
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "here");
  //clear screen
  GRect bounds = layer_get_bounds(s_canvas_layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, 0);

  //draw pixels
  for (size_t h = 0; h < LCD_HEIGHT; h++)
  {
    for (size_t w = 0; w < LCD_WIDTH; w++)
    {
      if (matrix_buffer[h][w])
      {
        draw_tama_pixel(ctx, w, h);
      }
    }
  }
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  //subscribe to ticks
  //tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  //create drawing canvas layer
  s_canvas_layer = layer_create(bounds); 
  //assign drawing procedure
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  //add to window
  layer_add_child(window_layer, s_canvas_layer);

  s_text_layer = text_layer_create(GRect(0, 72, bounds.size.w, 20));
  text_layer_set_text(s_text_layer, "Press a button");
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));

  //sub to ticks
  milli_tick_handler = app_timer_register(STEP_DELAY, milli_tick, NULL);
  screen_tick_handler = app_timer_register(FPS_DELAY, screen_tick, NULL);
}

static void prv_window_unload(Window *window) {
  //unsubscribe to ticks
  //tick_timer_service_unsubscribe();
  app_timer_cancel(milli_tick_handler);
  app_timer_cancel(screen_tick_handler);

  //destroy canvas layer
  layer_destroy(s_canvas_layer);

  text_layer_destroy(s_text_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  
  //register HAL
  tamalib_register_hal(&hal);
  //init
  tamalib_init(g_program, NULL, 1000000);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();

  //release tamalib
  tamalib_release();
  prv_deinit();
}
