#include <SPI.h>
#include "WiFi.h"
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
/*
  Sigma Nu Doorbell Code

  Reads a digital input on pin 2 (button). When the button is pressed, a
  message is posted on #house notifying the brothers that there is someone
  at the door.
*/
class Button {
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
      if (st == 0) { // Unpressed, rest state
        if (button_pressed) {
          st = 1;
          t_of_button_change = millis();
        }
      } else if (st == 1) { //Tentative pressed
        if (!button_pressed) {
          st = 0;
          t_of_button_change = millis();
        } else if (millis() - t_of_button_change >= debounce_time) {
          st = 2;
          t_of_state_2 = millis();
        }
      } else if (st == 2) { // Short press
        if (!button_pressed) {
          st = 4;
          t_of_button_change = millis();
        } else if (millis() - t_of_state_2 >= long_press_time) {
          st = 3;
        }
      } else if (st == 3) { //Long press
        if (!button_pressed) {
          st = 4;
          t_of_button_change = millis();
        }
      } else if (st == 4) { //Tentative unpressed
        if (button_pressed && millis() - t_of_state_2 < long_press_time) {
          st = 2; // Unpress was temporary, return to short press
          t_of_button_change = millis();
        } else if (!button_pressed && millis() - t_of_state_2 >= long_press_time) {
          st = 3; // Unpress was temporary, return to long press
          t_of_button_change = millis();
        } else if (millis() - t_of_button_change >= debounce_time) { // A full button push is complete
          st = 0;
          if (millis() - t_of_state_2 < long_press_time) { // It is a short press
            flag = 1;
          } else {  // It is a long press
            flag = 2;
          }
        }
      }
      return flag;
    }
};



// I got this from online
// http://www.zrzahid.com/moving-average-of-last-n-numbers-in-a-stream/
class MovingAvgLastN {
  public:
    int maxTotal;
    int total;
    double * lastN;
    double avg;
    int head;

    MovingAvgLastN(int N) {
      maxTotal = N;
      lastN = new double[N];
      avg = 0;
      head = 0;
      total = 0;
    }

    void add(double num) {
      double prevSum = total * avg;

      if (total == maxTotal) {
        prevSum -= lastN[head];
        total--;
      }

      head = (head + 1) % maxTotal;
      int emptyPos = (maxTotal + head - 1) % maxTotal;
      lastN[emptyPos] = num;

      double newSum = prevSum + num;
      total++;
      avg = newSum / total;
    }

    double getAvg() {
      return avg;
    }

};

const int pin_visitor = 15;
const int pin_delivery = 4;
const int pin_led = 2;

MovingAvgLastN btn_delivery = MovingAvgLastN(50);
MovingAvgLastN delivery_all = MovingAvgLastN(100);

MovingAvgLastN btn_visitor = MovingAvgLastN(50);
MovingAvgLastN visitor_all = MovingAvgLastN(100);

char* ssid = "MIT";


void setup() {
  pinMode(pin_visitor, INPUT);
  pinMode(pin_delivery, INPUT);

  pinMode(pin_led, OUTPUT);

  Serial.begin(115200);
  Serial.println("Connecting to " + String(ssid));
  WiFi.begin(ssid);
  int count = 0;  // count used for WiFi check times
  while (WiFi.status() != WL_CONNECTED && count < 6) {
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

  btn_delivery.add(digitalRead(pin_delivery));
  delivery_all.add(digitalRead(pin_delivery));

  btn_visitor.add(digitalRead(pin_visitor));
  visitor_all.add(digitalRead(pin_visitor));

  Serial.println();
  Serial.println(String(delivery_all.getAvg()) +", "+ String(btn_delivery.getAvg()));
  Serial.println(String(visitor_all.getAvg()) +", "+ String(btn_visitor.getAvg()) );

  double threshold = .17;

  if (delivery_all.getAvg() - btn_delivery.getAvg() > threshold) {
    ring("DELIVERY");
    Serial.println("DELIVERY");
    flash();
    delay(10000);
    btn_delivery.add(10);
  }
  if (visitor_all.getAvg() - btn_visitor.getAvg() > threshold) {
    ring("VISITOR");
    Serial.println("VISITOR");
    flash();
    delay(10000);
    btn_visitor.add(10);
  }

  delay(10);
}

void flash() {
  for (int i = 0; i < 2 ; i ++) {
    Serial.println("FLASH");
    digitalWrite(pin_led, HIGH);
    delay(500);
    Serial.println("OFF");
    digitalWrite(pin_led, LOW);
    delay(500);
  }
}

void ring(String type) {
  
  const char* host = "https://hooks.slack.com";
//  const String url = "/services/T0AQ9C3PG/B02KKBD0TAL/ZkDavuJa8vkp5JsiAmDT73f1";
  const String url = "/services/T0AQ9C3PG/B02BZ54CGBC/D3iKXJq6LvQ9zDtdTjGdg9Io";

  WiFiClientSecure client; //instantiate a client object
  client.setInsecure();
  
  if (client.connect(host, 443)) {

    String body = "{\"text\": \":rotating_light: There is a "+ type + " at the gates! :rotating_light:\n REACT WHEN YOU HAVE SECURED THE GATES!\"}";

    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: hooks.slack.com");
    client.println("Content-Type: application/json");
    client.println("Content-Length: "+String(body.length()));
    client.println();
    client.print(body);


    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("headers received");
        break;
      }
    }
    // if there are incoming bytes available
    // from the server, read them and print them:
    while (client.available()) {
      char c = client.read();
      Serial.write(c);
    }

    client.stop();
  } else {
    Serial.println("connection failed");
    Serial.println("wait 0.5 sec...");
    client.stop();
    delay(300);
  }
}
