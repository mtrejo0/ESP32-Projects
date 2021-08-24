#include <SPI.h>
#include "WiFi.h"
#include <WiFiClientSecure.h>
/*
  Sigma Nu Doorbell Code

  Reads a digital input on pin 2 (button). When the button is pressed, a
  message is posted on #house notifying the brothers that there is someone
  at the door.
*/

const int pushButton = 15;
const String slack_hook_url = "/services/T0AQ9C3PG/BGNAHK0S2/V1BzxAAaxlsl6fsr2D3DE41v"; // currently set to #house

class Button{
  public:
  unsigned long t_of_state_2;
  unsigned long t_of_button_change;
  unsigned long debounce_time;
  unsigned long long_press_time;
  int pin;
  int flag;
  bool button_pressed;
  int st;
  Button(int p) {
    flag = 0;  
    st = 0;
    pin = p;
    t_of_state_2 = millis(); //init
    t_of_button_change = millis(); //init
    debounce_time = 10;
    long_press_time = 1000;
    button_pressed = 0;
  }
  void read() {
    int button_state = digitalRead(pin);
    button_pressed = !button_state;
  }
  int update() {
    read();
    flag = 0;
    if (st==0) { // Unpressed, rest state
      if (button_pressed) {
        st = 1;
        t_of_button_change = millis();
      }
    } else if (st==1) { //Tentative pressed
      if (!button_pressed) {
        st = 0;
        t_of_button_change = millis();
      } else if (millis()-t_of_button_change >= debounce_time) {
        st = 2;
        t_of_state_2 = millis();
      }
    } else if (st==2) { // Short press
      if (!button_pressed) {
        st = 4;
        t_of_button_change = millis();
      } else if (millis()-t_of_state_2 >= long_press_time) {
        st = 3;
      }
    } else if (st==3) { //Long press
      if (!button_pressed) {
        st = 4;
        t_of_button_change = millis();
      }
    } else if (st==4) { //Tentative unpressed
      if (button_pressed && millis()-t_of_state_2 < long_press_time) {
        st = 2; // Unpress was temporary, return to short press
        t_of_button_change = millis();
      } else if (button_pressed && millis()-t_of_state_2 >= long_press_time) {
        st = 3; // Unpress was temporary, return to long press
        t_of_button_change = millis();
      } else if (millis()-t_of_button_change >= debounce_time) { // A full button push is complete
        st = 0;
        if (millis()-t_of_state_2 < long_press_time) { // It is a short press
          flag = 1;
        } else {  // It is a long press
          flag = 2;
        }
      }
    }
    return flag;
  }
};

Button button = Button(pushButton);

void setup() {
  pinMode(pushButton, INPUT_PULLUP);
  Serial.begin(115200);
  WiFi.begin("MIT");
  int count = 0;  // count used for WiFi check times
    while (WiFi.status() != WL_CONNECTED && count<6) {
      delay(500);
      Serial.print(".");
      count++;
    }
    delay(2000);
    if (WiFi.isConnected()) {  // print WiFi info
      Serial.println(WiFi.localIP().toString() + " (" + WiFi.macAddress() + ") (" + WiFi.SSID() + ")");
      Serial.println("All Connected");
      delay(500);
    } else {  // restarts ESP if WiFi not connected.
      Serial.println(WiFi.status());
      ESP.restart(); // restart the ESP
    }
}

void loop() {
  int state = button.update();
  if (state > 0){
    ring();
    delay(30000);
  }
  delay(10);        // delay in between reads for stability
}

void ring(){
  const char* host = "hooks.slack.com";
  WiFiClientSecure client; //instantiate a client object
  const int httpsPort = 443;  
  if (client.connect(host, httpsPort)) {    
    String body = "payload={\"text\": \"*There is an alarm at the gate!* <!channel>\",\"attachments\": [{\"blocks\": [{\"type\": \"actions\",\"elements\": [{\"type\": \"button\",\"text\": {\"type\": \"plain_text\",\"emoji\": true,\"text\": \"Attend to the alarm. :rotating_light:\"},\"value\": \"claim\"}]}]}]}";
    
    client.print("POST " + slack_hook_url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Content-Type: application/x-www-form-urlencoded\r\n" +
                 "Connection: close\r\n" +
                 "Content-Length: " + body.length() + "\r\n" +
                 "\r\n" + body);
    String line = client.readStringUntil('\n');
    Serial.printf("Response:");
    Serial.println(line);
  } else {
    Serial.println("connection failed");
    Serial.println("wait 0.5 sec...");
    client.stop();
    delay(300);
  }
}
