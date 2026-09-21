// @file      main.ino
// @brief     ESP32 smartwatch firmware with clock modes, 
// weather, and retro games
// @author    ImaniTechnology
// @date      2026-09-21

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

#define LCD_SCLK 12
#define LCD_MOSI 11
#define LCD_DC 8
#define LCD_CS 10
#define LCD_RST 18


#define BTN_UP 45
#define BTN_DOWN 46
#define BTN_LEFT 47
#define BTN_RIGHT 48

#define WIFI_SSID "Iphonej"
#define WIFI_PASS "qwertzui3322"

#define LATITUDE 47.4245
#define LONGITUDE 9.3767

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 panel;
  lgfx::Bus_SPI bus;
public:
  LGFX() {
    auto b = bus.config();
    b.spi_host = SPI2_HOST;
    b.spi_mode = 0;
    b.freq_write = 80000000;
    b.freq_read = 16000000;
    b.pin_sclk = LCD_SCLK;
    b.pin_mosi = LCD_MOSI;
    b.pin_miso = -1;
    b.pin_dc = LCD_DC;
    bus.config(b);
    panel.setBus(&bus);
    auto p = panel.config();
    p.pin_cs = LCD_CS;
    p.pin_rst = LCD_RST;
    p.panel_width = 240;
    p.panel_height = 240;
    p.memory_width = 240;
    p.memory_height = 240;
    p.offset_x = 0;
    p.offset_y = 0;
    p.readable = false;
    p.invert = true;
    p.rgb_order = false;
    p.dlen_16bit = false;
    panel.config(p);
    setPanel(&panel);
  }
};

LGFX lcd;
lgfx::LGFX_Sprite frame(&lcd);
Preferences prefs;

constexpr int W = 240;
constexpr int H = 240;
constexpr float CX = 119.5f;
constexpr float CY = 119.5f;
constexpr float R = 114.0f;
constexpr int MAX_SEG = 150;
constexpr int MAX_PART = 70;

enum Mode {
  CLOCK_MODE,
  DATE_MODE,
  WEATHER_MODE,
  GAME_SELECT_MODE,
  SNAKE_MODE,
  PACMAN_MODE,
  TETRIS_MODE
};

Mode mode = CLOCK_MODE;
int clockStyle = 0;
int gameSelectIdx = 0;

struct Vec { float x, y; };

struct Particle {
  Vec p, v;
  float life, size;
  uint16_t color;
  bool active;
};

uint16_t RGB(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3);
}

uint16_t C_BG        = RGB(12, 16, 24);
uint16_t C_PANEL     = RGB(24, 32, 45);
uint16_t C_WHITE     = RGB(255, 255, 255);
uint16_t C_BLACK     = RGB(0, 0, 0);
uint16_t C_TEXT      = RGB(210, 225, 240);
uint16_t C_MUTED     = RGB(100, 120, 145);
uint16_t C_ORANGE    = RGB(255, 120, 0);
uint16_t C_RED       = RGB(235, 35, 30);
uint16_t C_YELLOW    = RGB(255, 200, 20);
uint16_t C_BLUE      = RGB(0, 120, 220);
uint16_t C_CYAN      = RGB(0, 220, 255);
uint16_t C_GREEN     = RGB(40, 220, 70);

uint16_t C_SNAKE_HEAD = RGB(50, 240, 80);
uint16_t C_SNAKE_BODY = RGB(20, 180, 60);
uint16_t C_SNAKE_DARK = RGB(10, 110, 35);

Vec snake[MAX_SEG];
Vec food;
Vec dir, nextDir;
Particle particles[MAX_PART];
int snakeLen = 7;
int score = 0;
int highscore = 0;
float snakeSpeed = 6.0f;
float foodAnim = 0;
bool gameOver = false;
uint32_t lastLogic = 0;

Vec pacmanPos = {120, 120};
Vec pacmanDir = {1, 0};
Vec ghostPos = {80, 80};
int pacmanScore = 0;
bool pacmanGameOver = false;
uint32_t lastPacmanMove = 0;
float mouthAngle = 30.0f;
bool mouthOpening = true;
bool dotsActive[8] = {true, true, true, true, true, true, true, true};

constexpr int BOARD_W = 10;
constexpr int BOARD_H = 16;
uint8_t tetrisBoard[BOARD_H][BOARD_W] = {0};
int currentPiece = 0;
int pieceRotation = 0;
int pieceX = 3, pieceY = 0;
int tetrisScore = 0;
bool tetrisGameOver = false;
uint32_t lastTetrisDrop = 0;
uint32_t tetrisSpeedInterval = 500;

const uint8_t SHAPES[7][4][4][4] = {
  {{{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}, {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}}, {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}, {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}}},
  {{{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}, {{0,1,1,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}, {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}}, {{0,1,0,0},{0,1,0,0},{1,1,0,0},{0,0,0,0}}},
  {{{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}, {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}}, {{0,0,0,0},{1,1,1,0},{1,0,0,0},{0,0,0,0}}, {{1,1,0,0},{0,1,0,0},{0,1,0,0},{0,0,0,0}}},
  {{{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}, {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}, {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}, {{0,1,1,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}},
  {{{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}, {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}}, {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}, {{0,1,0,0},{0,1,1,0},{0,0,1,0},{0,0,0,0}}},
  {{{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}}, {{0,1,0,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}}, {{0,0,0,0},{1,1,1,0},{0,1,0,0},{0,0,0,0}}, {{0,1,0,0},{1,1,0,0},{0,1,0,0},{0,0,0,0}}},
  {{{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}, {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}}, {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}, {{0,0,1,0},{0,1,1,0},{0,1,0,0},{0,0,0,0}}}
};

const uint16_t PIECE_COLORS[7] = {
  RGB(0, 230, 255), RGB(30, 90, 220), RGB(255, 140, 0),
  RGB(255, 210, 0), RGB(40, 210, 50), RGB(160, 40, 220), RGB(230, 40, 40)
};

float temperature = NAN, humidity = NAN, wind = NAN;
int weatherCode = -1;
bool weatherOK = false;
String weatherText = "NO DATA";

uint32_t lastFrame = 0;
uint32_t lastWeather = 0;

void resetSnake();
void resetPacman();
void resetTetris();

void updateParticles(float dt) {
  for (int i = 0; i < MAX_PART; i++) {
    if (particles[i].active) {
      particles[i].p.x += particles[i].v.x * dt;
      particles[i].p.y += particles[i].v.y * dt;
      particles[i].life -= dt;
      if (particles[i].life <= 0) particles[i].active = false;
    }
  }
}

float d2(float x, float y) { return x * x + y * y; }

bool insideCircle(float x, float y, float radius = 0) {
  float dx = x - CX;
  float dy = y - CY;
  float rr = R - radius;
  return dx * dx + dy * dy <= rr * rr;
}

void centerText(const char *txt, float y, int size, uint16_t color) {
  frame.setTextDatum(middle_center);
  frame.setTextSize(size);
  frame.setTextColor(color);
  frame.drawString(txt, CX, y);
}

void drawRing(float radius, uint16_t color, float width = 1) {
  for (int i = 0; i < width; i++)
    frame.drawCircle(CX, CY, radius - i, color);
}

void drawCleanBackground(uint16_t ringColor = RGB(0, 90, 170)) {
  frame.fillScreen(C_BG);
  frame.fillCircle(CX, CY, 114, C_BG);
  drawRing(113, ringColor);
  drawRing(111, RGB(18, 35, 55));
}

bool checkComboExit() {
  if (!digitalRead(BTN_LEFT) && !digitalRead(BTN_RIGHT)) {
    mode = GAME_SELECT_MODE;
    delay(200);
    return true;
  }
  return false;
}

void showSplash() {
  frame.fillScreen(C_WHITE);
  frame.setTextDatum(middle_center);
  frame.setTextSize(4);
  
  frame.setTextColor(C_BLACK);
  frame.drawString("IM", CX - 45, CY);
  
  frame.setTextColor(C_BLUE);
  frame.drawString("TECH", CX + 35, CY);
  
  frame.pushSprite(0, 0);
  delay(2200);
}

void drawWatchFace() {
  drawCleanBackground();
  for (int i = 0; i < 60; i++) {
    float a = (i * 6.0f - 90.0f) * DEG_TO_RAD;
    float outer = 106;
    float inner = (i % 5 == 0) ? 94 : 101;
    uint16_t col = (i % 5 == 0) ? C_CYAN : RGB(40, 65, 95);
    frame.drawLine(CX + cosf(a) * inner, CY + sinf(a) * inner, CX + cosf(a) * outer, CY + sinf(a) * outer, col);
  }
}

void drawClock() {
  struct tm t;
  bool hasTime = getLocalTime(&t);

  if (clockStyle == 1) {
    drawWatchFace();
    if (!hasTime) { t.tm_hour = 10; t.tm_min = 10; t.tm_sec = 0; t.tm_mday = 1; }

    frame.fillRoundRect(CX + 40, CY - 10, 34, 20, 4, C_PANEL);
    frame.drawRoundRect(CX + 40, CY - 10, 34, 20, 4, C_CYAN);
    char dayBuf[8];
    snprintf(dayBuf, sizeof(dayBuf), "%02d", t.tm_mday);
    frame.setTextDatum(middle_center);
    frame.setTextSize(1);
    frame.setTextColor(C_WHITE);
    frame.drawString(dayBuf, CX + 57, CY);

    float sa = (t.tm_sec * 6.0f - 90.0f) * DEG_TO_RAD;
    float ma = ((t.tm_min + t.tm_sec / 60.0f) * 6.0f - 90.0f) * DEG_TO_RAD;
    float ha = (((t.tm_hour % 12) + t.tm_min / 60.0f) * 30.0f - 90.0f) * DEG_TO_RAD;

    frame.drawLine(CX, CY, CX + cosf(ha) * 45, CY + sinf(ha) * 45, C_WHITE);
    frame.drawLine(CX + 1, CY, CX + cosf(ha) * 45 + 1, CY + sinf(ha) * 45, C_WHITE);

    frame.drawLine(CX, CY, CX + cosf(ma) * 68, CY + sinf(ma) * 68, C_CYAN);
    frame.drawLine(CX + 1, CY, CX + cosf(ma) * 68 + 1, CY + sinf(ma) * 68, C_CYAN);

    frame.drawLine(CX, CY, CX + cosf(sa) * 85, CY + sinf(sa) * 85, C_RED);

    frame.fillCircle(CX, CY, 4, C_RED);
    frame.fillCircle(CX, CY, 2, C_WHITE);
  } else {
    drawCleanBackground();
    if (!hasTime) {
      centerText("--:--", 105, 4, C_WHITE);
      centerText("NO TIME", 140, 1, C_MUTED);
      return;
    }
    char timeBuf[16], secBuf[8], dateBuf[32];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);
    strftime(secBuf, sizeof(secBuf), "%S", &t);
    strftime(dateBuf, sizeof(dateBuf), "%a, %d.%m.%Y", &t);

    centerText(timeBuf, 98, 5, C_WHITE);
    centerText(secBuf, 134, 2, C_ORANGE);
    centerText(dateBuf, 162, 1, C_TEXT);
  }
}

void drawShoeSole(int x, int y) {
  frame.fillRoundRect(x - 4, y - 10, 8, 12, 3, C_CYAN);
  frame.fillRoundRect(x - 3, y + 4, 6, 6, 2, C_CYAN);
  frame.fillRect(x - 4, y + 2, 8, 1, C_BG);
}

void drawDate() {
  drawCleanBackground();

  drawShoeSole(CX - 50, CY - 28);
  drawShoeSole(CX - 36, CY - 36);
  
  frame.setTextDatum(middle_left);
  frame.setTextSize(2);
  frame.setTextColor(C_WHITE);
  frame.drawString("8,420", CX - 20, CY - 30);

  int x = CX - 50, y = CY + 2;
  frame.fillCircle(x + 6, y + 10, 8, C_ORANGE);
  frame.fillTriangle(x + 6, y - 4, x - 2, y + 10, x + 14, y + 10, C_ORANGE);
  frame.fillCircle(x + 6, y + 12, 4, C_YELLOW);

  frame.drawString("385", CX - 20, CY + 8);

  x = CX - 40; y = CY + 48;
  frame.drawRoundRect(x, y, 32, 16, 3, C_GREEN);
  frame.fillRect(x + 32, y + 4, 3, 8, C_GREEN);
  frame.fillRect(x + 3, y + 3, 22, 10, C_GREEN);

  frame.drawString("88%", CX, CY + 55);
}

const char* weatherDescription(int code) {
  if (code == 0) return "CLEAR";
  if (code == 1 || code == 2) return "PARTLY CLOUDY";
  if (code == 3) return "CLOUDY";
  if (code >= 45 && code <= 48) return "FOG";
  if (code >= 51 && code <= 57) return "DRIZZLE";
  if (code >= 61 && code <= 67) return "RAIN";
  if (code >= 71 && code <= 77) return "SNOW";
  if (code >= 80 && code <= 82) return "SHOWERS";
  if (code >= 95) return "THUNDER";
  return "UNKNOWN";
}

void weatherIcon(int code, float x, float y) {
  if (code == 0) {
    frame.fillCircle(x, y, 16, C_YELLOW);
    for (int i = 0; i < 8; i++) {
      float a = i * PI / 4.0f;
      frame.drawLine(x + cosf(a) * 21, y + sinf(a) * 21, x + cosf(a) * 27, y + sinf(a) * 27, C_YELLOW);
    }
    return;
  }
  frame.fillCircle(x - 8, y, 10, C_MUTED);
  frame.fillCircle(x + 5, y - 5, 13, C_TEXT);
  frame.fillCircle(x + 16, y + 1, 9, C_MUTED);
  frame.fillRoundRect(x - 20, y, 40, 15, 7, C_TEXT);
}

void updateWeather() {
  if (WiFi.status() != WL_CONNECTED) { weatherOK = false; return; }
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LATITUDE, 4) + "&longitude=" + String(LONGITUDE, 4) + "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&timezone=auto";
  http.begin(url);
  http.setTimeout(5000);
  if (http.GET() == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString())) {
      JsonObject c = doc["current"];
      temperature = c["temperature_2m"] | NAN;
      humidity = c["relative_humidity_2m"] | NAN;
      wind = c["wind_speed_10m"] | NAN;
      weatherCode = c["weather_code"] | -1;
      weatherText = weatherDescription(weatherCode);
      weatherOK = true;
    }
  }
  http.end();
  lastWeather = millis();
}

void drawWeather() {
  drawCleanBackground();
  if (!weatherOK) {
    centerText(WiFi.status() == WL_CONNECTED ? "LOADING..." : "NO WIFI", 105, 2, C_TEXT);
    return;
  }
  weatherIcon(weatherCode, CX, 75);
  char temp[20], hum[20], windText[20];
  snprintf(temp, sizeof(temp), "%.1f°C", temperature);
  snprintf(hum, sizeof(hum), "HUM %.0f%%", humidity);
  snprintf(windText, sizeof(windText), "WIND %.1f km/h", wind);

  centerText(temp, 120, 3, C_WHITE);
  centerText(weatherText.c_str(), 150, 1, C_CYAN);
  centerText(hum, 172, 1, C_TEXT);
  centerText(windText, 188, 1, C_MUTED);
}

void drawGameSelect() {
  drawCleanBackground();
  centerText("GAMING MENU", 45, 2, C_WHITE);

  const char* games[3] = {"1. SNAKE", "2. PACMAN", "3. TETRIS"};
  for (int i = 0; i < 3; i++) {
    uint16_t color = (i == gameSelectIdx) ? C_ORANGE : C_MUTED;
    int yPos = 85 + (i * 32);
    if (i == gameSelectIdx) {
      frame.fillRoundRect(CX - 65, yPos - 12, 130, 24, 6, RGB(25, 45, 70));
      frame.drawRoundRect(CX - 65, yPos - 12, 130, 24, 6, C_ORANGE);
    }
    centerText(games[i], yPos, 2, color);
  }
  centerText("UP/DN: SELECT | RIGHT: PLAY", 188, 1, C_TEXT);
  centerText("LEFT: EXIT TO CLOCK", 205, 1, C_MUTED);
}

void spawnFood() {
  for (int n = 0; n < 300; n++) {
    float a = random(0, 6283) / 1000.0f;
    float rr = sqrtf(random(0, 10000) / 10000.0f) * 92.0f;
    food = {CX + cosf(a) * rr, CY + sinf(a) * rr};
    bool ok = true;
    for (int i = 0; i < snakeLen; i++) {
      if (d2(food.x - snake[i].x, food.y - snake[i].y) < 280) { ok = false; break; }
    }
    if (ok) return;
  }
}

void burst(float x, float y, uint16_t color, int count) {
  for (int n = 0; n < count; n++) {
    for (int i = 0; i < MAX_PART; i++) {
      if (!particles[i].active) {
        float a = random(0, 6283) / 1000.0f;
        float s = 15 + random(0, 450) / 10.0f;
        particles[i].p = {x, y};
        particles[i].v = {cosf(a) * s, sinf(a) * s};
        particles[i].life = .35f + random(0, 300) / 1000.0f;
        particles[i].size = 1 + random(0, 15) / 10.0f;
        particles[i].color = color;
        particles[i].active = true;
        break;
      }
    }
  }
}

void resetSnake() {
  snakeLen = 6; score = 0; snakeSpeed = 6.0f; gameOver = false;
  dir = {1, 0}; nextDir = dir;
  for (int i = 0; i < snakeLen; i++) snake[i] = {CX - i * 7.0f, CY};
  for (auto &p : particles) p.active = false;
  spawnFood();
  lastLogic = millis();
}

void snakeInput() {
  if (checkComboExit()) return;

  static uint32_t debounce[4] = {0, 0, 0, 0};
  const uint8_t pins[4] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT};

  for (int i = 0; i < 4; i++) {
    if (!digitalRead(pins[i]) && millis() - debounce[i] > 90) {
      debounce[i] = millis();

      if (gameOver) {
        if (pins[i] == BTN_UP) resetSnake();
        else if (pins[i] == BTN_LEFT) mode = GAME_SELECT_MODE;
        return;
      }

      if (pins[i] == BTN_UP && dir.y != 1) nextDir = {0, -1};
      if (pins[i] == BTN_DOWN && dir.y != -1) nextDir = {0, 1};
      if (pins[i] == BTN_LEFT && dir.x != 1) nextDir = {-1, 0};
      if (pins[i] == BTN_RIGHT && dir.x != -1) nextDir = {1, 0};
    }
  }
}

void snakeLogic() {
  if (gameOver) return;
  uint32_t now = millis();
  float interval = 1000.0f / snakeSpeed;

  while (now - lastLogic >= interval) {
    lastLogic += interval;
    dir = nextDir;
    Vec head = snake[0];
    head.x += dir.x * 7.0f; head.y += dir.y * 7.0f;

    if (!insideCircle(head.x, head.y, 7) || gameOver) {
      gameOver = true; burst(head.x, head.y, C_RED, 25); return;
    }

    for (int i = 4; i < snakeLen; i++) {
      if (d2(head.x - snake[i].x, head.y - snake[i].y) < 42) {
        gameOver = true; burst(head.x, head.y, C_RED, 25); return;
      }
    }

    for (int i = snakeLen - 1; i > 0; i--) snake[i] = snake[i - 1];
    snake[0] = head;

    if (d2(head.x - food.x, head.y - food.y) < 100) {
      if (snakeLen < MAX_SEG - 1) snakeLen++;
      score++;
      if (score > highscore) { highscore = score; prefs.putInt("high", highscore); }
      snakeSpeed = 6.0f + min(score, 20) * 0.25f;
      burst(food.x, food.y, C_ORANGE, 18);
      spawnFood();
    }
  }
}

void drawSnakeMode() {
  drawCleanBackground(C_GREEN);

  float pulse = 1.0f + .12f * sinf(foodAnim * 5.0f);
  frame.fillCircle(food.x, food.y, 6.5f * pulse, C_RED);
  frame.fillCircle(food.x - 2, food.y - 2, 2.0f * pulse, RGB(255, 120, 120));
  frame.fillRect(food.x - 1, food.y - 9, 2, 3, RGB(100, 60, 20));
  frame.fillCircle(food.x + 2, food.y - 8, 1.5f, C_GREEN);

  for (int i = snakeLen - 1; i > 0; i--) {
    float r = 6.0f - (float(i) / snakeLen) * 1.5f;
    frame.fillCircle(snake[i].x, snake[i].y, r, C_SNAKE_BODY);
    frame.drawCircle(snake[i].x, snake[i].y, r, C_SNAKE_DARK);
  }

  Vec h = snake[0];
  frame.fillCircle(h.x, h.y, 7.5f, C_SNAKE_HEAD);
  frame.drawCircle(h.x, h.y, 7.5f, C_WHITE);

  Vec tongue = {h.x + dir.x * 12, h.y + dir.y * 12};
  Vec side = {-dir.y, dir.x};
  frame.drawLine(h.x + dir.x * 6, h.y + dir.y * 6, tongue.x, tongue.y, C_RED);
  frame.drawLine(tongue.x, tongue.y, tongue.x + side.x * 3 + dir.x * 2, tongue.y + side.y * 3 + dir.y * 2, C_RED);
  frame.drawLine(tongue.x, tongue.y, tongue.x - side.x * 3 + dir.x * 2, tongue.y - side.y * 3 + dir.y * 2, C_RED);

  float eyeOffsetX = side.x * 3.5f;
  float eyeOffsetY = side.y * 3.5f;
  frame.fillCircle(h.x + dir.x * 3 + eyeOffsetX, h.y + dir.y * 3 + eyeOffsetY, 2.5f, C_WHITE);
  frame.fillCircle(h.x + dir.x * 3 - eyeOffsetX, h.y + dir.y * 3 - eyeOffsetY, 2.5f, C_WHITE);
  frame.fillCircle(h.x + dir.x * 4 + eyeOffsetX, h.y + dir.y * 4 + eyeOffsetY, 1.2f, C_BLACK);
  frame.fillCircle(h.x + dir.x * 4 - eyeOffsetX, h.y + dir.y * 4 - eyeOffsetY, 1.2f, C_BLACK);

  char s[24];
  snprintf(s, sizeof(s), "%d | BEST %d", score, highscore);
  centerText(s, 22, 1, C_WHITE);

  for (auto &p : particles) {
    if (p.active) frame.fillCircle(p.p.x, p.p.y, max(1.0f, p.size), p.color);
  }

  if (gameOver) {
    frame.fillCircle(CX, CY, 72, RGB(20, 8, 8));
    centerText("GAME OVER", 82, 2, C_RED);
    snprintf(s, sizeof(s), "SCORE %d", score);
    centerText(s, 110, 1, C_WHITE);
    centerText("UP: REPLAY", 142, 1, C_SNAKE_HEAD);
    centerText("LEFT: MENU", 162, 1, C_MUTED);
  }
}

void resetPacman() {
  pacmanPos = {CX, CY + 20}; ghostPos = {CX, CY - 40};
  pacmanDir = {1, 0}; pacmanScore = 0; pacmanGameOver = false;
  for (int i = 0; i < 8; i++) dotsActive[i] = true;
}

void pacmanInput() {
  if (checkComboExit()) return;

  static uint32_t debounce[4] = {0, 0, 0, 0};
  const uint8_t pins[4] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT};

  for (int i = 0; i < 4; i++) {
    if (!digitalRead(pins[i]) && millis() - debounce[i] > 90) {
      debounce[i] = millis();

      if (pacmanGameOver) {
        if (pins[i] == BTN_UP) resetPacman();
        else if (pins[i] == BTN_LEFT) mode = GAME_SELECT_MODE;
        return;
      }

      if (pins[i] == BTN_UP) pacmanDir = {0, -1};
      if (pins[i] == BTN_DOWN) pacmanDir = {0, 1};
      if (pins[i] == BTN_LEFT) pacmanDir = {-1, 0};
      if (pins[i] == BTN_RIGHT) pacmanDir = {1, 0};
    }
  }
}

void pacmanLogic() {
  if (pacmanGameOver) return;

  if (mouthOpening) {
    mouthAngle += 4.0f;
    if (mouthAngle >= 45.0f) mouthOpening = false;
  } else {
    mouthAngle -= 4.0f;
    if (mouthAngle <= 5.0f) mouthOpening = true;
  }

  if (millis() - lastPacmanMove > 90) {
    lastPacmanMove = millis();
    pacmanPos.x += pacmanDir.x * 4.5f;
    pacmanPos.y += pacmanDir.y * 4.5f;

    if (!insideCircle(pacmanPos.x, pacmanPos.y, 10)) {
      pacmanPos.x -= pacmanDir.x * 4.5f;
      pacmanPos.y -= pacmanDir.y * 4.5f;
    }

    bool allEaten = true;
    for (int i = 0; i < 8; i++) {
      if (dotsActive[i]) {
        float a = (i * 45) * DEG_TO_RAD;
        float dotX = CX + cosf(a) * 60;
        float dotY = CY + sinf(a) * 60;

        if (d2(pacmanPos.x - dotX, pacmanPos.y - dotY) < 144) {
          dotsActive[i] = false;
          pacmanScore += 10;
        } else {
          allEaten = false;
        }
      }
    }

    if (allEaten) {
      for (int i = 0; i < 8; i++) dotsActive[i] = true;
    }

    if (ghostPos.x < pacmanPos.x) ghostPos.x += 1.6f; else ghostPos.x -= 1.6f;
    if (ghostPos.y < pacmanPos.y) ghostPos.y += 1.6f; else ghostPos.y -= 1.6f;

    if (d2(pacmanPos.x - ghostPos.x, pacmanPos.y - ghostPos.y) < 90) pacmanGameOver = true;
  }
}

void drawPacmanMode() {
  drawCleanBackground(C_BLUE);

  for (int i = 0; i < 8; i++) {
    if (dotsActive[i]) {
      float a = (i * 45) * DEG_TO_RAD;
      frame.fillCircle(CX + cosf(a) * 60, CY + sinf(a) * 60, 4, C_YELLOW);
    }
  }

  float startA = atan2f(pacmanDir.y, pacmanDir.x) * RAD_TO_DEG + mouthAngle;
  float endA = atan2f(pacmanDir.y, pacmanDir.x) * RAD_TO_DEG + (360.0f - mouthAngle);
  frame.fillArc(pacmanPos.x, pacmanPos.y, 10, 0, startA, endA, C_YELLOW);

  frame.fillCircle(ghostPos.x, ghostPos.y - 2, 8, C_RED);
  frame.fillRect(ghostPos.x - 8, ghostPos.y - 2, 16, 8, C_RED);
  frame.fillCircle(ghostPos.x - 3, ghostPos.y - 3, 2, C_WHITE);
  frame.fillCircle(ghostPos.x + 3, ghostPos.y - 3, 2, C_WHITE);

  char s[24];
  snprintf(s, sizeof(s), "SCORE %d", pacmanScore);
  centerText(s, 25, 1, C_WHITE);

  if (pacmanGameOver) {
    frame.fillCircle(CX, CY, 72, RGB(20, 8, 8));
    centerText("GAME OVER", 82, 2, C_RED);
    centerText("UP: REPLAY", 125, 1, C_WHITE);
    centerText("LEFT: MENU", 148, 1, C_MUTED);
  }
}

bool checkTetrisCollision(int px, int py, int pr) {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (SHAPES[currentPiece][pr][r][c]) {
        int boardX = px + c;
        int boardY = py + r;

        if (boardX < 0 || boardX >= BOARD_W || boardY >= BOARD_H) return true;
        if (boardY >= 0 && tetrisBoard[boardY][boardX] != 0) return true;
      }
    }
  }
  return false;
}

void lockTetrisPiece() {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (SHAPES[currentPiece][pieceRotation][r][c]) {
        int boardY = pieceY + r;
        int boardX = pieceX + c;
        if (boardY >= 0 && boardY < BOARD_H && boardX >= 0 && boardX < BOARD_W) {
          tetrisBoard[boardY][boardX] = currentPiece + 1;
        }
      }
    }
  }

  for (int r = BOARD_H - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < BOARD_W; c++) {
      if (tetrisBoard[r][c] == 0) { full = false; break; }
    }
    if (full) {
      tetrisScore += 100;
      if (tetrisSpeedInterval > 100) tetrisSpeedInterval -= 15;
      
      for (int y = r; y > 0; y--) {
        for (int x = 0; x < BOARD_W; x++) {
          tetrisBoard[y][x] = tetrisBoard[y - 1][x];
        }
      }
      for (int x = 0; x < BOARD_W; x++) tetrisBoard[0][x] = 0;
      r++;
    }
  }

  currentPiece = random(0, 7);
  pieceRotation = 0;
  pieceX = 3;
  pieceY = 0;

  if (checkTetrisCollision(pieceX, pieceY, pieceRotation)) {
    tetrisGameOver = true;
  }
}

void resetTetris() {
  memset(tetrisBoard, 0, sizeof(tetrisBoard));
  tetrisScore = 0; 
  tetrisSpeedInterval = 500;
  tetrisGameOver = false;
  currentPiece = random(0, 7); 
  pieceRotation = 0; 
  pieceX = 3; 
  pieceY = 0;
}

void tetrisInput() {
  if (checkComboExit()) return;

  static uint32_t debounce[4] = {0, 0, 0, 0};
  const uint8_t pins[4] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT};

  for (int i = 0; i < 4; i++) {
    if (!digitalRead(pins[i]) && millis() - debounce[i] > 80) {
      debounce[i] = millis();

      if (tetrisGameOver) {
        if (pins[i] == BTN_UP) resetTetris();
        else if (pins[i] == BTN_LEFT) mode = GAME_SELECT_MODE;
        return;
      }

      if (pins[i] == BTN_LEFT) {
        if (!checkTetrisCollision(pieceX - 1, pieceY, pieceRotation)) pieceX--;
      }
      if (pins[i] == BTN_RIGHT) {
        if (!checkTetrisCollision(pieceX + 1, pieceY, pieceRotation)) pieceX++;
      }
      if (pins[i] == BTN_DOWN) {
        if (!checkTetrisCollision(pieceX, pieceY + 1, pieceRotation)) {
          pieceY++;
        } else {
          lockTetrisPiece();
        }
      }
      if (pins[i] == BTN_UP) {
        int nextRot = (pieceRotation + 1) % 4;
        if (!checkTetrisCollision(pieceX, pieceY, nextRot)) pieceRotation = nextRot;
      }
    }
  }
}

void tetrisLogic() {
  if (tetrisGameOver) return;

  if (millis() - lastTetrisDrop > tetrisSpeedInterval) {
    lastTetrisDrop = millis();
    if (!checkTetrisCollision(pieceX, pieceY + 1, pieceRotation)) {
      pieceY++;
    } else {
      lockTetrisPiece();
    }
  }
}

void drawTetrisMode() {
  drawCleanBackground(C_BLUE);
  int startX = CX - (BOARD_W * 6);
  int startY = CY - (BOARD_H * 6) + 10;

  frame.drawRect(startX - 2, startY - 2, BOARD_W * 12 + 4, BOARD_H * 12 + 4, C_BLUE);

  for (int r = 0; r < BOARD_H; r++) {
    for (int c = 0; c < BOARD_W; c++) {
      if (tetrisBoard[r][c]) {
        uint16_t col = PIECE_COLORS[tetrisBoard[r][c] - 1];
        frame.fillRect(startX + c * 12, startY + r * 12, 11, 11, col);
        frame.drawRect(startX + c * 12, startY + r * 12, 11, 11, C_WHITE);
      }
    }
  }

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (SHAPES[currentPiece][pieceRotation][r][c]) {
        uint16_t col = PIECE_COLORS[currentPiece];
        frame.fillRect(startX + (pieceX + c) * 12, startY + (pieceY + r) * 12, 11, 11, col);
        frame.drawRect(startX + (pieceX + c) * 12, startY + (pieceY + r) * 12, 11, 11, C_WHITE);
      }
    }
  }

  char s[24];
  snprintf(s, sizeof(s), "SCORE %d", tetrisScore);
  centerText(s, 15, 1, C_WHITE);

  if (tetrisGameOver) {
    frame.fillCircle(CX, CY, 72, RGB(20, 8, 8));
    centerText("GAME OVER", 82, 2, C_RED);
    centerText("UP: REPLAY", 125, 1, C_WHITE);
    centerText("LEFT: MENU", 148, 1, C_MUTED);
  }
}

void drawMode() {
  if (mode == CLOCK_MODE) drawClock();
  else if (mode == DATE_MODE) drawDate();
  else if (mode == WEATHER_MODE) drawWeather();
  else if (mode == GAME_SELECT_MODE) drawGameSelect();
  else if (mode == SNAKE_MODE) drawSnakeMode();
  else if (mode == PACMAN_MODE) drawPacmanMode();
  else if (mode == TETRIS_MODE) drawTetrisMode();
}

void globalInput() {
  static uint32_t lastBtn = 0;

  if (mode == SNAKE_MODE) { snakeInput(); return; }
  if (mode == PACMAN_MODE) { pacmanInput(); return; }
  if (mode == TETRIS_MODE) { tetrisInput(); return; }

  if (millis() - lastBtn < 150) return;

  if (mode == GAME_SELECT_MODE) {
    if (!digitalRead(BTN_UP)) {
      lastBtn = millis();
      gameSelectIdx = (gameSelectIdx + 2) % 3;
    } else if (!digitalRead(BTN_DOWN)) {
      lastBtn = millis();
      gameSelectIdx = (gameSelectIdx + 1) % 3;
    } else if (!digitalRead(BTN_RIGHT)) {
      lastBtn = millis();
      if (gameSelectIdx == 0) { mode = SNAKE_MODE; resetSnake(); }
      else if (gameSelectIdx == 1) { mode = PACMAN_MODE; resetPacman(); }
      else if (gameSelectIdx == 2) { mode = TETRIS_MODE; resetTetris(); }
    } else if (!digitalRead(BTN_LEFT)) {
      lastBtn = millis();
      mode = CLOCK_MODE;
    }
    return;
  }

  if (!digitalRead(BTN_LEFT)) {
    lastBtn = millis();
    if (mode != CLOCK_MODE) mode = CLOCK_MODE;
    else clockStyle = (clockStyle + 1) % 2;
  } else if (!digitalRead(BTN_UP)) {
    lastBtn = millis();
    mode = GAME_SELECT_MODE;
  } else if (!digitalRead(BTN_RIGHT)) {
    lastBtn = millis();
    mode = (mode == DATE_MODE) ? CLOCK_MODE : DATE_MODE;
  } else if (!digitalRead(BTN_DOWN)) {
    lastBtn = millis();
    mode = (mode == WEATHER_MODE) ? CLOCK_MODE : WEATHER_MODE;
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 7000) delay(100);

  if (WiFi.status() == WL_CONNECTED) {
    configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
    updateWeather();
  }
}

void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  lcd.init();
  lcd.setRotation(0);
  lcd.setColorDepth(16);

  frame.setColorDepth(16);
  frame.createSprite(W, H);
  frame.setSwapBytes(true);

  prefs.begin("watch", false);
  highscore = prefs.getInt("high", 0);

  randomSeed(esp_random());

  showSplash();
  connectWiFi();

  lastFrame = micros();
  lastLogic = millis();
}

void loop() {
  uint32_t now = micros();
  float dt = (now - lastFrame) / 1000000.0f;
  if (dt > .05f) dt = .05f;
  lastFrame = now;

  globalInput();

  if (mode == SNAKE_MODE) {
    snakeLogic();
    updateParticles(dt);
    foodAnim += dt;
  } else if (mode == PACMAN_MODE) {
    pacmanLogic();
  } else if (mode == TETRIS_MODE) {
    tetrisLogic();
  }

  if (mode == WEATHER_MODE && WiFi.status() != WL_CONNECTED && millis() - lastWeather > 600000) {
    updateWeather();
  }

  drawMode();
  frame.pushSprite(0, 0);
  delay(8);
}