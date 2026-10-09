// Btns 0.6
// by Leo Kuroshita for Hügelton instruments, modified by jhbruhn.
// 16x8 variant: 8 rows x 16 columns, 128 SK6812 LEDs, no tilt sensor.

#include "MonomeSerialDevice.h"
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

#define NUM_ROWS 8
#define NUM_COLS 16
#define NUM_LEDS (NUM_ROWS * NUM_COLS)
#define LED_PIN 28

// 0 = USB connector on the left (default), 2 = rotated by 180 degrees.
// Hold the top-left key (default) or the bottom-right key (180 degrees) while plugging in.
#define DEFAULT_ROTATION 0
#define DEFAULT_FLIP_HORIZONTAL false
#define DEFAULT_FLIP_VERTICAL false

// LED power budget. SK6812-EC20: 12 mA per colour channel at full scale, about 1 mA idle per LED.
// All values of a frame are scaled down together if the estimate exceeds the budget.
#define LED_MA_PER_CHANNEL 12
#define LED_MA_IDLE 1
#define MAX_LED_CURRENT_MA 400

// Brightness setting: hold the two top corner keys (row 1, column 1 and column 16) for
// SETTINGS_HOLD_MS. Then press any key: its column sets the maximum brightness
// (column 1 = 1/16, column 16 = 100 %). The setting is stored and the mode ends
// SETTINGS_TIMEOUT_MS after the last key press. No key events reach the host in this mode.
#define SETTINGS_HOLD_MS 1000
#define SETTINGS_TIMEOUT_MS 3000
#define EEPROM_MAGIC 0xB5
#define EEPROM_ADDR_MAGIC 0
#define EEPROM_ADDR_BRIGHTNESS 1

// Matrix timing in microseconds
#define COL_SETTLE_US 5
#define COL_RELEASE_US 20

// SW_ROW_1..8 -> GPIO0..7
const uint8_t ROW_PINS[NUM_ROWS] = {0, 1, 2, 3, 4, 5, 6, 7};
// SW_COL_1..16, physical order left to right.
// Columns 1-7: GPIO8-14, columns 8-15: GPIO16-22 and GPIO26, column 16: GPIO15.
const uint8_t COL_PINS[NUM_COLS] = {8, 9, 10, 11, 12, 13, 14, 16, 17, 18, 19, 20, 21, 22, 26, 15};
const uint8_t gammaTable[16] = { 0,  2,  3,  6,  11, 18, 25, 32, 41, 59, 70, 80, 92, 103, 115, 127};
const uint8_t gammaAdj = 2;

bool isInited = false;
String deviceID = "btns";
String serialNum = "m4216124";

char mfgstr[32] = "monome";
char prodstr[32] = "monome";
char serialstr[32] = "m4216124";

Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

MonomeSerialDevice mdp;

bool buttonStates[NUM_ROWS][NUM_COLS] = {0};
uint32_t rowMask = 0;

uint8_t maxBrightness = 16;      // 1..16, 16 = 100 %
bool settingsMode = false;
bool settingsWaitRelease = false;
unsigned long comboSince = 0;
unsigned long settingsLastActivity = 0;
uint8_t settingsPrevious = 16;

int gridRotation = DEFAULT_ROTATION;
bool flipHorizontal = DEFAULT_FLIP_HORIZONTAL;
bool flipVertical = DEFAULT_FLIP_VERTICAL;

void updateLEDMatrix();
void loadSettings();
void saveSettings();
void scanButtonMatrix();
void mapPhysicalToGrid(int row, int col, int &y, int &x);
uint8_t readColumn(int col);
bool detectGridOrientation();

// Maps a physical position (row, col) to grid coordinates (y, x).
// Only 0 and 180 degrees keep the 16x8 shape, both mappings are their own inverse.
void mapPhysicalToGrid(int row, int col, int &y, int &x) {
    y = row;
    x = col;

    if (gridRotation == 2) {
        y = NUM_ROWS - 1 - y;
        x = NUM_COLS - 1 - x;
    }

    if (flipHorizontal) x = NUM_COLS - 1 - x;
    if (flipVertical) y = NUM_ROWS - 1 - y;
}

// Drives one column low and returns the state of all rows as a bitmask (bit = row, 1 = pressed).
uint8_t readColumn(int col) {
    digitalWrite(COL_PINS[col], LOW);
    delayMicroseconds(COL_SETTLE_US);
    uint32_t in = ~gpio_get_all() & rowMask;
    digitalWrite(COL_PINS[col], HIGH);
    // let the row pull-ups recharge the row lines before the next column is selected
    delayMicroseconds(COL_RELEASE_US);

    uint8_t result = 0;
    for (int row = 0; row < NUM_ROWS; row++) {
        if (in & (1u << ROW_PINS[row])) result |= (1 << row);
    }
    return result;
}

bool detectGridOrientation() {
    gridRotation = DEFAULT_ROTATION;
    if (readColumn(0) & 0x01) {
        gridRotation = 0;
        return true;
    }
    if (readColumn(NUM_COLS - 1) & (1 << (NUM_ROWS - 1))) {
        gridRotation = 2;
        return true;
    }
    return false;
}

void setup() {
    USBDevice.setManufacturerDescriptor(mfgstr);
    USBDevice.setProductDescriptor(prodstr);
    USBDevice.setSerialDescriptor(serialstr);

    pinMode(LED_BUILTIN, OUTPUT);

    for (int i = 0; i < NUM_ROWS; i++) {
        pinMode(ROW_PINS[i], INPUT_PULLUP);
        rowMask |= (1u << ROW_PINS[i]);
    }
    for (int i = 0; i < NUM_COLS; i++) {
        pinMode(COL_PINS[i], OUTPUT);
        digitalWrite(COL_PINS[i], HIGH);
    }
    delay(1);

    loadSettings();
    detectGridOrientation();

    mdp.isMonome = true;
    mdp.deviceID = deviceID;
    mdp.setupAsGrid(NUM_ROWS, NUM_COLS);

    isInited = true;
    mdp.poll();

    // Send grid size and rotation information
    mdp.sendSysSize();
    mdp.sendSysRotation();
    pixels.begin();
    pixels.clear();
    pixels.show();
}

void loop() {
    static unsigned long lastCheck = 0;
    unsigned long currentMillis = millis();

    mdp.poll();

    if (currentMillis - lastCheck >= 15) {
        lastCheck = currentMillis;
        scanButtonMatrix();
        updateLEDMatrix();
    }
}

void loadSettings() {
    EEPROM.begin(256);
    if (EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC) {
        uint8_t v = EEPROM.read(EEPROM_ADDR_BRIGHTNESS);
        if (v >= 1 && v <= 16) maxBrightness = v;
    }
}

void saveSettings() {
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.write(EEPROM_ADDR_BRIGHTNESS, maxBrightness);
    EEPROM.commit();
}

void scanButtonMatrix() {
    bool current[NUM_ROWS][NUM_COLS];
    bool anyPressed = false;
    for (int col = 0; col < NUM_COLS; col++) {
        uint8_t pressed = readColumn(col);
        for (int row = 0; row < NUM_ROWS; row++) {
            current[row][col] = pressed & (1 << row);
            anyPressed |= current[row][col];
        }
    }
    unsigned long now = millis();

    if (settingsMode) {
        if (settingsWaitRelease) {
            // ignore everything until all keys (including the combo) are released
            if (!anyPressed) settingsWaitRelease = false;
        } else {
            for (int row = 0; row < NUM_ROWS; row++) {
                for (int col = 0; col < NUM_COLS; col++) {
                    if (current[row][col] && !buttonStates[row][col]) {
                        int x, y;
                        mapPhysicalToGrid(row, col, y, x);
                        maxBrightness = x + 1;
                        settingsLastActivity = now;
                    }
                }
            }
        }
        if (anyPressed) settingsLastActivity = now;
        memcpy(buttonStates, current, sizeof(buttonStates));

        if (!anyPressed && now - settingsLastActivity >= SETTINGS_TIMEOUT_MS) {
            settingsMode = false;
            comboSince = 0;
            if (maxBrightness != settingsPrevious) saveSettings();
        }
        return;
    }

    for (int col = 0; col < NUM_COLS; col++) {
        for (int row = 0; row < NUM_ROWS; row++) {
            bool currentState = current[row][col];
            if (currentState != buttonStates[row][col]) {
                int x, y;
                mapPhysicalToGrid(row, col, y, x);
                mdp.sendGridKey(x, y, currentState);
                buttonStates[row][col] = currentState;
            }
        }
    }

    // both top corner keys (grid coordinates, follows the rotation) held -> brightness setting
    int r0, c0, r1, c1;
    mapPhysicalToGrid(0, 0, r0, c0);                      // involution: grid (0,0) -> physical
    mapPhysicalToGrid(0, NUM_COLS - 1, r1, c1);           // grid (0,15) -> physical
    if (current[r0][c0] && current[r1][c1]) {
        if (comboSince == 0) {
            comboSince = now ? now : 1;
        } else if (now - comboSince >= SETTINGS_HOLD_MS) {
            // release all held keys towards the host, then take over the matrix
            for (int row = 0; row < NUM_ROWS; row++) {
                for (int col = 0; col < NUM_COLS; col++) {
                    if (buttonStates[row][col]) {
                        int x, y;
                        mapPhysicalToGrid(row, col, y, x);
                        mdp.sendGridKey(x, y, 0);
                    }
                }
            }
            settingsMode = true;
            settingsWaitRelease = true;
            settingsPrevious = maxBrightness;
            settingsLastActivity = now;
        }
    } else {
        comboSince = 0;
    }
}

void updateLEDMatrix() {
    static uint8_t level[NUM_LEDS];
    uint32_t sum = 0;

    // LED chain runs row by row, 16 LEDs per row, starting at the top left
    for (int row = 0; row < NUM_ROWS; row++) {
        for (int col = 0; col < NUM_COLS; col++) {
            int x, y;
            mapPhysicalToGrid(row, col, y, x);
            uint8_t intensity;
            if (settingsMode) {
                // selected column at full level, columns below it dim
                if (x == maxBrightness - 1) intensity = 255;
                else if (x < maxBrightness - 1) intensity = 12;
                else intensity = 0;
            } else {
                intensity = gammaTable[mdp.leds[y * NUM_COLS + x] & 0x0F] * gammaAdj;
            }
            intensity = (uint16_t)intensity * maxBrightness / 16;
            level[row * NUM_COLS + col] = intensity;
            // colour is (i/2, i, i/2)
            sum += intensity + 2 * (intensity / 2);
        }
    }

    // estimated current in mA
    uint32_t budget = MAX_LED_CURRENT_MA - NUM_LEDS * LED_MA_IDLE;
    uint32_t estimate = sum * LED_MA_PER_CHANNEL / 255;
    uint32_t scale = 256;
    if (estimate > budget) scale = budget * 256 / estimate;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t intensity = (level[i] * scale) >> 8;
        pixels.setPixelColor(i, pixels.Color(intensity / 2, intensity, intensity / 2));
    }
    pixels.show();
}
