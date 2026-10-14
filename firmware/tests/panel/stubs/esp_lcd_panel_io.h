#pragma once
#include <stddef.h>
#include "esp_err.h"
typedef void *esp_lcd_panel_handle_t;
typedef void *esp_lcd_panel_io_handle_t;
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t io,int command,const void *data,size_t size);
