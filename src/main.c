#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
	
#include "config.h"	
#include "http.h"
#include "util.h"
#include "mini-printf.h"

// Define Universal Unique Identifier
// 9141B628-BC89-498E-B147-049F49B1AD66
#define MY_HTTP_UUID { 0x91, 0x41, 0xB6, 0x28, 0xBC, 0x89, 0x49, 0x8E, 0xB1, 0x47, 0x04, 0x9F, 0x49, 0xB1, 0xAD, 0x66 }
PBL_APP_INFO(HTTP_UUID,
             "Gas Finder", "Matt Donders",
             1, 0, /* App version */
             RESOURCE_ID_APPICON,
             APP_INFO_STANDARD_APP);
	
#define TITLE_DIST_FRAME	(GRect(2, 2, 141, 16))
#define GAS_DIST_FRAME		(GRect(0, 18, 144, 58))
#define GAS_DIST_TEXT		(GRect(2, 18, 141, 34))
#define GAS_DIST_DETL		(GRect(2, 52, 141, 22))

#define TITLE_PRICE_FRAME	(GRect(2, 78, 141, 16))
#define GAS_PRICE_FRAME		(GRect(0, 94, 144, 60))
#define GAS_PRICE_TEXT		(GRect(2, 94, 141, 36))
#define GAS_PRICE_DETL		(GRect(2, 130, 141, 22))

// POST variables
#define WEATHER_KEY_LATITUDE 1
#define WEATHER_KEY_LONGITUDE 2
#define KEY_REQUEST_TYPE 3

static int our_latitude, our_longitude;
static bool located = false;
char *textlayerMain, *textlayerDetl;

/* ARRAY DEFINITION

1: By Distance | Regular Price Whole
2: By Distance | Regular Price Decimal
3: By Distance | Premium Price Whole
4: By Distance | Premium Price Decimal
5: By Distance | Diesel Price Whole (999 = Error)
6: By Distance | Diesel Price Decimal (999 = Error)
7: By Distance | Distance Whole
8: By Distance | Distance Decimal
9: By Distance | Direction Numeric
	0:  N   1:  NNE   2:  NE   3:  ENE 
	4:  E   5:  ESE   6:  SE   7:  SSE
	8:  S   9:  SSW   10: SW   11: WSW 
	12: W   13: WNW   14: NW   15: NNW
10: By Distance | Station Numeric (See README)
	
	
101: By Price | Regular Price Whole
102: By Price | Regular Price Decimal
103: By Price | Premium Price Whole
104: By Price | Premium Price Decimal
105: By Price | Diesel Price Whole (999 = Error)
106: By Price | Diesel Price Decimal (999 = Error)
107: By Price | Distance Whole
108: By Price | Distance Decimal
109: By Price | Direction Numeric
	0:  N   1:  NNE   2:  NE   3:  ENE 
	4:  E   5:  ESE   6:  SE   7:  SSE
	8:  S   9:  SSW   10: SW   11: WSW 
	12: W   13: WNW   14: NW   15: NNW
110: By Price | Station Numeric (See README)
*/


// Received variables
#define KEY_STATION_MAIN 1
#define KEY_STATION_DETL 2
#define KEY_STATION_TIME 3
#define KEY_ERROR_MAIN 97
#define KEY_ERROR_API 98
#define KEY_ERROR_DETL 99
	
Window window;
TextLayer timeLayer;

TextLayer titleDistLayer;
TextLayer titleDistTime;
TextLayer gasDistBG;
TextLayer gasDistText;
TextLayer gasDistDetl;

TextLayer titlePriceLayer;
TextLayer titlePriceTime;
TextLayer gasPriceBG;
TextLayer gasPriceText;
TextLayer gasPriceDetl;

static char strDistMain[30];
static char strDistDetl[25];
static char strDistTime[10];
static char strPriceMain[30];
static char strPriceDetl[25];
static char strPriceTime[10];

// HTTPebble Initialization
void request_gas(int8_t requestTypeNum);

void failed(int32_t cookie, int http_status, void* context) {
	if (cookie == 0 || cookie == DIST_GAS_COOKIE || cookie == PRICE_GAS_COOKIE) {
		text_layer_set_text(&gasDistText, "Network Error");
		text_layer_set_text(&gasPriceText, "Select to retry!");
	}
	
	//Re-request the location
	located = false;
}

void success(int32_t cookie, int http_status, DictionaryIterator* received, void* context) {
	if(cookie != DIST_GAS_COOKIE || cookie != PRICE_GAS_COOKIE) {
		//return;
	}
	
	if(cookie == DIST_GAS_COOKIE) {
		Tuple *station_main_tuple = dict_find(received, KEY_STATION_MAIN);
		Tuple *station_detl_tuple = dict_find(received, KEY_STATION_DETL);
		Tuple *station_time_tuple = dict_find(received, KEY_STATION_TIME);

		if(station_main_tuple) {
			//text_layer_set_text(&gasDistText, "Received!");
			memcpy(strDistMain, station_main_tuple->value->cstring, station_main_tuple->length);
			text_layer_set_text(&gasDistText, strDistMain);
		}
	
		if(station_detl_tuple) {
			memcpy(strDistDetl, station_detl_tuple->value->cstring, station_detl_tuple->length);
			text_layer_set_text(&gasDistDetl, strDistDetl);
		}
		
		if(station_time_tuple) {
			memcpy(strDistTime, station_time_tuple->value->cstring, station_time_tuple->length);
			text_layer_set_text(&titleDistTime, strDistTime);
		}
		
		// Request Cheapest Gas
		request_gas(2);
	}

	
	if(cookie == PRICE_GAS_COOKIE) {
		Tuple *station_main_tuple = dict_find(received, KEY_STATION_MAIN);
		Tuple *station_detl_tuple = dict_find(received, KEY_STATION_DETL);
		Tuple *station_time_tuple = dict_find(received, KEY_STATION_TIME);

		if(station_main_tuple) {
			//text_layer_set_text(&gasDistText, "Received!");
			memcpy(strPriceMain, station_main_tuple->value->cstring, station_main_tuple->length);
			text_layer_set_text(&gasPriceText, strPriceMain);
		}
	
		if(station_detl_tuple) {
			memcpy(strPriceDetl, station_detl_tuple->value->cstring, station_detl_tuple->length);
			text_layer_set_text(&gasPriceDetl, strPriceDetl);
		}
		
		if(station_time_tuple) {
			memcpy(strPriceTime, station_time_tuple->value->cstring, station_time_tuple->length);
			text_layer_set_text(&titlePriceTime, strPriceTime);
		}
	}
	
	// Error Handling
	Tuple *error_main_tuple = dict_find(received, KEY_ERROR_MAIN);
	Tuple *error_api_tuple = dict_find(received, KEY_ERROR_API);
	Tuple *error_detl_tuple = dict_find(received, KEY_ERROR_DETL);
	
	if(error_main_tuple) {
		text_layer_set_text(&gasDistText, "Error: Main Feed!");
	}
	
	if(error_detl_tuple) {
		text_layer_set_text(&gasDistDetl, "Error: Detail Feed!");
	}
	
	if(error_api_tuple) {
		text_layer_set_text(&gasPriceText, "Error: API Return!");
	}
}

void location(float latitude, float longitude, float altitude, float accuracy, void* context) {
	// Fix the floats
	our_latitude = latitude * 10000;
	our_longitude = longitude * 10000;
	located = true;
	request_gas(1);
}

void reconnect(void* context) {
  	located = false;
	request_gas(1);
}


// Standard app initialisation

void handle_init(AppContextRef ctx) {
  (void)ctx;
	
  // CODE SETUP: HTTP Parameter Setup
  http_set_app_id(341566297);
  http_register_callbacks((HTTPCallbacks){
		.failure=failed,
		.success=success,
		.reconnect=reconnect,
	    .location=location
	}, (void*)ctx);

  window_init(&window, "Pebble Window");
  window_stack_push(&window, true /* Animated */);
	
  resource_init_current_app(&APP_RESOURCES);
	
  // UI SETUP: Background Layers
  text_layer_init(&gasDistBG, GAS_DIST_FRAME);
  text_layer_set_background_color(&gasDistBG, GColorBlack);
  layer_add_child(&window.layer, &gasDistBG.layer);
	
  text_layer_init(&gasPriceBG, GAS_PRICE_FRAME);
  text_layer_set_background_color(&gasPriceBG, GColorBlack);
  layer_add_child(&window.layer, &gasPriceBG.layer);
	

  // UI SETUP: Text Layers	
  // Top Title Layer (Closest Gas)
  text_layer_init(&titleDistLayer, TITLE_DIST_FRAME);
  text_layer_set_background_color(&titleDistLayer, GColorClear);
  text_layer_set_text(&titleDistLayer, "Closest Gas");
  text_layer_set_font(&titleDistLayer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(&titleDistLayer, GTextAlignmentLeft);
  layer_add_child(&window.layer, &titleDistLayer.layer);
	
  // Top Update Layer (Closest Gas)
  text_layer_init(&titleDistTime, TITLE_DIST_FRAME);
  text_layer_set_background_color(&titleDistTime, GColorClear);
  text_layer_set_text(&titleDistTime, "(x days)");
  text_layer_set_font(&titleDistTime, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(&titleDistTime, GTextAlignmentRight);
  layer_add_child(&window.layer, &titleDistTime.layer);
	
  // Closet Gas Layer
  text_layer_init(&gasDistText, GAS_DIST_TEXT);
  text_layer_set_background_color(&gasDistText, GColorBlack);
  text_layer_set_text_color(&gasDistText, GColorWhite);
  text_layer_set_text(&gasDistText, "Wait...");
  text_layer_set_font(&gasDistText, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(&gasDistText, GTextAlignmentLeft);
  layer_add_child(&window.layer, &gasDistText.layer);
	
  // Closet Gas Layer (DETAIL)
  text_layer_init(&gasDistDetl, GAS_DIST_DETL);
  text_layer_set_background_color(&gasDistDetl, GColorBlack);
  text_layer_set_text_color(&gasDistDetl, GColorWhite);
  text_layer_set_text(&gasDistDetl, "$X.XX | $X.XX | $X.XX");
  text_layer_set_font(&gasDistDetl, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(&gasDistDetl, GTextAlignmentCenter);
  layer_add_child(&window.layer, &gasDistDetl.layer);
	
  // Bottom Title Layer (Cheapest Gas)
  text_layer_init(&titlePriceLayer, TITLE_PRICE_FRAME);
  text_layer_set_text(&titlePriceLayer, "Cheapest Gas");
  text_layer_set_font(&titlePriceLayer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(&titlePriceLayer, GTextAlignmentLeft);
  layer_add_child(&window.layer, &titlePriceLayer.layer);
	
  // Bottom Time Layer (Cheapest Gas)
  text_layer_init(&titlePriceTime, TITLE_PRICE_FRAME);
  text_layer_set_background_color(&titlePriceTime, GColorClear);
  text_layer_set_text(&titlePriceTime, "(x days)");
  text_layer_set_font(&titlePriceTime, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(&titlePriceTime, GTextAlignmentRight);
  layer_add_child(&window.layer, &titlePriceTime.layer);
	
  // Cheapest Gas Layer
  text_layer_init(&gasPriceText, GAS_PRICE_TEXT);
  text_layer_set_background_color(&gasPriceText, GColorBlack);
  text_layer_set_text_color(&gasPriceText, GColorWhite);
  text_layer_set_text(&gasPriceText, "Wait...");
  text_layer_set_font(&gasPriceText, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(&gasPriceText, GTextAlignmentLeft);
  layer_add_child(&window.layer, &gasPriceText.layer);

  // Cheapest Gas Layer (DETAIL)
  text_layer_init(&gasPriceDetl, GAS_PRICE_DETL);
  text_layer_set_background_color(&gasPriceDetl, GColorBlack);
  text_layer_set_text_color(&gasPriceDetl, GColorWhite);
  text_layer_set_text(&gasPriceDetl, "$X.XX | $X.XX | $X.XX");
  text_layer_set_font(&gasPriceDetl, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(&gasPriceDetl, GTextAlignmentCenter);
  layer_add_child(&window.layer, &gasPriceDetl.layer);

  // Time Layer - Top Right
  text_layer_init(&timeLayer, TITLE_DIST_FRAME);
  text_layer_set_background_color(&timeLayer, GColorClear);
  // text_layer_set_text(&timeLayer, "(02:24)");
  text_layer_set_font(&timeLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(&timeLayer, GTextAlignmentRight);
  layer_add_child(&window.layer, &timeLayer.layer);
	
  request_gas(1);
}

void pbl_main(void *params) {	
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
	 			
	.messaging_info = {
			.buffer_sizes = {
				.inbound = 124,
				.outbound = 256,
			}
		},
  };
  app_event_loop(params, &handlers);
}

void request_gas(int8_t requestTypeNum) {
	// 1 = Distance
	// 2 = Price
	
	if(!located) {
		http_location_request();
		return;
	}
	
	if (requestTypeNum == 1) {
		// Distance Request
		
		// Build the HTTP request
		DictionaryIterator *body;
		HTTPResult result = http_out_get(GASURL, DIST_GAS_COOKIE, &body);
		if(result == HTTP_BUSY) {
			text_layer_set_text(&gasDistText, "HTTP BUSY ERROR");
		}
		else if(result != HTTP_OK) {
			text_layer_set_text(&gasDistText, "HTTP GET ERROR");
			return; // Don't care - send it anyway.
		}
		
		dict_write_int32(body, WEATHER_KEY_LATITUDE, our_latitude);
		dict_write_int32(body, WEATHER_KEY_LONGITUDE, our_longitude);
		dict_write_cstring(body, KEY_REQUEST_TYPE, REQ_DIST);
	
		// Now that it's built, send it.
		if(http_out_send() != HTTP_OK) {
			text_layer_set_text(&gasDistText, "Error - N/A");
			text_layer_set_text(&gasDistDetl, "N/A | N/A | N/A");			
			return; // Don't care - want to test it anyway.
		}
		else {
			text_layer_set_text(&gasDistText, "Searching...");
			text_layer_set_text(&gasDistDetl, "$X.XX | $X.XX | $X.XX");
		}
	}
	
	else if (requestTypeNum == 2) {
		// Price Request
		
		// Build the HTTP request
		DictionaryIterator *body;
		HTTPResult result = http_out_get(GASURL, PRICE_GAS_COOKIE, &body);
		if(result == HTTP_BUSY) {
			text_layer_set_text(&gasPriceText, "HTTP BUSY ERROR");
		}
		else if(result != HTTP_OK) {
			text_layer_set_text(&gasPriceText, "HTTP GET ERROR");
			return; // Don't care - send it anyway.
		}
		
		dict_write_int32(body, WEATHER_KEY_LATITUDE, our_latitude);
		dict_write_int32(body, WEATHER_KEY_LONGITUDE, our_longitude);
		dict_write_cstring(body, KEY_REQUEST_TYPE, REQ_PRICE);
	
		// Now that it's built, send it.
		if(http_out_send() != HTTP_OK) {
			text_layer_set_text(&gasPriceText, "Error - N/A");
			text_layer_set_text(&gasPriceDetl, "N/A | N/A | N/A");
			
			return; // Don't care - want to test it anyway.
		}
		else {
			text_layer_set_text(&gasPriceText, "Searching...");
			text_layer_set_text(&gasPriceDetl, "$X.XX | $X.XX | $X.XX");
		}
	}
}