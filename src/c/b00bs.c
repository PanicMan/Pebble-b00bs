#include <pebble.h>

enum TimerKey {
	TIMER_ANIM_B00BS = 0x0001,
	TIMER_ANIM_BATT = 0x0002,
};

static const uint32_t segments_hr[] = {100, 100, 100};
static const VibePattern vibe_pat_hr = {
	.durations = segments_hr,
	.num_segments = ARRAY_LENGTH(segments_hr),
};

static const uint32_t segments_bt[] = {100, 100, 100, 100, 400, 400, 100, 100, 100};
static const VibePattern vibe_pat_bt = {
	.durations = segments_bt,
	.num_segments = ARRAY_LENGTH(segments_bt),
};

Window *window;
BitmapLayer *b00bs_layer, *radio_layer, *battery_layer;

static GFont digitS;
char hhBuffer[] = "00";
char ddmmyyyyBuffer[] = "00.00.0000";
static GBitmapSequence 
				*s_sequence;
static GBitmap 	*bmp_b00bs,
				*bmp_batt, 
				*bmp_radio, 
				*bmp_batteryAll;
static int16_t 	aktBatt, 
				aktBattAnim, 
				aktBT;
static AppTimer *timer_b00bs, 
				*timer_face, 
				*timer_batt;
static bool 	b_initialized, 
				b_charging, 
				b_animating;

//-------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------
static void timerCallback(void *data) 
{
	if ((int)data == TIMER_ANIM_B00BS)
	{
		uint32_t next_delay;
		// Advance to the next APNG frame, and get the delay for this frame
		if(gbitmap_sequence_update_bitmap_next_frame(s_sequence, bmp_b00bs, &next_delay)) 
		{
			// Set the new frame into the BitmapLayer
			bitmap_layer_set_bitmap(b00bs_layer, bmp_b00bs);
			layer_mark_dirty(bitmap_layer_get_layer(b00bs_layer));

			// Timer for that frame's delay
			timer_b00bs = app_timer_register(next_delay, timerCallback, (void*)TIMER_ANIM_B00BS);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------
static void update_configuration(void)
{
}
//-------------------------------------------------------------------------------------------------------------
static void window_load(Window *window) 
{
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer), rc;
	
	digitS = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITAL_23));
	
	// Create sequence
	s_sequence = gbitmap_sequence_create_with_resource(RESOURCE_ID_IMAGE_B00BS);

	// Create blank GBitmap using APNG frame size
	GSize png_size = gbitmap_sequence_get_bitmap_size(s_sequence);
#ifdef PBL_COLOR
	bmp_b00bs = gbitmap_create_blank(png_size, GBitmapFormat8Bit);
#else
	bmp_b00bs = gbitmap_create_blank(png_size, GBitmapFormat1Bit);
#endif

	int32_t png_idx = gbitmap_sequence_get_current_frame_idx(s_sequence);
	uint32_t png_frames = gbitmap_sequence_get_total_num_frames(s_sequence);

	APP_LOG(APP_LOG_LEVEL_DEBUG, "png_size: %dx%d", png_size.w, png_size.h);
	APP_LOG(APP_LOG_LEVEL_DEBUG, "png_idx: %d, png_frames: %d", (int)png_idx, (int)png_frames);

	//Init Bitmaps
	bmp_batteryAll = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY);
	bmp_radio = gbitmap_create_as_sub_bitmap(bmp_batteryAll, GRect(110, 0, 10, 20));

	//Init b00bs
	b00bs_layer = bitmap_layer_create(GRect(0, 0, png_size.w, png_size.h)); 
	bitmap_layer_set_background_color(b00bs_layer, GColorWhite);

	//Init bluetooth radio
	radio_layer = bitmap_layer_create(GRect(1, bounds.size.h, 10, 20));
	bitmap_layer_set_background_color(radio_layer, GColorClear);
	bitmap_layer_set_bitmap(radio_layer, bmp_radio);
		
	//Init battery
	battery_layer = bitmap_layer_create(GRect(bounds.size.w-11, bounds.size.h, 10, 20)); 
	bitmap_layer_set_background_color(battery_layer, GColorClear);

	//Update Configuration
	update_configuration();

	rc = layer_get_frame(bitmap_layer_get_layer(radio_layer));
	rc.origin.y -= rc.size.h+1;
	layer_set_frame(bitmap_layer_get_layer(radio_layer), rc);
	
	rc = layer_get_frame(bitmap_layer_get_layer(battery_layer));
	rc.origin.y -= rc.size.h+1;
	layer_set_frame(bitmap_layer_get_layer(battery_layer), rc);

	layer_add_child(window_layer, bitmap_layer_get_layer(b00bs_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(radio_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(battery_layer));
	
	b_initialized = true;
}
//-------------------------------------------------------------------------------------------------------------
static void window_unload(Window *window) 
{
	bitmap_layer_destroy(b00bs_layer);
	bitmap_layer_destroy(battery_layer);
	bitmap_layer_destroy(radio_layer);
	fonts_unload_custom_font(digitS);
	gbitmap_destroy(bmp_b00bs);
	gbitmap_destroy(bmp_batt);
	gbitmap_destroy(bmp_radio);
	gbitmap_destroy(bmp_batteryAll);
	gbitmap_sequence_destroy(s_sequence);
	
	if (!b_initialized)
		app_timer_cancel(timer_face);
	if (b_animating)
		app_timer_cancel(timer_b00bs);
	if (b_charging)
		app_timer_cancel(timer_batt);
}
//-------------------------------------------------------------------------------------------------------------
static void init(void) 
{
	b_initialized = b_charging = b_animating = false;
	aktBT = -1;

	char* sLocale = setlocale(LC_TIME, ""), sLang[3];
	if (strncmp(sLocale, "en", 2) == 0)
		strcpy(sLang, "en");
	else if (strncmp(sLocale, "de", 2) == 0)
		strcpy(sLang, "de");
	else if (strncmp(sLocale, "es", 2) == 0)
		strcpy(sLang, "es");
	else if (strncmp(sLocale, "fr", 2) == 0)
		strcpy(sLang, "fr");
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Time locale is set to: %s/%s", sLocale, sLang);

	window = window_create();
	window_set_background_color(window, GColorBlack);
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});

	// Push the window onto the stack
	window_stack_push(window, true);
/*	
	//Subscribe ticks
	tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);

	//Subscribe smart status
	battery_state_service_subscribe(&battery_state_service_handler);
	bluetooth_connection_service_subscribe(&bluetooth_connection_handler);
	
	//Subscribe messages
	app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_open(128, 128);
*/
	timer_b00bs = app_timer_register(10, timerCallback, (void*)TIMER_ANIM_B00BS);
}
//-------------------------------------------------------------------------------------------------------------
static void deinit(void) 
{
	animation_unschedule_all();
	
	app_message_deregister_callbacks();
	tick_timer_service_unsubscribe();
	battery_state_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	
	window_destroy(window);
}
//-------------------------------------------------------------------------------------------------------------
int main(void) {
	init();
	app_event_loop();
	deinit();
}
//-------------------------------------------------------------------------------------------------------------