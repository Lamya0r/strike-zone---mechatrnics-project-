#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <ESP32Servo.h>

// ===== الشاشة =====
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ===== السيرفو =====
Servo servos[3];
int servoPins[3] = {15, 16, 17};

// ===== الحساسات =====
int ldrPins[3] = {34, 35, 32};
int threshold = 2700;
bool triggerHigh = true;

// ===== البنات =====
int buttonPin = 4;
int redLED = 25;
int yellowLED = 26;
int greenLED = 27;

// ===== حالات اللعبة =====
enum GameState { WAITING, COUNTDOWN, PLAYING, END };
GameState state = WAITING;

// ===== متغيرات =====
int score = 0;
unsigned long stateStartTime;
unsigned long gameStartTime;
unsigned long gameDuration = 30000;

// الهدف
int currentTarget = -1;
int lastTarget = -1;
unsigned long targetStartTime;
bool targetActive = false;

// مدة رفع الهدف
unsigned long targetUpDuration = 3000;

// تحديث الشاشة
unsigned long lastDisplayUpdate = 0;
unsigned long displayInterval = 300;

// زر debounce
unsigned long lastButtonPress = 0;
unsigned long debounceDelay = 200;

// ===== دوال =====
void updateDisplay(String l1, String l2) {
  if (millis() - lastDisplayUpdate < displayInterval) return;

  lastDisplayUpdate = millis();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 20, l1.c_str());
  u8g2.drawStr(0, 40, l2.c_str());
  u8g2.sendBuffer();
}

bool isHit(int value) {
  if (triggerHigh) return value > threshold;
  else return value < threshold;
}


bool sequentialTestMode = false;

int getNewTarget() {
  if (sequentialTestMode) {
    int t = (lastTarget + 1) % 3;   // 0 -> 1 -> 2 -> 0 ...
    lastTarget = t;
    return t;
  }

  int t;
  do {
    t = random(0, 3);
  } while (t == lastTarget);
  lastTarget = t;
  return t;
}

// ينزل كل السيرفوات لوضعها الأصلي
void resetAllServos() {
  for (int i = 0; i < 3; i++) {
    servos[i].write(0);
  }
}

// يبدأ هدف جديد بشكل صريح
void spawnNewTarget(unsigned long now) {
  resetAllServos();               // تأكد الكل نازل قبل ما يطلع الجديد
  currentTarget = getNewTarget();
  servos[currentTarget].write(90);
  targetStartTime = now;
  targetActive = true;
}

void setup() {
  Wire.begin(21, 22);
  u8g2.begin();

  for (int i = 0; i < 3; i++) {
    servos[i].attach(servoPins[i]);
    servos[i].write(0);
  }

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(redLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(greenLED, OUTPUT);

  randomSeed(analogRead(0));
}

void loop() {

  unsigned long now = millis();

  // ===== WAITING =====
  if (state == WAITING) {

    updateDisplay("Wanna play?", "Press button");

    if (digitalRead(buttonPin) == LOW &&
        now - lastButtonPress > debounceDelay) {

      lastButtonPress = now;
      state = COUNTDOWN;
      stateStartTime = now;
      score = 0;
    }

  }

  // ===== COUNTDOWN =====
  else if (state == COUNTDOWN) {

    int sec = (now - stateStartTime) / 1000;

    if (sec == 0) {
      digitalWrite(redLED, HIGH);
      updateDisplay("Ready...", "3");
    }
    else if (sec == 1) {
      digitalWrite(redLED, LOW);
      digitalWrite(yellowLED, HIGH);
      updateDisplay("Set...", "2");
    }
    else if (sec == 2) {
      digitalWrite(yellowLED, LOW);
      digitalWrite(greenLED, HIGH);
      updateDisplay("GO!", "1");
    }
    else {
      digitalWrite(greenLED, LOW);

      // إعادة ضبط كاملة قبل بداية اللعب - هذا يمنع أي هدف "عالق" من الجولة السابقة
      resetAllServos();
      targetActive = false;
      currentTarget = -1;
      lastTarget = -1;

      state = PLAYING;
      gameStartTime = now;
    }

  }

  // ===== PLAYING =====
  else if (state == PLAYING) {

    int timeLeft = (gameDuration - (now - gameStartTime)) / 1000;
    updateDisplay("Time: " + String(timeLeft), "Score: " + String(score));

    if (now - gameStartTime >= gameDuration) {
      resetAllServos();          // تأكد كل السيرفوات نازلة قبل نهاية اللعبة
      targetActive = false;
      currentTarget = -1;
      state = END;
      stateStartTime = now;
      return;
    }

    // ===== لا يوجد هدف حالياً -> أنشئ واحد =====
    if (!targetActive) {
      spawnNewTarget(now);
    }

    // ===== تحقق من الإصابة =====
    int lightValue = analogRead(ldrPins[currentTarget]);

    if (isHit(lightValue)) {
      score++;
      servos[currentTarget].write(0);   // ينزل فوراً فقط هو
      targetActive = false;
      currentTarget = -1;
    }
    // ===== انتهت مهلة الـ 3 ثواني بدون إصابة =====
    else if (now - targetStartTime > targetUpDuration) {
      servos[currentTarget].write(0);
      targetActive = false;
      currentTarget = -1;
    }

  }

  // ===== END =====
  else if (state == END) {

    updateDisplay("Good Game!", "Score: " + String(score));

    if (now - stateStartTime > 4000) {
      state = WAITING;
    }

  }
}