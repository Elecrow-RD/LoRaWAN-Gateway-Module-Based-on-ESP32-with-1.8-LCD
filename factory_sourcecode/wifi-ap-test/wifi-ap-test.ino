#include <WiFi.h>

// 1.The Wi-Fi name and password you want your phone to see
const char* AP_SSID     = "ESP32_AP";
const char* AP_PASSWORD = "aaaabbbb";   

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32 Soft-AP demo");

  // 2. Start AP: Channel 6, not hidden, maximum number of connections 4
  WiFi.softAP(AP_SSID, AP_PASSWORD, 6, false, 4);

  // 3. Print AP IP (gateway)
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  // No action is required here; once the phone is connected, it will obtain an IP address in the 192.168.4.x range.
}
