#include <Arduino.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
TFT_eSprite spr = TFT_eSprite(&tft);

void init_ST7735()
{
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  spr.fillScreen(TFT_BLACK);

  //  pinMode(16, OUTPUT); //设置IO0模式
  //  digitalWrite(16, HIGH);

}

void ST7735_Clear()
{
  digitalWrite(16, LOW);
  tft.fillScreen(TFT_BLACK);
  spr.fillScreen(TFT_BLACK);
  delay(100);
}

void ST7735_SHOW_TEST(int x, int y, int w, int h, int text_x, int text_y, String str, int str_font, int str_size)
{
  spr.createSprite(w, h); //显示区域 宽和高
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_YELLOW);
  spr.setTextFont(str_font);
  spr.drawString(str, text_x, text_y, str_size);  //写字符串，从显示区域的什么位置开始写
  spr.pushSprite(x, y);  //矩形显示在屏幕的位置
  spr.deleteSprite();
  digitalWrite(16, HIGH);
}

void ROLL_Text(int x, int y, int w, int h, int text_x, int text_y, String str, int str_font, int str_size)
{
  static int16_t x_pos = 0; // 文字横坐标
  //  static char* text = "SERVER:eu1.cloud.thethings.network"; // 要滚动的文字，末尾加多个空格
  static String text = str;
  int16_t textWidth = tft.textWidth(text); // 获取文字宽度
  int16_t screenWidth = tft.width(); // 获取屏幕宽度

  if (x_pos < -textWidth) { // 判断是否超出屏幕左侧
    x_pos = screenWidth; // 重置横坐标
  }

  spr.createSprite(w, h);
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_YELLOW);
  spr.setTextFont(str_font);
  //  tft.setFreeFont(&FreeSerif9pt7b);

  spr.fillRect(x_pos, 0, textWidth, 16, TFT_BLACK); // 擦除文字
  for (int i = 0; i < text.length(); i++) { // 遍历每个字符
    char c = text.charAt(i);
    spr.drawChar(x_pos + i * 8, 0, c, TFT_YELLOW, TFT_BLACK, str_size); // 绘制字符
  }

  x_pos--; // 更新横坐标
  spr.pushSprite(x, y);
  spr.deleteSprite();

  delay(20); // 延时100毫秒
  digitalWrite(16, HIGH);
}

uint16_t touchY[6];
uint16_t touch_pos;
uint16_t error_cont;
bool ST7735_GET_TOUCH()
{
  if (led_show == true)
  {
    digitalWrite(GATEWAY_RXTX_PIN, LOW);
    delay(20);
    digitalWrite(GATEWAY_RXTX_PIN, HIGH);
    delay(20);
    digitalWrite(GATEWAY_RXTX_PIN, LOW);
    delay(20);
    digitalWrite(GATEWAY_RXTX_PIN, HIGH);
    led_show = false;
  }

  //  struct tm timeInfo;
  if (!getLocalTime(&timeInfo))
  {
    Serial.println("setup:: ERROR Time not set Time");
    error_cont++;
    if (error_cont == 10)
    {
      error_cont = 0;

      ST7735_Clear();
      ST7735_SHOW_TEST(0, 75, 128, 20, 3, 0, "ERROR", 3, 2);
      prefs.begin("gatewayConfig", false); //为false才能删除键值
      Serial.println(prefs.freeEntries());//查询清除前的剩余空间

      prefs.remove("netMode");
      prefs.remove("wifiid");
      prefs.remove("wifipass");
      prefs.remove("gatewayid");
      prefs.remove("serverAddr");
      prefs.remove("serverPort");
      prefs.remove("region");
      prefs.remove("getway_channel");
      prefs.remove("getway_sf");
      prefs.remove("getway_utc");
      prefs.clear();

      delay(500);
      Serial.println(prefs.freeEntries());//查询清除后的剩余空间
      prefs.end();

      Serial.println("remove config --> ESP32 restart ");
      ESP.restart(); //重启ESP32

    }
    //    configTime(8 * 3600, 0, "pool.ntp.org");
    if (getway_utc == "UTC")
      configTime(0 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+1")
      configTime(1 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+2")
      configTime(2 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+3")
      configTime(3 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+4")
      configTime(4 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+5")
      configTime(5 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+6")
      configTime(6 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+7")
      configTime(7 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+8")
      configTime(8 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+9")
      configTime(9 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+10")
      configTime(10 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+11")
      configTime(11 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC+12")
      configTime(12 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-1")
      configTime(-1 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-2")
      configTime(-2 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-3")
      configTime(-3 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-4")
      configTime(-4 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-5")
      configTime(-5 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-6")
      configTime(-6 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-7")
      configTime(-7 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-8")
      configTime(-8 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-9")
      configTime(-9 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-10")
      configTime(-10 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-11")
      configTime(-11 * 3600, 0, "pool.ntp.org");
    else if (getway_utc == "UTC-12")
      configTime(-12 * 3600, 0, "pool.ntp.org");
  }
  else {
    String time_str = String(timeInfo.tm_year + 1900); //年
    time_str += "-";
    time_str += String(timeInfo.tm_mon + 1); //月
    time_str += "-";
    time_str += String(timeInfo.tm_mday); //日
    time_str += " ";
    if (timeInfo.tm_hour < 10)
      time_str += ("0" + String(timeInfo.tm_hour));
    else
      time_str += String(timeInfo.tm_hour);
    time_str += ":";
    if (timeInfo.tm_min < 10)
      time_str += ("0" + String(timeInfo.tm_min));
    else
      time_str += String(timeInfo.tm_min);
    time_str += ":";
    if (timeInfo.tm_sec < 10)
      time_str += ("0" + String(timeInfo.tm_sec));
    else
      time_str += String(timeInfo.tm_sec);
    sys_time = time_str;
  }

  uint16_t touchX_now = 0, touchY_now = 0;

  bool touched = tft.getTouch( &touchX_now, &touchY_now, 600);
  if ( touched )
  {
    if (touch_pos == 5)
    {
      //      Serial.println("touchY ---->> ");

      int i, j;
      uint16_t temp;
      for (i = 0; i < 6 - 1; i++) {
        for (j = 0; j < 6 - 1 - i; j++) {
          if (touchY[j] > touchY[j + 1]) {
            temp = touchY[j];
            touchY[j] = touchY[j + 1];
            touchY[j + 1] = temp;
          }
        }
      }

      //      for (i = 0; i < 5; i++)
      //      {
      //        Serial.print(touchY[i]);
      //        Serial.print("  ");
      //      }
      //
      //      Serial.println("end");

      if (abs(touchY[5] - touchY[0]) > 20)
      {
        if (show_flag == true)
        {
          ST7735_Clear();
          show_flag = false;
        }
        else
        {
          ST7735_Clear();
          show_flag = true;
        }
      }

      touch_pos = 0;
      touchY[0] = 0;
      touchY[1] = 0;
      touchY[2] = 0;
      touchY[3] = 0;
      touchY[4] = 0;
      touchY[5] = 0;

      delay(30);
      return true;
    }

    touchY[touch_pos] = touchY_now;
    touch_pos++;


    if (show_flag == true)
    {
      ST7735_SHOW_TEST(0, 25, 128, 12, 10, 0, sys_time, 3, 1);
      ST7735_SHOW_TEST(0, 45, 128, 12, 10, 0, "SERVER:", 3, 1);
      ROLL_Text(0, 60, 128, 12, 0, 0, serverAddr, 3, 1);

      if (mode_getwag == "net")
      {
        IPAddress ip = ETH.localIP();
        ST7735_SHOW_TEST(0, 75, 128, 12, 10, 0, "IP:" + ip.toString() , 3, 1);
      }
      else if (mode_getwag == "wifi")
      {
        IPAddress ip =  WiFi.localIP();
        ST7735_SHOW_TEST(0, 75, 128, 12, 10, 0, "IP:" + ip.toString(), 3, 1);
      }

      ST7735_SHOW_TEST(0, 90, 128, 12, 10, 0, "ID:" + gatewayid, 3, 1);
      ST7735_SHOW_TEST(0, 105, 128, 12, 10, 0, "REGION:" + region, 3, 1);
      ST7735_SHOW_TEST(0, 120, 128, 12, 10, 0, "RX: " + String(Rx_cont), 3, 1);
      ST7735_SHOW_TEST(0, 135, 128, 12, 10, 0, "TX: " + String(Tx_cont), 3, 1);
    }
    else
    {
      String str1 = "Last Tx:";
      String str2 = "Last Rx:";

      ST7735_SHOW_TEST(0, 35, 128, 12, 10, 0, str1, 3, 1);
      ST7735_SHOW_TEST(0, 60, 128, 12, 10, 0, Tx_last_time, 3, 1);

      ST7735_SHOW_TEST(0, 85, 128, 12, 10, 0, str2, 3, 1);
      ST7735_SHOW_TEST(0, 110, 128, 12, 10, 0, Rx_last_time, 3, 1);

    }


    //    ST7735_SHOW_TEST(0, 150, 110, 12, 5, 1, "X=" + String(touchX_now) + " Y=" + String(touchY_now), 3, 1);
    return true;

  }

}
