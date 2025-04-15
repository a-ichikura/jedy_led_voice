#include <M5StickCPlus.h>
#include <ros.h>
#include <std_msgs/UInt16.h>
#include <std_msgs/Float32.h>
#include <std_msgs/String.h>
#include <std_msgs/ColorRGBA.h>
#include <std_msgs/Bool.h>
#include <geometry_msgs/Vector3.h> 
#include <ArduinoJson.h>  // JSON解析ライブラリ

ros::NodeHandle  nh;
#include "FastLED.h"
String received_text = "";  // 受信した文字列
bool new_message = false;   // 新しいメッセージが来たかどうかのフラグ

// ジョイスティックボタン付きのI2Cアドレス
#define JOY_ADDR 0x52

//LEDのための設定
#define Neopixel_PIN 26
#define NUM_LEDS     37

CRGB leds[NUM_LEDS];
uint8_t gHue = 0;  // Initial tone value.

int led_mode = 1;
float r, g, b, brightness;
float duration = 3.0;
int blink_time = 1;
int rainbow_delta_hue = 1;

//音のための設定
#define SS 0.0      /*休符*/
#define T8  200  /*8分休符*/
#define T4  400  /*4分休符*/
#define T2  800  /*2分休符*/

#define C4 261.626  /*ド*/
#define E4 329.628  /*ミ*/
#define G4 391.995  /*ソ*/
#define C5 523.251  /*高いド*/
#define E5 659.255  /*高いミ*/
#define G5 783.991  /*高いソ*/

typedef struct {
    float freq;
    uint16_t period;
} tone_t;

// "Help" (緊急感を出すために急激な上昇)
const tone_t help_sound[] = {
    {C5, T8}, {E5, T8}, {G5, T8}, {SS, T8},
    {C5, T8}, {E5, T8}, {G5, T8}, {SS, T4}
};

// "Ready" (落ち着いた下降音階)
const tone_t ready_sound[] = {
    {G4, T8}, {E4, T8}, {C4, T4}, {SS, T4}
};

// "Joy" (明るい上昇)
const tone_t joy_sound[] = {
    {C4, T8}, {E4, T8}, {G4, T8}, {C5, T4}, {SS, T4}
};

//
// JSONで受信する音階データ
String received_tone_json = "";

// 声再生のコールバック関数
void toneCallback(const std_msgs::String& msg) {
    received_tone_json = msg.data;
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, received_tone_json);

    if (error) {
        Serial.println("Failed to parse JSON");
        return;
    }

    for (JsonObject note : doc.as<JsonArray>()) {
        float freq = note["freq"];
        int duration = note["duration"];

        if (freq > 0) {
            M5.Beep.tone(freq);
            delay(duration);
        } else {
            M5.Beep.mute();
            delay(duration);
        }
    }
    M5.Beep.mute();
}

//声のためのSubscriber
// ROS Subscriber
ros::Subscriber<std_msgs::String> tone_subscriber("tone_sequence", &toneCallback);


//LEDのためのコールバック
void ledModeMessageCb(const std_msgs::UInt16& msg){
  led_mode = msg.data;
}

void colorMessageCb( const std_msgs::ColorRGBA& msg){
  r = msg.r;
  g = msg.g;
  b = msg.b;
  brightness = msg.a;
 }

void blinkTimeMessageCb(const std_msgs::UInt16& msg){
  blink_time = msg.data;
}

void durationMessageCb(const std_msgs::Float32& msg){
  duration = msg.data;
}

void rainbowDeltaHueMessageCb(const std_msgs::UInt16& msg){
  rainbow_delta_hue = msg.data;
}

//LEDのための関数
void updateLED() {
    if (led_mode == 1) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].setRGB(r, g, b);
        }
        FastLED.setBrightness(brightness);
        FastLED.show();  // Updated LED color.
    } 
    
    else if (led_mode == 2) {
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i].setRGB(r, g, b);
        }
        delay(1000);
        blink_led();
    }
    
    else if (led_mode == 3) {
        fill_rainbow(leds, NUM_LEDS, gHue, rainbow_delta_hue);
        FastLED.setBrightness(brightness);
        FastLED.show();  // Updated LED color.
    }

    EVERY_N_MILLISECONDS(20) {
        gHue++;
    }  // The program is executed every 20 milliseconds.
}

void brighten_led(){
  int delta = max(int(brightness / 10), 1);
  float tmp_brightness = 0;
  for (tmp_brightness; tmp_brightness < brightness; tmp_brightness += delta){
    FastLED.setBrightness(tmp_brightness);
    FastLED.show(); 
    delay(200);
  }
  tmp_brightness = brightness;
  FastLED.setBrightness(tmp_brightness);
  FastLED.show();
  delay(200);
}

void fade_led(){
  int delta = max(int(brightness / 10), 1);
  float tmp_brightness = brightness;
  for (tmp_brightness; tmp_brightness > delta; tmp_brightness -= delta){
    FastLED.setBrightness(tmp_brightness);
    FastLED.show(); 
    delay(200);
  }
  tmp_brightness = 0;
  FastLED.setBrightness(tmp_brightness);
  FastLED.show();
  delay(200);
}

void blink_led(){
 for (blink_time; blink_time > 0; blink_time -= 1){
  brighten_led();
  delay(duration * 1000);
  fade_led();
 }
 brightness = 0;
}


//LEDのためのsubscriber
ros::Subscriber<std_msgs::UInt16> led_mode_subscriber("led_mode", &ledModeMessageCb);
ros::Subscriber<std_msgs::ColorRGBA> led_color_subscriber("led_rgb", &colorMessageCb);
ros::Subscriber<std_msgs::UInt16> led_blink_time_subscriber("led_blink_time", &blinkTimeMessageCb);
ros::Subscriber<std_msgs::Float32> led_duration_subscriber("led_duration", &durationMessageCb);
ros::Subscriber<std_msgs::UInt16> led_rainbow_delta_hue_subscriber("led_rainbow_delta_hue", &rainbowDeltaHueMessageCb);

//音楽のための関数
void playSound(const tone_t* tone, uint32_t length) {
    for (int i = 0; i < length; i++) {
        if (tone[i].freq > 0) {
            M5.Beep.tone(tone[i].freq);
        } else {
            M5.Beep.mute();
        }
        //delayMicroseconds(tone[i].period * 1000);
        delay(tone[i].period); // delayMicroseconds ではなく delay() を使用
    }
    //M5.Beep.mute();
}

void jedyvoiceMessageCb(const std_msgs::String& msg){
  //M5.Lcd.println(msg.data);  // トピックの内容を表示
  M5.Lcd.fillScreen(BLACK);  // 画面をクリア
  M5.Lcd.setCursor(0, 0);    // カーソルを左上にリセット
  M5.Lcd.setTextColor(WHITE); // 文字色を白に設定
  int char_width = 12;
  new_message = true;  // 新しいメッセージが来たことを通知
  received_text = msg.data;
  
  delay(100);
  if (strcmp(msg.data,"help")==0) {
        M5.Lcd.println("Help!");
        playSound(help_sound, sizeof(help_sound)/sizeof(help_sound[0]));
        delay(1000);
    } else if (strcmp(msg.data,"ready")==0) {
        M5.Lcd.println("Ready!");
        playSound(ready_sound, sizeof(ready_sound)/sizeof(ready_sound[0]));
        delay(1000);
    } else if (strcmp(msg.data,"joy")==0) {
        M5.Lcd.println("Joy!");
        playSound(joy_sound, sizeof(joy_sound)/sizeof(joy_sound[0])); 
    }
    else{
      if (new_message) {
        M5.Lcd.fillScreen(TFT_BLACK);  // 画面をクリア
        int x = 20, y = 20;  // 文字の初期位置
        int char_width = 15; // 文字幅（フォントサイズに応じて調整）
        
        for (int i = 0; i < received_text.length(); i++) {
            M5.Lcd.setCursor(x, y);
            M5.Lcd.print(received_text[i]); // 一文字ずつ表示
            x += char_width;  // 次の文字の位置を調整
            if (x > 200) {  // 画面端に来たら改行
                x = 10;
                y += 20;
            }
            delay(100);  // 100msの遅延でタイプライター風に
        }
        new_message = false;  // フラグをリセット
    }
}
}


//音楽のためのsubscriber
ros::Subscriber<std_msgs::String> jedy_voice_subscriber("jedy_voice", &jedyvoiceMessageCb);

//ボタンの状態を送信するためのpublisher
std_msgs::Bool button_msg;
ros::Publisher button_publisher("button_ispressed", &button_msg);

//ジョイスティックの状態を送信するためのpublisher
geometry_msgs::Vector3 xy_msg;
ros::Publisher xy_publisher("joystick_xy", &xy_msg);

void joystick_update() {
  uint8_t x = 0, y = 0, btn = 0;

  Wire.beginTransmission(JOY_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(JOY_ADDR, 3);

  if (Wire.available() == 3) {
    x = Wire.read();  // 0〜255
    y = Wire.read();
    btn = Wire.read();  // Zボタン（1: 押してる, 0: 離れてる）

    // 座標を publish（範囲は正規化したければ変更してOK）
    xy_msg.x = static_cast<float>(x);
    xy_msg.y = static_cast<float>(y);
    xy_msg.z = 0;
    xy_publisher.publish(&xy_msg);

    // Zボタンが押されたら publish
    if (btn == 1) {
      button_msg.data = true;
      button_publisher.publish(&button_msg);
      M5.Lcd.fillScreen(BLACK);  // 画面をクリア
      M5.Lcd.setCursor(20, 20);    // カーソルを左上にリセット
      M5.Lcd.setTextColor(WHITE); // 文字色を白に設定
      M5.Lcd.println("Button Pressed!");
    }
  }
}



void setup() {
    M5.begin();// Init M5Stack.
    M5.Lcd.setRotation(3);
    delay(100);
    //M5.Power.begin();       // Init power
    M5.Lcd.setTextSize(3);  
    M5.Lcd.println("HEX Example");
    M5.Lcd.println("Display rainbow effect");
    M5.Beep.setVolume(20);
    M5.Beep.begin();
    Wire.begin(32,33);

    // Neopixel initialization. 
    FastLED.addLeds<WS2811, Neopixel_PIN, GRB>(leds, NUM_LEDS)
        .setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(1);  // set the LED brightness to 5.
    nh.getHardware()->setBaud(115200);
    nh.initNode(); 
    nh.subscribe(led_mode_subscriber);
    nh.subscribe(led_color_subscriber);
    nh.subscribe(led_blink_time_subscriber);
    nh.subscribe(led_duration_subscriber);
    nh.subscribe(led_rainbow_delta_hue_subscriber);
    nh.subscribe(jedy_voice_subscriber);
    nh.subscribe(tone_subscriber);
    nh.advertise(button_publisher);
    nh.advertise(xy_publisher);
}

void loop() {
    nh.spinOnce();
    M5.update();
    updateLED();  // LED の更新を関数として呼び出す
    joystick_update(); //ジョイスティックの更新を関数として呼び出す
}
