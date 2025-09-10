#include <TFT_eSPI.h>
#include <Adafruit_PM25AQI.h>
#include <math.h>   // For sinf() and sqrtf()
#include <cstring>  // For strcmp and memset

// --- Hardware & Display ---
TFT_eSPI tft = TFT_eSPI();
Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

// --- Buttons ---
const int BTN_RED   = 13;
const int BTN_GREEN = 12;
const int BTN_BLUE  = 14;

// --- Debouncing & Long Press ---
bool prevRed = HIGH, prevGreen = HIGH, prevBlue = HIGH;
unsigned long lastDebounce = 0;
const unsigned long debounceMs = 50;
unsigned long redPressStart = 0;
unsigned long lastDropCombo = 0;

// --- Modes & State ---
enum Mode { MODE_AQ, MODE_GAME_SELECT, MODE_GAME };
Mode currentMode = MODE_AQ;
int selectedGame = 0; // 0=falling_blocks, 1=brick_breaker
String games[2] = {"falling_blocks", "brick_breaker"};
bool sensorOk = true;

// --- Splash & Animations ---
bool initialBootSplashComplete = false;

// --- AQ Mode Globals ---
uint16_t lastPm25 = 0;
float breathePhase = 0.0f;
int8_t lastBucket = -1;

// --- falling_blocks Game State & Definitions ---
#define falling_blocks_BLOCK_SIZE 8
#define falling_blocks_BOARD_WIDTH 10
#define falling_blocks_BOARD_HEIGHT 20
#define BOARD_DRAW_X_OFFSET 80
#define BOARD_DRAW_Y_OFFSET 40
const int UI_TOP_Y = 26;
const int UI_PANEL_RIGHT = BOARD_DRAW_X_OFFSET - 6;
uint8_t falling_blocksBoard[falling_blocks_BOARD_HEIGHT][falling_blocks_BOARD_WIDTH];
const uint8_t falling_blocksShapes[7][4][4][4] = {
  { { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} }, { {0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0} }, { {0,0,0,0}, {0,0,0,0}, {1,1,1,1}, {0,0,0,0} }, { {0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0} } },
  { { {1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, { {0,1,1,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0} }, { {0,0,0,0}, {1,1,1,0}, {0,0,1,0}, {0,0,0,0} }, { {0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0} } },
  { { {0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, { {0,1,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,0,0} }, { {0,0,0,0}, {1,1,1,0}, {1,0,0,0}, {0,0,0,0} }, { {1,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0} } },
  { { {0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }, { {0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }, { {0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }, { {0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} } },
  { { {0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} }, { {0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0} }, { {0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0} }, { {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0} } },
  { { {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, { {0,1,0,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0} }, { {0,0,0,0}, {1,1,1,0}, {0,1,0,0}, {0,0,0,0} }, { {0,1,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0} } },
  { { {1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }, { {0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0} }, { {0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0} }, { {0,1,0,0}, {1,1,0,0}, {1,0,0,0}, {0,0,0,0} } }
};
const uint16_t blockColors[] = { TFT_BLACK, TFT_CYAN, TFT_BLUE, 0xFDA0, TFT_YELLOW, TFT_GREEN, TFT_MAGENTA, TFT_RED };
struct Tetromino { int type; int rotation; int x; int y; };
Tetromino currentPiece, nextPiece;
long score = 0;
int level = 1, linesCleared = 0;
bool gameOver = false;
unsigned long lastFallTime = 0, fallInterval = 1000;
unsigned long gameLoopLastCall = 0;
const unsigned long gameLoopDelay = 50;

// --- brick_breaker Game State & Definitions ---
#define BR_W 160
#define BR_H 160
#define BR_X ((240 - BR_W)/2)
#define BR_Y ((240 - BR_H)/2)
#define BR_COLS 8
#define BR_ROWS 6
struct Brick { uint8_t alive; uint16_t color; };
static Brick bricks[BR_ROWS][BR_COLS];
#define PADDLE_W 36
#define PADDLE_H 6
#define PADDLE_Y (BR_Y + BR_H - 12)
static int paddleX;
static float paddleXF; // Float accumulator for smooth movement
const float PADDLE_STEP_F = 8.0f * 0.85f; // 6.8
#define BALL_R 3
static float ballX, ballY, ballVX, ballVY;
static bool ballStuck;
static int lives;
static long brick_breakerScore;
static bool brick_breakerOver;
static unsigned long lastbrick_breakerFrame = 0;
const unsigned long brick_breakerFrameDelay = 16; // ~60 FPS

// --- Function Prototypes ---
void drawCenteredString(const String &text, int y, uint16_t color, int size);
void drawRoundBezel();
void initfalling_blocksGame();
void resetfalling_blocksGame();
void falling_blocksLoop();
void drawTetromino(const Tetromino& piece, uint16_t color);
void drawfalling_blocksBoard();
void placeTetromino();
void clearLines();
void spawnNewPiece();
void drawGameUI();
void initbrick_breakerGame();
void brick_breakerLoop();
void showGameOverScreen();
void showSplash();
void showScanningAnimation();

// --- General Helpers & Splash ---
void drawCenteredString(const String &text, int y, uint16_t color, int size = 2) {
  tft.setTextSize(size); tft.setTextColor(color, TFT_BLACK); tft.setTextDatum(MC_DATUM); tft.drawString(text, tft.width()/2, y);
}
void drawRoundBezel() {
  int cx = tft.width() / 2, cy = tft.height() / 2;
  tft.drawCircle(cx, cy, 119, TFT_DARKGREY); tft.drawCircle(cx, cy, 118, TFT_BLACK);
}
uint16_t rainbowColor(int i) {
  switch (i % 6) {
    case 0: return TFT_RED; case 1: return 0xFDA0; case 2: return TFT_YELLOW;
    case 3: return TFT_GREEN; case 4: return TFT_CYAN; case 5: return TFT_MAGENTA;
  }
  return TFT_WHITE;
}
void showSplash() {
  tft.fillScreen(TFT_BLACK);
  String hello = "HELLO";
  int textWidth = hello.length() * 14;
  int xStart = (tft.width() - textWidth) / 2;
  for (int i = 0; i < (int)hello.length(); i++) { tft.drawChar(xStart + i * 14, 60, hello[i], rainbowColor(i), TFT_BLACK, 2); }
  drawCenteredString("from BOB BOT", 100, TFT_WHITE, 2);
  for (int i = 0; i < 30; i++) { tft.fillCircle(random(240), random(240), random(1, 3), rainbowColor(i)); delay(20); }
  int cx = tft.width() / 2, cy = tft.height() / 2;
  for (int sweep = 0; sweep < 2; sweep++) { for (int r = 10; r <= 100; r += 10) { tft.drawCircle(cx, cy, r, TFT_GREEN); delay(50); tft.drawCircle(cx, cy, r, TFT_BLACK); } }
  delay(600);
  tft.fillScreen(TFT_BLACK);
  drawCenteredString("Bob Bot", 90, TFT_WHITE, 2); drawCenteredString("checks the air...", 120, TFT_WHITE, 2);
  delay(1200);
  tft.fillScreen(TFT_BLACK);
  drawCenteredString("for particles from", 90, TFT_ORANGE, 2); drawCenteredString("wild fires", 120, TFT_RED, 2);
  for (int i = 0; i < 40; i++) { tft.drawPixel(random(240), random(240), (i % 2 == 0) ? TFT_RED : TFT_ORANGE); delay(20); }
  delay(900);
  tft.fillScreen(TFT_BLACK);
  drawCenteredString("and dust", 110, TFT_LIGHTGREY, 2);
  for (int i = 0; i < 30; i++) { tft.fillCircle(random(240), random(240), random(2, 5), TFT_LIGHTGREY); delay(30); }
  delay(900);
  tft.fillScreen(TFT_BLACK);
  drawCenteredString("pollution", 90, TFT_WHITE, 2); drawCenteredString("(car exhaust)", 120, TFT_LIGHTGREY, 2);
  for (int i = 0; i < 30; i++) { tft.fillCircle(i * 8, 150 + sin(i * 0.5) * 10, 6, TFT_DARKGREY); delay(40); }
  delay(900);
}
void showScanningAnimation() {
  tft.fillScreen(TFT_BLACK);
  drawCenteredString("Scanning...", 110, TFT_GREEN, 2);
  for (int sweep = 0; sweep < 2; sweep++) {
    for (int x = 0; x < tft.width(); x += 4) { tft.fillRect(x, 0, 4, tft.height(), TFT_DARKGREY); delay(8); tft.fillRect(x, 0, 4, tft.height(), TFT_BLACK); }
    delay(250);
  }
  tft.fillScreen(TFT_BLACK);
  drawCenteredString("Scanning Complete!", 110, TFT_GREEN, 2);
  delay(900);
}

// --- Air Quality Mode ---
uint16_t aqColorFor(uint16_t pm25) { if (pm25 <= 12) return TFT_GREEN; if (pm25 <= 35) return TFT_YELLOW; if (pm25 <= 55) return 0xFD20; return TFT_RED; }
String aqLabelFor(uint16_t pm25) { if (pm25 <= 12) return "Good"; if (pm25 <= 35) return "Moderate"; if (pm25 <= 55) return "Unhealthy"; return "Very Bad"; }
const int AQ_NUM_Y = 110, AQ_NUM_BOX_W = 140, AQ_NUM_BOX_H = 60;
const int AQ_NUM_BOX_X = (240 - AQ_NUM_BOX_W)/2, AQ_NUM_BOX_Y = AQ_NUM_Y - AQ_NUM_BOX_H/2;
void drawAQStatic(uint16_t pm25) {
  tft.fillScreen(TFT_BLACK);
  drawCenteredString("PM2.5", 50, TFT_WHITE, 1);
  drawCenteredString(aqLabelFor(pm25), 170, aqColorFor(pm25), 2);
  drawRoundBezel();
}
void drawAQNumberBreathing(uint16_t pm25, float phase) {
  tft.fillRect(AQ_NUM_BOX_X, AQ_NUM_BOX_Y, AQ_NUM_BOX_W, AQ_NUM_BOX_H, TFT_BLACK);
  float s = 4.0f + 0.25f * sinf(phase);
  int ts = (int)roundf(s);
  tft.setTextSize(ts);
  tft.setTextColor(aqColorFor(pm25), TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(String(pm25), tft.width()/2, AQ_NUM_Y);
}

// --- Game Select Mode ---
void drawGameMenu() {
  tft.fillScreen(TFT_BLACK);
  drawCenteredString("Select a Game", 50, TFT_WHITE, 2);
  for (int i = 0; i < 2; i++) { drawCenteredString(games[i], 100 + i * 30, (i == selectedGame) ? TFT_GREEN : TFT_WHITE, 2); }
  drawRoundBezel();
}

// --- brick_breaker Implementation ---
void drawPaddle(int x, uint16_t color) { tft.fillRect(x, PADDLE_Y, PADDLE_W, PADDLE_H, color); }
void erasePaddle(int x) { drawPaddle(x, TFT_BLACK); }
void drawBall(float x, float y, uint16_t color) {
  tft.fillCircle((int)roundf(x), (int)roundf(y), BALL_R, color);
}
void eraseBall(float x, float y) { drawBall(x, y, TFT_BLACK); }
void drawBrick(int r, int c) {
  int brickW = BR_W / BR_COLS; int brickH = 12;
  int brickX = BR_X + c * brickW; int brickY = BR_Y + 20 + r * brickH;
  tft.fillRect(brickX, brickY, brickW, brickH, bricks[r][c].color);
  tft.drawRect(brickX, brickY, brickW, brickH, TFT_DARKGREY);
}
void eraseBrick(int r, int c) {
  int brickW = BR_W / BR_COLS; int brickH = 12;
  int brickX = BR_X + c * brickW; int brickY = BR_Y + 20 + r * brickH;
  tft.fillRect(brickX, brickY, brickW, brickH, TFT_BLACK);
}
void drawbrick_breakerUI() {
  tft.fillRect(BR_X, BR_Y, BR_W, 18, TFT_BLACK);
  tft.setTextDatum(TL_DATUM); tft.setTextSize(1); tft.setTextColor(TFT_WHITE);
  tft.drawString("Lives: " + String(lives), BR_X + 4, BR_Y + 4);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("Score: " + String(brick_breakerScore), BR_X + BR_W - 4, BR_Y + 4);
  tft.setTextDatum(MC_DATUM);
}
void rebuildBricks() {
  for (int r = 0; r < BR_ROWS; r++) {
    for (int c = 0; c < BR_COLS; c++) {
      bricks[r][c].alive = 1; bricks[r][c].color = rainbowColor(r);
      drawBrick(r, c);
    }
  }
}
void resetBallOnPaddle() {
  ballStuck = true;
  ballX = paddleXF + PADDLE_W / 2.0f; ballY = PADDLE_Y - BALL_R - 1;
  ballVX = 1.6f; ballVY = -1.6f;
  drawBall(ballX, ballY, TFT_WHITE);
}
void normalizeBallSpeed() {
    float mag = sqrtf(ballVX * ballVX + ballVY * ballVY);
    float targetMag = 2.2f;
    if (mag > 0) { ballVX = (ballVX / mag) * targetMag; ballVY = (ballVY / mag) * targetMag; }
}
void initbrick_breakerGame() {
  Serial.println("Initializing brick_breaker...");
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(BR_X - 1, BR_Y - 1, BR_W + 2, BR_H + 2, TFT_DARKGREY);
  lives = 3; brick_breakerScore = 0; brick_breakerOver = false;
  paddleXF = BR_X + (BR_W - PADDLE_W) / 2.0f;
  paddleX  = (int)roundf(paddleXF);
  rebuildBricks();
  drawbrick_breakerUI();
  drawPaddle(paddleX, TFT_WHITE);
  resetBallOnPaddle();
  lastbrick_breakerFrame = 0;
  drawRoundBezel();
}
void brick_breakerLoop() {
  unsigned long now = millis();
  if (now - lastbrick_breakerFrame < brick_breakerFrameDelay) return;
  lastbrick_breakerFrame = now;
  if (brick_breakerOver) return;

  int oldPaddleX = paddleX;
  if (digitalRead(BTN_RED)  == LOW) paddleXF -= PADDLE_STEP_F;
  if (digitalRead(BTN_BLUE) == LOW) paddleXF += PADDLE_STEP_F;

  paddleXF = constrain(paddleXF, (float)BR_X, (float)(BR_X + BR_W - PADDLE_W));
  int newPaddleX = (int)roundf(paddleXF);

  if (newPaddleX != oldPaddleX) {
    erasePaddle(oldPaddleX);
    paddleX = newPaddleX;
    drawPaddle(paddleX, TFT_WHITE);
  }

  if (ballStuck) {
    eraseBall(ballX, ballY);
    ballX = paddleXF + PADDLE_W / 2.0f;
    drawBall(ballX, ballY, TFT_WHITE);
    return;
  }

  eraseBall(ballX, ballY);
  ballX += ballVX; ballY += ballVY;

  if (ballX <= BR_X + BALL_R) { ballX = BR_X + BALL_R; ballVX = -ballVX; }
  if (ballX >= BR_X + BR_W - BALL_R) { ballX = BR_X + BR_W - BALL_R; ballVX = -ballVX; }
  if (ballY <= BR_Y + BALL_R) { ballY = BR_Y + BALL_R; ballVY = -ballVY; }

  if (ballVY > 0 && ballY + BALL_R >= PADDLE_Y && ballX >= paddleX && ballX <= paddleX + PADDLE_W) {
    ballY = PADDLE_Y - BALL_R - 1;
    ballVY = -fabsf(ballVY);
    float hit = (ballX - (paddleX + PADDLE_W / 2.0f)) / (PADDLE_W / 2.0f);
    ballVX += 0.8f * hit;
    normalizeBallSpeed();
  }

  if (ballY >= BR_Y + BR_H - BALL_R) {
    lives--;
    if (lives <= 0) { brick_breakerOver = true; gameOver = true; showGameOverScreen(); return; }
    drawbrick_breakerUI();
    resetBallOnPaddle();
    return;
  }

  bool allBricksCleared = true;
  for (int r = 0; r < BR_ROWS; r++) {
    for (int c = 0; c < BR_COLS; c++) {
      if (bricks[r][c].alive) {
        allBricksCleared = false;
        int brickW = BR_W / BR_COLS; int brickH = 12;
        int brickX = BR_X + c * brickW; int brickY = BR_Y + 20 + r * brickH;
        if (ballX >= brickX && ballX <= brickX + brickW && ballY >= brickY && ballY <= brickY + brickH) {
          bricks[r][c].alive = 0;
          eraseBrick(r, c);
          ballVY = -ballVY;
          brick_breakerScore += 50;
          drawbrick_breakerUI();
          goto end_brick_check;
        }
      }
    }
  }
end_brick_check:

  if (allBricksCleared) {
    rebuildBricks();
    ballVX *= 1.1f; ballVY *= 1.1f;
    resetBallOnPaddle();
  }

  drawBall(ballX, ballY, TFT_WHITE);
}

// --- falling_blocks Implementation ---
void drawBlock(int bx, int by, uint16_t color) {
  if (by < 0) return;
  int sx = BOARD_DRAW_X_OFFSET + bx * falling_blocks_BLOCK_SIZE;
  int sy = BOARD_DRAW_Y_OFFSET + by * falling_blocks_BLOCK_SIZE;
  tft.fillRect(sx, sy, falling_blocks_BLOCK_SIZE, falling_blocks_BLOCK_SIZE, color);
  if (color != TFT_BLACK) { tft.drawRect(sx, sy, falling_blocks_BLOCK_SIZE, falling_blocks_BLOCK_SIZE, TFT_DARKGREY); }
}
void drawTetromino(const Tetromino& piece, uint16_t color) {
  tft.startWrite();
  for (int i = 0; i < 4; i++) { for (int j = 0; j < 4; j++) { if (falling_blocksShapes[piece.type][piece.rotation][i][j]) { drawBlock(piece.x + j, piece.y + i, color); } } }
  tft.endWrite();
}
void drawfalling_blocksBoard() {
  tft.startWrite();
  tft.fillRect(BOARD_DRAW_X_OFFSET, BOARD_DRAW_Y_OFFSET, falling_blocks_BOARD_WIDTH * falling_blocks_BLOCK_SIZE, falling_blocks_BOARD_HEIGHT * falling_blocks_BLOCK_SIZE, TFT_BLACK);
  for (int y = 0; y < falling_blocks_BOARD_HEIGHT; y++) { for (int x = 0; x < falling_blocks_BOARD_WIDTH; x++) { if (falling_blocksBoard[y][x] != 0) { drawBlock(x, y, blockColors[falling_blocksBoard[y][x]]); } } }
  tft.drawRect(BOARD_DRAW_X_OFFSET - 1, BOARD_DRAW_Y_OFFSET - 1, falling_blocks_BOARD_WIDTH * falling_blocks_BLOCK_SIZE + 2, falling_blocks_BOARD_HEIGHT * falling_blocks_BLOCK_SIZE + 2, TFT_WHITE);
  tft.endWrite();
  drawRoundBezel();
}
bool checkCollision(const Tetromino& piece, int newX, int newY, int newRot) {
  for (int i = 0; i < 4; i++) { for (int j = 0; j < 4; j++) { if (falling_blocksShapes[piece.type][newRot][i][j]) { int bX = newX + j; int bY = newY + i; if (bX < 0 || bX >= falling_blocks_BOARD_WIDTH || bY >= falling_blocks_BOARD_HEIGHT || (bY >= 0 && falling_blocksBoard[bY][bX] != 0)) return true; } } }
  return false;
}
void placeTetromino() {
  bool toppedOut = false;
  for (int i = 0; i < 4; i++) { for (int j = 0; j < 4; j++) { if (falling_blocksShapes[currentPiece.type][currentPiece.rotation][i][j]) { int yy = currentPiece.y + i; int xx = currentPiece.x + j; if (yy < 0) { toppedOut = true; } else { falling_blocksBoard[yy][xx] = currentPiece.type + 1; } } } }
  if (toppedOut) { gameOver = true; showGameOverScreen(); }
}
void clearLines() {
  int linesClearedThisFrame = 0;
  for (int y = falling_blocks_BOARD_HEIGHT - 1; y >= 0; y--) {
    bool fullLine = true;
    for (int x = 0; x < falling_blocks_BOARD_WIDTH; x++) { if (falling_blocksBoard[y][x] == 0) { fullLine = false; break; } }
    if (fullLine) {
      linesClearedThisFrame++;
      for (int moveY = y; moveY > 0; moveY--) { for (int x = 0; x < falling_blocks_BOARD_WIDTH; x++) { falling_blocksBoard[moveY][x] = falling_blocksBoard[moveY - 1][x]; } }
      for (int x = 0; x < falling_blocks_BOARD_WIDTH; x++) { falling_blocksBoard[0][x] = 0; }
      y++;
    }
  }
  if (linesClearedThisFrame > 0) { score += linesClearedThisFrame * 100 * level; linesCleared += linesClearedThisFrame; if (linesCleared / 10 >= level) { level++; fallInterval = max(100L, 1000L - (level - 1) * 70L); } }
}
void spawnNewPiece() {
  currentPiece = nextPiece;
  currentPiece.x = (falling_blocks_BOARD_WIDTH / 2) - 2; currentPiece.y = -2;
  nextPiece.type = random(7); nextPiece.rotation = 0;
  if (checkCollision(currentPiece, currentPiece.x, currentPiece.y, currentPiece.rotation)) { gameOver = true; showGameOverScreen(); }
}
void drawGameUI() {
  tft.fillRect(0, 0, UI_PANEL_RIGHT, tft.height(), TFT_BLACK);
  tft.fillRect(BOARD_DRAW_X_OFFSET + falling_blocks_BOARD_WIDTH * falling_blocks_BLOCK_SIZE + 1, 0, tft.width() - (BOARD_DRAW_X_OFFSET + falling_blocks_BOARD_WIDTH * falling_blocks_BLOCK_SIZE + 1), tft.height(), TFT_BLACK);
  tft.setTextDatum(TR_DATUM); tft.setTextSize(2); tft.setTextColor(TFT_WHITE);
  tft.drawString("Score:", UI_PANEL_RIGHT, UI_TOP_Y); tft.drawString(String(score), UI_PANEL_RIGHT, UI_TOP_Y + 20);
  tft.drawString("Level:", UI_PANEL_RIGHT, UI_TOP_Y + 50); tft.drawString(String(level), UI_PANEL_RIGHT, UI_TOP_Y + 70);
  tft.drawString("Lines:", UI_PANEL_RIGHT, UI_TOP_Y + 100); tft.drawString(String(linesCleared), UI_PANEL_RIGHT, UI_TOP_Y + 120);
  int nextLabelX = tft.width() - 20;
  tft.drawString("Next:", nextLabelX, UI_TOP_Y);
  const int PREV_BOX_X = nextLabelX - (4 * falling_blocks_BLOCK_SIZE), PREV_BOX_Y = UI_TOP_Y + 20;
  for (int i = 0; i < 4; i++) { for (int j = 0; j < 4; j++) { if (falling_blocksShapes[nextPiece.type][nextPiece.rotation][i][j]) { tft.fillRect(PREV_BOX_X + j * falling_blocks_BLOCK_SIZE, PREV_BOX_Y + i * falling_blocks_BLOCK_SIZE, falling_blocks_BLOCK_SIZE, falling_blocks_BLOCK_SIZE, blockColors[nextPiece.type + 1]); tft.drawRect(PREV_BOX_X + j * falling_blocks_BLOCK_SIZE, PREV_BOX_Y + i * falling_blocks_BLOCK_SIZE, falling_blocks_BLOCK_SIZE, falling_blocks_BLOCK_SIZE, TFT_DARKGREY); } } }
  tft.setTextDatum(MC_DATUM);
  drawRoundBezel();
}
void initfalling_blocksGame() {
  Serial.println("Initializing falling_blocks...");
  tft.fillScreen(TFT_BLACK);
  memset(falling_blocksBoard, 0, sizeof(falling_blocksBoard));
  score = 0; level = 1; linesCleared = 0; gameOver = false;
  fallInterval = 1000; lastFallTime = millis(); lastDropCombo = 0;
  randomSeed(millis());
  nextPiece.type = random(7); nextPiece.rotation = 0;
  spawnNewPiece();
  drawfalling_blocksBoard();
  drawGameUI();
  drawTetromino(currentPiece, blockColors[currentPiece.type + 1]);
}
void resetfalling_blocksGame() { initfalling_blocksGame(); }

void showGameOverScreen() {
    tft.fillScreen(TFT_BLACK);
    drawCenteredString("GAME OVER!", 80, TFT_RED, 3);
    long finalScore = (selectedGame == 0) ? score : brick_breakerScore;
    drawCenteredString("Score: " + String(finalScore), 120, TFT_WHITE, 2);
    drawCenteredString("BLUE = Play Again", 160, TFT_GREEN, 1);
    drawCenteredString("RED (hold) = Back", 185, TFT_WHITE, 1);
    drawRoundBezel();
}
void falling_blocksLoop() {
  if (gameOver) return;
  unsigned long now = millis();
  if (now - gameLoopLastCall < gameLoopDelay) return;
  gameLoopLastCall = now;
  if (now - lastFallTime > fallInterval) {
    drawTetromino(currentPiece, TFT_BLACK);
    if (!checkCollision(currentPiece, currentPiece.x, currentPiece.y + 1, currentPiece.rotation)) {
      currentPiece.y++;
      drawTetromino(currentPiece, blockColors[currentPiece.type + 1]);
    } else {
      placeTetromino();
      if (!gameOver) { clearLines(); drawfalling_blocksBoard(); spawnNewPiece(); if (!gameOver) { drawGameUI(); drawTetromino(currentPiece, blockColors[currentPiece.type + 1]); } }
    }
    lastFallTime = now;
  }
}

// --- Button Handling ---
void handleButtonPress(const char* name) {
  if (currentMode == MODE_AQ) { if (strcmp(name, "GREEN") == 0) { currentMode = MODE_GAME_SELECT; drawGameMenu(); } } 
  else if (currentMode == MODE_GAME_SELECT) {
    if (strcmp(name, "RED") == 0) { currentMode = MODE_AQ; lastBucket = -1; }
    else if (strcmp(name, "GREEN") == 0) { selectedGame = (selectedGame + 1) % 2; drawGameMenu(); }
    else if (strcmp(name, "BLUE") == 0) {
      currentMode = MODE_GAME;
      gameOver = false;
      if (selectedGame == 0) initfalling_blocksGame();
      else if (selectedGame == 1) initbrick_breakerGame();
    }
  } 
  else if (currentMode == MODE_GAME) {
    if (gameOver) {
      if (strcmp(name, "BLUE") == 0) {
        if (selectedGame == 0) resetfalling_blocksGame();
        else if (selectedGame == 1) initbrick_breakerGame();
      }
      return;
    }
    if (selectedGame == 0) { // falling_blocks
      drawTetromino(currentPiece, TFT_BLACK);
      Tetromino tempPiece = currentPiece;
      if (strcmp(name, "RED") == 0) { tempPiece.x--; } else if (strcmp(name, "GREEN") == 0) { tempPiece.rotation = (tempPiece.rotation + 1) % 4; } else if (strcmp(name, "BLUE") == 0) { tempPiece.x++; }
      if (!checkCollision(tempPiece, tempPiece.x, tempPiece.y, tempPiece.rotation)) currentPiece = tempPiece;
      drawTetromino(currentPiece, blockColors[currentPiece.type + 1]);
    } else if (selectedGame == 1) { // brick_breaker
      if (strcmp(name, "GREEN") == 0 && ballStuck) ballStuck = false;
    }
  }
}

void checkButtons() {
  unsigned long now = millis();
  if (now - lastDebounce < debounceMs) return;
  int r = digitalRead(BTN_RED), g = digitalRead(BTN_GREEN), b = digitalRead(BTN_BLUE);
  if (prevRed == HIGH && r == LOW) { redPressStart = now; handleButtonPress("RED"); lastDebounce = now; }
  if (prevGreen == HIGH && g == LOW) { handleButtonPress("GREEN"); lastDebounce = now; }
  if (prevBlue == HIGH && b == LOW) { handleButtonPress("BLUE"); lastDebounce = now; }
  if (currentMode == MODE_GAME && selectedGame == 0 && !gameOver) { if (r == LOW && b == LOW && now - lastDropCombo > 500) { drawTetromino(currentPiece, TFT_BLACK); while (!checkCollision(currentPiece, currentPiece.x, currentPiece.y + 1, currentPiece.rotation)) { currentPiece.y++; } placeTetromino(); if (!gameOver) { clearLines(); drawfalling_blocksBoard(); spawnNewPiece(); if (!gameOver) { drawGameUI(); drawTetromino(currentPiece, blockColors[currentPiece.type + 1]); } } lastFallTime = now; lastDropCombo = now; } }
  if (currentMode == MODE_GAME) { if (r == LOW) { if (redPressStart != 0 && (now - redPressStart > 2000)) { currentMode = MODE_GAME_SELECT; drawGameMenu(); redPressStart = 0; } } else { redPressStart = 0; } }
  prevRed = r; prevGreen = g; prevBlue = b;
}

// --- Arduino Setup ---
void setup() {
  Serial.begin(115200);
  pinMode(BTN_RED, INPUT_PULLUP); pinMode(BTN_GREEN, INPUT_PULLUP); pinMode(BTN_BLUE, INPUT_PULLUP);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextWrap(false);
  
  if (!initialBootSplashComplete) {
    showSplash();
    initialBootSplashComplete = true;
  }

  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  delay(800);
  if (!aqi.begin_UART(&Serial2)) {
    sensorOk = false;
    Serial.println("PM2.5 UART init failed (check wiring)");
  } else {
    sensorOk = true;
    Serial.println("PM2.5 sensor ready (UART)");
    showScanningAnimation();
  }
}

// --- Main Loop ---
void loop() {
  checkButtons();

  switch (currentMode) {
    case MODE_AQ: {
      if (sensorOk) {
        PM25_AQI_Data data;
        if (aqi.read(&data)) { lastPm25 = data.pm25_standard; }
        int bucket = (lastPm25 <= 12) ? 0 : (lastPm25 <= 35) ? 1 : (lastPm25 <= 55) ? 2 : 3;
        if (bucket != lastBucket) { drawAQStatic(lastPm25); lastBucket = bucket; }
        drawAQNumberBreathing(lastPm25, breathePhase);
        breathePhase += 0.05f;
        if (breathePhase > TWO_PI) breathePhase = 0.0f;
      } else {
        static bool drawn = false;
        if (!drawn) {
          tft.fillScreen(TFT_BLACK);
          drawCenteredString("Sensor Error", 120, TFT_RED, 2);
          drawRoundBezel();
          drawn = true;
        }
        delay(300);
      }
      delay(80);
      break;
    }
    case MODE_GAME_SELECT:
      delay(50);
      break;
    case MODE_GAME:
      if (selectedGame == 0) { falling_blocksLoop(); }
      else if (selectedGame == 1) { brick_breakerLoop(); }
      break;
  }
}
