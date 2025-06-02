#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui.h"
#include <sps30.h>

#define LVGL_TICK_PERIOD 5
#define BUZZER_PIN 15

static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10]; 

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

void sps_init() {
    int16_t ret;
    uint8_t auto_clean_days = 4;

    delay(2000);
    sensirion_i2c_init();

    while (sps30_probe() != 0) {
        Serial.print("SPS sensor probing failed\n");
        delay(500);
    }

    Serial.print("SPS sensor probing successful\n");

    ret = sps30_set_fan_auto_cleaning_interval_days(auto_clean_days);
    if (ret) {
        Serial.print("error setting the auto-clean interval: ");
        Serial.println(ret);
    }

    ret = sps30_start_measurement();
    if (ret < 0) {
        Serial.print("error starting measurement\n");
    }
}

void sps_read(float* pm25, float* pm10){
  struct sps30_measurement m;
  char serial[SPS30_MAX_SERIAL_LEN];
  uint16_t data_ready;
  int16_t ret;

  do {
    ret = sps30_read_data_ready(&data_ready);
    if (ret < 0) {
      Serial.print("error reading data-ready flag: ");
      Serial.println(ret);
    } else if (!data_ready)
      Serial.print("data not ready, no new measurement available\n");
    else
      break;
    delay(100); 
  } while (1);

  ret = sps30_read_measurement(&m);
  if (ret < 0) {
    Serial.print("error reading measurement\n");
  } else {
    Serial.print("PM  2.5: ");
    Serial.println(m.mc_2p5);
    *pm25 = m.mc_2p5;
    Serial.print("PM 10.0: ");
    Serial.println(m.mc_10p0);
    *pm10 = m.mc_10p0;
  }
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

void SPStimer(lv_timer_t * timer) {
    float pm25 = 0;
    float pm10 = 0;

    sps_read(&pm25, &pm10);

    if(pm25 >= 55.0 || pm10 >= 75.0){
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }

    static char pm25_buf[16];
    static char pm10_buf[16];
    snprintf(pm25_buf, sizeof(pm25_buf), "%.1f ug/m3", pm25);
    snprintf(pm10_buf, sizeof(pm10_buf), "%.1f ug/m3", pm10);

    lv_label_set_text(ui_pm25val, pm25_buf);
    lv_label_set_text(ui_pm10val, pm10_buf);

    lv_chart_set_next_value(ui_History_Chart, ui_History_Chart_series_1, pm25);
    lv_chart_set_next_value(ui_History_Chart, ui_History_Chart_series_2, pm10);
    lv_chart_refresh(ui_History_Chart);
}

void setup() {
    Serial.begin(115200);

    pinMode(BUZZER_PIN, OUTPUT);
    lv_init();
    sps_init();
    tft.begin();
    tft.setRotation(1);

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    ui_init();  

    lv_timer_create(SPStimer, 5000, NULL);  
}

void loop() {
    lv_tick_inc(LVGL_TICK_PERIOD);  
    lv_timer_handler();             
    delay(LVGL_TICK_PERIOD);        
}