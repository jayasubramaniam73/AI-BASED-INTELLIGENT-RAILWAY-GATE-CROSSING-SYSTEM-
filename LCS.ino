#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ---------------- WIFI ----------------
const char* ssid = "YOUR WIFI";
const char* password = "PASSWORD";

WebServer server(80);

// ---------------- PINS ----------------
#define IR_ENTRY 34
#define IR_EXIT  35
#define TRIG 5
#define ECHO 18
#define SERVO1_PIN 13
#define SERVO2_PIN 12
#define RED_LED 25
#define GREEN_LED 33

Servo gate1;
Servo gate2;

// ---------------- STATES ----------------
bool gateClosed = false;
bool ultraRunning = false;
bool waitingToOpen = false;
bool exitDetected = false;
bool gateMoving = false;

// ---------------- TIMERS ----------------
unsigned long ultraStartTime = 0;
unsigned long gateOpenTimer = 0;
unsigned long exitClearStart = 0;

// ---------------- IR STATE ----------------
bool exitPrevState = HIGH;

// ---------------- STATUS ----------------
String obstacleStatus = "NO";

// ---------------- WEB PAGE ----------------
String webpage() {

  String page = "<!DOCTYPE html><html><head>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";

  page += "<style>";
  page += "body{font-family:Arial;text-align:center;background:#111;color:#fff;margin-top:20px;}";
  page += "h2{color:#00c3ff;}";
  page += ".box{margin:10px auto;padding:12px;width:220px;background:#222;border-radius:8px;font-size:18px;}";
  page += ".btn{padding:10px 18px;margin:8px;border:none;border-radius:6px;font-size:15px;color:#fff;}";
  page += ".open{background:green;}";
  page += ".close{background:red;}";
  page += "</style></head><body>";

  page += "<h2>Railway Gate System</h2>";
  page += "<div class='box'>Gate: " + String(gateClosed ? "CLOSED" : "OPEN") + "</div>";
  page += "<div class='box'>Obstacle: <span id='obs'>---</span></div>";

  page += "<a href='/open'><button class='btn open'>OPEN</button></a>";
  page += "<a href='/close'><button class='btn close'>CLOSE</button></a>";

  page += "<script>";
  page += "setInterval(()=>{fetch('/status').then(r=>r.json()).then(d=>{";
  page += "document.getElementById('obs').innerText=d.obstacle;";
  page += "});},1000);";
  page += "</script>";

  page += "</body></html>";

  return page;
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  pinMode(IR_ENTRY, INPUT);
  pinMode(IR_EXIT, INPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  gate1.attach(SERVO1_PIN);
  gate2.attach(SERVO2_PIN);

  openGate();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  server.on("/", []() {
    server.send(200, "text/html", webpage());
  });

  server.on("/status", []() {
    String json = "{";
    json += "\"obstacle\":\"" + obstacleStatus + "\",";
    json += "\"gateClosed\":" + String(gateClosed ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/open", []() {
    openGate();
    server.send(200, "text/html", webpage());
  });

  server.on("/close", []() {
    closeGate();
    server.send(200, "text/html", webpage());
  });

  server.begin();
}

// ---------------- LOOP ----------------
void loop() {

  server.handleClient();

  int entry = digitalRead(IR_ENTRY);
  int exitSensor = digitalRead(IR_EXIT);

  // 🚆 ENTRY
  if (entry == LOW && !gateClosed && !gateMoving) {
    closeGate();
    gateClosed = true;

    ultraStartTime = millis();
    ultraRunning = true;
  }

  // 📏 ULTRASONIC + LED FIXED LOGIC
  if (gateClosed) {

    float dist = getDistance();

    if (ultraRunning && dist < 15) {
      obstacleStatus = "YES";

      // 🔴 BLINK ONLY WHEN OBSTACLE
      digitalWrite(RED_LED, millis() % 200 < 100);
    } 
    else {
      obstacleStatus = "NO";

      // 🔴 STABLE ON WHEN GATE CLOSED (NO OBSTACLE)
      digitalWrite(RED_LED, HIGH);
    }

    if (millis() - ultraStartTime >= 5000) {
      ultraRunning = false;
    }
  }

  // 🚆 EXIT DETECTION
  if (exitSensor == LOW && exitPrevState == HIGH && gateClosed) {
    exitDetected = true;
  }

  if (exitSensor == HIGH && exitPrevState == LOW) {
    exitClearStart = millis();
  }

  if (exitDetected && exitSensor == HIGH && millis() - exitClearStart > 1000) {
    exitDetected = false;
    waitingToOpen = true;
    gateOpenTimer = millis();
  }

  if (waitingToOpen && millis() - gateOpenTimer > 5000) {
    openGate();
    gateClosed = false;
    waitingToOpen = false;
    ultraRunning = false;
  }

  exitPrevState = exitSensor;
}

// ---------------- ULTRASONIC ----------------
float getDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 30000);
  if (duration <= 0) return 999;

  return duration * 0.034 / 2;
}

// ---------------- CLOSE ----------------
void closeGate() {
  if (gateMoving) return;
  gateMoving = true;

  for (int i = 0; i <= 90; i += 5) {
    gate1.write(i);
    gate2.write(i);
    delay(10);
  }

  gateClosed = true;
  gateMoving = false;

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);
}

// ---------------- OPEN ----------------
void openGate() {
  if (gateMoving) return;
  gateMoving = true;

  for (int i = 90; i >= 0; i -= 5) {
    gate1.write(i);
    gate2.write(i);
    delay(10);
  }

  gateClosed = false;
  gateMoving = false;

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);
}