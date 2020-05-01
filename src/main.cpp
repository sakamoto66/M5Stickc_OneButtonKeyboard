#include <M5StickC.h>
#include <BleKeyboard.h>
#include "FS.h"
#include "SPIFFS.h"

#define FORMAT_SPIFFS_IF_FAILED true
#define GROVE_SDA 32
#define GROVE_SCL 33

#define VBAT_CHECK_INTERVAL_MSEC 60000;
#define LCDOFF_CHECK_INTERVAL_MSEC 10000;

const uint8_t KEY_PRINT_SCREEN = 0xCE;

typedef struct Shortcut {
    char title[255];
    char keymap[255];
    uint8_t key1;
    uint8_t key2;
    uint8_t key3;
    uint8_t key4;
    uint8_t key5;
    uint8_t key6;
} shortcut_t;

BleKeyboard bleKeyboard("One Button Keyboard");

// バッテリー更新用
unsigned long nextVbatCheck = 0;

// 選択中のモード
int8_t currentMode = 0;

// LCD OFF用
unsigned long nextLcdOffCheck = 0;

// 外部ボタン
int grove_button_a = 0;
int grove_button_b = 0;

// ショートカット一覧
static const shortcut_t shortcutList[15] = {
  {"Teams Mute", "Ctrl+Shift+M", KEY_LEFT_CTRL, KEY_LEFT_SHIFT, 'm', 0}
 ,{"Zoom Mute", "Alt+A", KEY_LEFT_ALT, 'a', 0, 0}
 ,{"Win Capture Screen",   "Win+PrtSc", KEY_LEFT_GUI, KEY_PRINT_SCREEN, 0, 0}
 ,{"Win Capture Movie",    "Win+Alt+R", KEY_LEFT_GUI, KEY_LEFT_ALT, 'r', 0, KEY_LEFT_GUI, 'g'}
 ,{"Win Clipboard Screen", "PrtSc",     KEY_PRINT_SCREEN, 0, 0, 0}
 ,{"Win Clipboard Window", "Alt+PrtSc", KEY_LEFT_ALT, KEY_PRINT_SCREEN, 0, 0}
 ,{"Mac Capture Screen",   "Cmd+Shift+3",     KEY_LEFT_GUI, KEY_LEFT_SHIFT, 0, '3'}
 ,{"Mac Capture Area",     "Cmd+Shift+4",     KEY_LEFT_GUI, KEY_LEFT_SHIFT, 0, '4'}
 ,{"Mac Capture Movie",    "Cmd+Shift+5",     KEY_LEFT_GUI, KEY_LEFT_SHIFT, 0, '5'}
 ,{"Mac Clipboard Screen", "Cmd+Shift+Ctl+3", KEY_LEFT_GUI, KEY_LEFT_SHIFT, KEY_LEFT_CTRL, '3'}
 ,{"Mac Clipboard Area",   "Cmd+Shift+Ctl+4", KEY_LEFT_GUI, KEY_LEFT_SHIFT, KEY_LEFT_CTRL, '4'}
 ,{"Chrome Capture Page",  "Shift+Alt+P",     KEY_LEFT_SHIFT, KEY_LEFT_ALT, 0, 'p'}
 ,{"Free Key:F13", "F13", KEY_F13, 0, 0, 0}
 ,{"Free Key:F14", "F14", KEY_F14, 0, 0, 0}
 ,{"Free Key:F15", "F15", KEY_F15, 0, 0, 0}
};

// バッテリー残量取得
int getVlevel() {
  float vbat = M5.Axp.GetBatVoltage();
  int vlevel = ( vbat - 3.2 ) / 0.8 * 100;
  if ( vlevel < 0 ) {
    vlevel = 0;
  }
  if ( 100 < vlevel ) {
    vlevel = 100;
  }
  return vlevel;
}

// 外部ボタンAが押されているか
bool pressedGroveButtonA(){
  int curr = digitalRead(GROVE_SDA);
  if(curr == grove_button_a){
    return false;
  }
  grove_button_a = curr;
  return 0 != curr;
}

// 外部ボタンBが押されているか
bool pressedGroveButtonB(){
  int curr = digitalRead(GROVE_SCL);
  if(curr == grove_button_b){
    return false;
  }
  grove_button_b = curr;
  return 0 != curr;
}

// 画面表示
void displayMessage(void){
  shortcut_t kmode = shortcutList[currentMode];

  M5.Lcd.setTextColor(WHITE, TFT_BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("%2d/%d", currentMode+1, sizeof(shortcutList)/sizeof(*shortcutList));

  M5.Lcd.setTextColor(WHITE, TFT_BLUE);
  M5.Lcd.setCursor(0, 22);
  M5.Lcd.fillRect(0, 16, 160, 44, TFT_BLUE);
  M5.Lcd.printf("%-26s", kmode.title);

  M5.Lcd.setTextColor(WHITE, TFT_BLACK);
  M5.Lcd.setCursor(0, 64);
  M5.Lcd.printf("%13s", kmode.keymap);
}

// キーボードのキーを送信
void sendKey(void){
  if (!bleKeyboard.isConnected()) {
    return;
  }
  shortcut_t kmode = shortcutList[currentMode];
  if(0<kmode.key5 && 0<kmode.key6){
    if(0<kmode.key5){
      bleKeyboard.press(kmode.key5);
    }
    if(0<kmode.key6){
      bleKeyboard.press(kmode.key6);
    }
    delay(100);
    bleKeyboard.releaseAll();
  }
  if(0<kmode.key1){
    bleKeyboard.press(kmode.key1);
  }
  if(0<kmode.key2){
    bleKeyboard.press(kmode.key2);
  }
  if(0<kmode.key3){
    bleKeyboard.press(kmode.key3);
  }
  if(0<kmode.key4){
    bleKeyboard.press(kmode.key4);
  }
  delay(100);
  bleKeyboard.releaseAll();
}

// モードを保存
void saveMode(uint8_t mode){
  fs::FS fs = SPIFFS;
  File file = fs.open("/mode.txt", FILE_WRITE);
  file.write(mode);
}

// モードを読込
uint8_t loadMode(){
  fs::FS fs = SPIFFS;
  File file = fs.open("/mode.txt");
  char buf[1] = {0};
  if(file.available()){
    file.readBytes(buf, 1);
  }
  return buf[0];
}

void setup() {
  // M5セットアップ
  M5.begin(true, true, false);
  SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED);
  setCpuFrequencyMhz(80);
  pinMode(GROVE_SDA ,INPUT);
  pinMode(GROVE_SCL ,INPUT);

  // 値初期化
  currentMode = (int)loadMode();
  nextLcdOffCheck = millis() + LCDOFF_CHECK_INTERVAL_MSEC;

  // 画面初期化
  M5.Axp.ScreenBreath(9);
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextSize(2);

  // BLE起動
  bleKeyboard.setBatteryLevel(getVlevel());
  bleKeyboard.begin();

  // 画面表示
  displayMessage();
}

void loop() {
  // ボタン状態更新
  M5.update();

  // 一定時間後、バッテリー残量更新
  if (nextVbatCheck < millis()) {
    nextVbatCheck = millis() + VBAT_CHECK_INTERVAL_MSEC;
    M5.Lcd.setTextColor(WHITE, TFT_BLACK);
    M5.Lcd.setCursor(112, 0);
    M5.Lcd.printf("%3d%%", getVlevel());
  }

  // 一定時間後、バックライトOFF
  if (nextLcdOffCheck < millis()) {
    M5.Axp.SetLDO2( false );
  }

  // Homeボタン/外部ボタンAボタン/外部ボタンBボタン押下
  if ( M5.BtnA.wasPressed() | pressedGroveButtonA() | pressedGroveButtonB()) {
    sendKey();
    return;
  }

  // Bボタン押下
  if ( M5.BtnB.wasPressed() ) {
    if (nextLcdOffCheck < millis()) {
      M5.Axp.SetLDO2( true );
    } else {
      size_t max = sizeof(shortcutList)/sizeof(*shortcutList);
      if(max <= ++currentMode) currentMode = 0;
      saveMode((uint8_t)currentMode);
      displayMessage();
    }
    nextLcdOffCheck = millis() + LCDOFF_CHECK_INTERVAL_MSEC;
  }
  // リセットボタン押下
  if ( 0x02 == M5.Axp.GetBtnPress() ) {
    if (nextLcdOffCheck < millis()) {
      M5.Axp.SetLDO2( true );
    } else {
      size_t max = sizeof(shortcutList)/sizeof(*shortcutList);
      if(0 > --currentMode) currentMode = max - 1;
      saveMode((uint8_t)currentMode);
      displayMessage();
    }
    nextLcdOffCheck = millis() + LCDOFF_CHECK_INTERVAL_MSEC;
  }

  // Wait
  delay(100);
}