#include <driver/gpio.h>
#include <driver/sdmmc_host.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/unistd.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

// SD card pins
#define SD_PIN_CLK GPIO_NUM_14
#define SD_PIN_CMD GPIO_NUM_13
#define SD_PIN_D0 GPIO_NUM_12
#define SD_PIN_D3 GPIO_NUM_9

// LCD pins
#define LCD_PIN_DC GPIO_NUM_35
#define LCD_PIN_WR GPIO_NUM_8  // unused, WR is connected to ground
#define LCD_PIN_EN GPIO_NUM_36
#define LCD_PIN_D0 GPIO_NUM_18
#define LCD_PIN_D1 GPIO_NUM_7
#define LCD_PIN_D2 GPIO_NUM_17
#define LCD_PIN_D3 GPIO_NUM_6
#define LCD_PIN_D4 GPIO_NUM_16
#define LCD_PIN_D5 GPIO_NUM_5
#define LCD_PIN_D6 GPIO_NUM_15
#define LCD_PIN_D7 GPIO_NUM_4

/**
 * LCD command macros "borrowed" from the LiquidCrystal arduino library
 * https://github.com/arduino-libraries/LiquidCrystal/blob/master/src/LiquidCrystal.h
 */

// commands
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// flags for display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags for display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// flags for display/cursor shift
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE 0x00
#define LCD_MOVERIGHT 0x04
#define LCD_MOVELEFT 0x00

// flags for function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// make sure to change these if working with an LCD that is not 20x4
const uint8_t rowOffsets[] = {0x00, 0x40, 0x14, 0x54};
FILE* dataFile;

void sendByte(uint8_t data) {
  gpio_set_level(LCD_PIN_D0, data & 1);
  gpio_set_level(LCD_PIN_D1, (data >> 1) & 1);
  gpio_set_level(LCD_PIN_D2, (data >> 2) & 1);
  gpio_set_level(LCD_PIN_D3, (data >> 3) & 1);
  gpio_set_level(LCD_PIN_D4, (data >> 4) & 1);
  gpio_set_level(LCD_PIN_D5, (data >> 5) & 1);
  gpio_set_level(LCD_PIN_D6, (data >> 6) & 1);
  gpio_set_level(LCD_PIN_D7, (data >> 7) & 1);

  gpio_set_level(LCD_PIN_EN, 0);
  esp_rom_delay_us(1);
  gpio_set_level(LCD_PIN_EN, 1);
  esp_rom_delay_us(1);
  gpio_set_level(LCD_PIN_EN, 0);
  esp_rom_delay_us(150);
}

void sendData(uint8_t* data, size_t size) {
  gpio_set_level(LCD_PIN_DC, 1);
  for (size_t i = 0; i < size; i++) sendByte(data[i]);
}

void sendCommand(uint8_t cmd) {
  gpio_set_level(LCD_PIN_DC, 0);
  sendByte(cmd);
}

void defineChar(uint8_t location, uint8_t* charMap) {
  sendCommand(LCD_SETCGRAMADDR | (location << 3));
  sendData(charMap, 8);
}

void clearDisplay() {
  sendCommand(LCD_CLEARDISPLAY);
  esp_rom_delay_us(2000);
}

void initSD() {
  esp_vfs_fat_sdmmc_mount_config_t mountConfig = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024,
  };

  sdmmc_card_t* card;
  const char mount_point[] = "/sdcard";

  sdmmc_host_t hostConfig = SDMMC_HOST_DEFAULT();
  hostConfig.slot = SDMMC_HOST_SLOT_1;
  hostConfig.flags = SDMMC_HOST_FLAG_1BIT;
  hostConfig.max_freq_khz = SDMMC_FREQ_DEFAULT;

  sdmmc_slot_config_t slotConfig = {
      .clk = GPIO_NUM_14,
      .cmd = GPIO_NUM_13,
      .d0 = GPIO_NUM_12,
      .d1 = GPIO_NUM_11,
      .d2 = GPIO_NUM_10,
      .d3 = GPIO_NUM_9,
      .d4 = GPIO_NUM_NC,
      .d5 = GPIO_NUM_NC,
      .d6 = GPIO_NUM_NC,
      .d7 = GPIO_NUM_NC,
      .cd = GPIO_NUM_NC,
      .wp = GPIO_NUM_NC,
      .width = 1,
      .flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP,
  };

  gpio_pullup_en(GPIO_NUM_9);

  esp_vfs_fat_sdmmc_mount(mount_point, &hostConfig, &slotConfig, &mountConfig, &card);

  dataFile = fopen("/sdcard/lcd-bapple/data.bin", "r");

  vTaskDelay(100);
}

void initLCD() {
  const gpio_config_t config = {.pin_bit_mask = 1ULL << LCD_PIN_DC | 1ULL << LCD_PIN_WR | 1ULL << LCD_PIN_EN |
                                                1ULL << LCD_PIN_D0 | 1ULL << LCD_PIN_D1 | 1ULL << LCD_PIN_D2 |
                                                1ULL << LCD_PIN_D3 | 1ULL << LCD_PIN_D4 | 1ULL << LCD_PIN_D5 |
                                                1ULL << LCD_PIN_D6 | 1ULL << LCD_PIN_D7,
                                .mode = GPIO_MODE_INPUT_OUTPUT};

  gpio_config(&config);

  uint8_t displayFunction = LCD_8BITMODE | LCD_2LINE | LCD_5x8DOTS;
  uint8_t displayControl = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;

  gpio_set_level(LCD_PIN_WR, 0);
  gpio_set_level(LCD_PIN_DC, 0);

  sendCommand(LCD_FUNCTIONSET | displayFunction);
  esp_rom_delay_us(4500);
  sendCommand(LCD_FUNCTIONSET | displayFunction);
  esp_rom_delay_us(300);
  sendCommand(LCD_FUNCTIONSET | displayFunction);
  sendCommand(LCD_FUNCTIONSET | displayFunction);
  sendCommand(LCD_DISPLAYCONTROL | displayControl);
  clearDisplay();
  sendCommand(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);
}

void setRow(uint8_t row) { sendCommand(LCD_SETDDRAMADDR | rowOffsets[row]); }

extern "C" {
void app_main(void) {
  initSD();
  initLCD();

  sendData((uint8_t*)"it arrives", 11);

  vTaskDelay(1000 / portTICK_PERIOD_MS);

  uint64_t lastTime = esp_timer_get_time();
  while (true) {
    uint64_t curTime = esp_timer_get_time();
    if (curTime - lastTime <= 133333) continue;  // 133.333ms -> delay between frames in a 7.5fps video
    lastTime = curTime;

    for (uint8_t row = 0; row < 4; row++) {
      // load character data from the SD for this row
      uint8_t charMap[8][8];
      fread(&charMap, 1, sizeof(charMap), dataFile);
      for (uint8_t col = 0; col < 8; col++) {
        defineChar(col, charMap[col]);
      }

      // print out the row
      // NOTE: the display could be cleared before loading the new character data to prevent characters from a row
      // faintly appearing on the row before it, but this also makes the image harder to see as nothing will be
      // displayed while the characters for the new row are being defined (which takes a long time)
      clearDisplay();
      setRow(row);
      uint8_t rowData[] = {0, 1, 2, 3, 4, 5, 6, 7};
      sendData(rowData, 8);

      // delay between rows to avoid ghosting
      vTaskDelay(2);
    }

    // skip 7 frames
    // the video data provided in the repo was made for 60fps in mind, but I had to reduce it to 7.5fps (60 / 8) since
    // writing all of the characters and then displaying them while trying to avoid excessive ghosting takes a lot of
    // time (~125ms)
    fseek(dataFile, 1792, SEEK_CUR);
  }
}
}