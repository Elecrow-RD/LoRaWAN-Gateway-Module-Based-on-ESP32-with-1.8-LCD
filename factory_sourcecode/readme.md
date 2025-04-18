### 

## Key Functions in the Arduino Program

- # Function Descriptions

  ## `setup()`
  The `setup()` function is called once when the program starts. It initializes the serial communication, TFT display, touchscreen, and LVGL library. It also sets up the display driver, touchscreen driver, and initializes the UI.

  ## `loop()`
  The `loop()` function is called repeatedly and handles the main program logic. It checks for serial input to determine if the test program should run and handles the LVGL task handler to update the display.

  ## `my_disp_flush()`
  This function is a callback used by LVGL to flush the display buffer to the TFT display. It writes the pixel data to the display.

  ## `my_touchpad_read()`
  This function is a callback used by LVGL to read the touchscreen input. It retrieves the touch coordinates and updates the touch state.

  ## `touch_calibrate()`
  This function is used to calibrate the touchscreen. It prompts the user to touch specific corners of the screen and adjusts the calibration data accordingly.

  ## `label_xy()`
  This function creates label objects on the TOUCH screen to display the touch coordinates.

  ## `lv_example_bar()`
  This function creates a progress bar on the MENU screen and sets its initial value.

  ## `callback1()`
  This function is a callback used by the Ticker library to update the progress bar value and label text.

  ## `clearBufferArray()`
  This function clears the serial buffer array by setting all elements to NULL.

  ## `SD_test()`
  This function initializes the SD card and prints the directory structure to the serial monitor.

  ## `Ce_shi()`
  This function contains the test program logic. It listens for serial commands and performs various display tests such as showing colored screens, testing the touchscreen, and initializing the SD card.

  ## `lv_timer_handler()`
  This function is called to handle LVGL timer events, which are used to update the UI and handle animations.

  ## `watchdog_reboot()`
  This function reboots the system using the hardware watchdog timer.

  ## `pinMode()`
  This function sets the mode of a pin to either INPUT, OUTPUT, or INPUT_PULLUP.

  ## `digitalWrite()`
  This function writes a HIGH or LOW value to a digital pin.

  ## `analogWrite()`
  This function writes an analog value (PWM wave) to a pin.

  ## `delay()`
  This function pauses the program for the specified number of milliseconds.

  ## `Serial.begin()`
  This function initializes the serial communication with the specified baud rate.

  ## `Serial.read()`
  This function reads the next byte from the serial buffer.

  ## `Serial.println()`
  This function prints data to the serial port followed by a carriage return and newline.

  ## `tft.begin()`
  This function initializes the TFT display.

  ## `tft.setRotation()`
  This function sets the rotation of the TFT display.

  ## `tft.setTouch()`
  This function sets the touchscreen calibration data.

  ## `tft.fillScreen()`
  This function fills the entire TFT display with a specified color.

  ## `tft.getTouch()`
  This function retrieves the touch coordinates from the touchscreen.

  ## `lv_init()`
  This function initializes the LVGL library.

  ## `lv_disp_draw_buf_init()`
  This function initializes the display buffer used by LVGL.

  ## `lv_disp_drv_register()`
  This function registers the display driver with LVGL.

  ## `lv_indev_drv_register()`
  This function registers the input device driver with LVGL.

  ## `ui_init()`
  This function initializes the UI elements and screens.

  ## `lv_scr_load_anim()`
  This function loads a screen with an animation effect.

  ## `lv_obj_invalidate()`
  This function invalidates an object, causing it to be redrawn.

  ## `pinMode()`
  This function sets the mode of a pin to either INPUT, OUTPUT, or INPUT_PULLUP.

  ## `digitalWrite()`
  This function writes a HIGH or LOW value to a digital pin.

  ## `analogWrite()`
  This function writes an analog value (PWM wave) to a pin.

  ## `delay()`
  This function pauses the program for the specified number of milliseconds.

  ## `Serial.begin()`
  This function initializes the serial communication with the specified baud rate.

  ## `Serial.read()`
  This function reads the next byte from the serial buffer.

  ## `Serial.println()`
  This function prints data to the serial port followed by a carriage return and newline.

  ## `tft.begin()`
  This function initializes the TFT display.

  ## `tft.setRotation()`
  This function sets the rotation of the TFT display.

  ## `tft.setTouch()`
  This function sets the touchscreen calibration data.

  ## `tft.fillScreen()`
  This function fills the entire TFT display with a specified color.

  ## `tft.getTouch()`
  This function retrieves the touch coordinates from the touchscreen.

  ## `lv_init()`
  This function initializes the LVGL library.

  ## `lv_disp_draw_buf_init()`
  This function initializes the display buffer used by LVGL.

  ## `lv_disp_drv_register()`
  This function registers the display driver with LVGL.

  ## `lv_indev_drv_register()`
  This function registers the input device driver with LVGL.

  ## `ui_init()`
  This function initializes the UI elements and screens.

  ## `lv_scr_load_anim()`
  This function loads a screen with an animation effect.

  ## `lv_obj_invalidate()`
  This function invalidates an object, causing it to be redrawn.