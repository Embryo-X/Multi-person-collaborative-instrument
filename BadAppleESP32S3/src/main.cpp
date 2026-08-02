#include <Arduino.h>
#include <SPIFFS.h>
#include <Arduino_GFX_Library.h>
#include <AudioFileSourceSPIFFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

namespace AmpPins {
constexpr int LRC = 4;
constexpr int BCLK = 5;
constexpr int DIN = 6;
constexpr int GAIN = 7;
constexpr int SD = 15;
}

namespace DisplayPins {
constexpr int SCLK = 20;
constexpr int MOSI = 21;
constexpr int RST = 47;
constexpr int DC = 48;
constexpr int CS = 45;
constexpr int BLK = 35;
}

struct ButtonPair {
    uint8_t inputPin;
    uint8_t lowPin;
    bool rawState;
    bool stableState;
    uint32_t changedAt;
};

ButtonPair buttons[] = {
    {17, 18, false, false, 0},
    {8, 3, false, false, 0},
    {46, 9, false, false, 0},
};

constexpr uint8_t BUTTON_COUNT = 3;
constexpr uint8_t NOTE_COUNT = 88;
constexpr float OUTPUT_GAIN = 0.50F;
constexpr uint32_t NOTE_DURATION_MS = 500;
constexpr uint32_t DEBOUNCE_MS = 25;

constexpr uint16_t C_BLACK = 0x0000;
constexpr uint16_t C_WHITE = 0xFFFF;
constexpr uint16_t C_CYAN = 0x07FF;
constexpr uint16_t C_BLUE = 0x0255;
constexpr uint16_t C_GREEN = 0x07E0;
constexpr uint16_t C_RED = 0xF800;
constexpr uint16_t C_YELLOW = 0xFFE0;
constexpr uint16_t C_DARK = 0x1082;

Arduino_DataBus *displayBus = new Arduino_ESP32SPI(
    DisplayPins::DC, DisplayPins::CS, DisplayPins::SCLK, DisplayPins::MOSI,
    GFX_NOT_DEFINED, FSPI, false);
Arduino_GFX *display = new Arduino_NV3007(
    displayBus, DisplayPins::RST, 0, true, 142, 428,
    12, 0, 14, 0,
    nv3007_279_init_operations, sizeof(nv3007_279_init_operations));

AudioFileSourceSPIFFS *audioFile = nullptr;
AudioGeneratorMP3 *decoder = nullptr;
AudioOutputI2S *audioOutput = nullptr;

uint8_t scoreNote = 0;
uint8_t expectedButton = 0;
int16_t lastAssignedNote[BUTTON_COUNT] = {-1, -1, -1};
int8_t lastPressedButton = -1;
int16_t lastPlayedRoot = -1;
bool lastPressCorrect = false;
bool advanceAfterPlayback = false;
bool noteWindowActive = false;
uint32_t noteStartedAt = 0;

void noteName(uint8_t sampleIndex, char *buffer, size_t length) {
    static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const uint8_t midi = 21 + sampleIndex;
    snprintf(buffer, length, "%s%d", names[midi % 12], static_cast<int>(midi / 12) - 1);
}

void centeredText(const char *text, int16_t y, uint8_t size, uint16_t color) {
    int16_t x = (display->width() - static_cast<int16_t>(strlen(text) * 6 * size)) / 2;
    if (x < 0) x = 0;
    display->setTextSize(size);
    display->setTextColor(color);
    display->setCursor(x, y);
    display->print(text);
}

void drawInterface() {
    display->fillScreen(C_BLACK);
    display->fillRect(0, 0, display->width(), 54, C_BLUE);
    centeredText("PIANO", 8, 2, C_WHITE);
    centeredText("SCORE", 30, 2, C_CYAN);

    display->drawRoundRect(6, 66, 130, 112, 8, C_CYAN);
    centeredText("TARGET", 78, 1, C_CYAN);
    char line[32];
    snprintf(line, sizeof(line), "KEY %u", expectedButton + 1);
    centeredText(line, 102, 3, C_WHITE);
    snprintf(line, sizeof(line), "STEP %02u/88", scoreNote + 1);
    centeredText(line, 145, 1, C_YELLOW);

    display->drawFastHLine(12, 194, 118, C_DARK);
    if (lastPressedButton < 0) {
        centeredText("WAITING", 222, 2, C_YELLOW);
        centeredText("PRESS LIT KEY", 258, 1, C_WHITE);
    } else {
        centeredText("PRESSED", 208, 1, C_CYAN);
        snprintf(line, sizeof(line), "KEY %u", lastPressedButton + 1);
        centeredText(line, 228, 2, C_WHITE);
        centeredText(lastPressCorrect ? "CORRECT" : "WRONG", 258, 2,
                     lastPressCorrect ? C_GREEN : C_RED);

        centeredText("ACTUAL SOUND", 300, 1, C_CYAN);
        if (lastPlayedRoot >= 0) {
            char rootName[8];
            noteName(static_cast<uint8_t>(lastPlayedRoot), rootName, sizeof(rootName));
            centeredText(rootName, 326, 2, C_YELLOW);
        } else {
            centeredText("NO MEMORY", 326, 2, C_RED);
        }
    }

    display->drawFastHLine(12, 382, 118, C_DARK);
    centeredText("VOLUME 0.50", 398, 1, C_WHITE);
}

void assignCurrentNote() {
    expectedButton = static_cast<uint8_t>(random(BUTTON_COUNT));
    lastAssignedNote[expectedButton] = scoreNote;
    drawInterface();
    Serial.printf("Assigned note %u/88 to button %u\n", scoreNote + 1, expectedButton + 1);
}

void closeAudio() {
    if (decoder != nullptr) {
        if (decoder->isRunning()) decoder->stop();
        delete decoder;
        decoder = nullptr;
    }
    if (audioFile != nullptr) {
        audioFile->close();
        delete audioFile;
        audioFile = nullptr;
    }
}

int8_t readNewPress() {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        const bool pressed = digitalRead(buttons[i].inputPin) == LOW;
        if (pressed != buttons[i].rawState) {
            buttons[i].rawState = pressed;
            buttons[i].changedAt = now;
        }
        if (now - buttons[i].changedAt >= DEBOUNCE_MS && pressed != buttons[i].stableState) {
            buttons[i].stableState = pressed;
            if (pressed) return static_cast<int8_t>(i);
        }
    }
    return -1;
}

void startNote(uint8_t root, uint8_t buttonIndex, bool correct) {
    closeAudio();
    lastPressedButton = buttonIndex;
    lastPlayedRoot = root;
    lastPressCorrect = correct;
    advanceAfterPlayback = correct;
    drawInterface();

    char path[24];
    snprintf(path, sizeof(path), "/piano/%02u.mp3", root + 1);
    audioFile = new AudioFileSourceSPIFFS(path);
    decoder = new AudioGeneratorMP3();
    if (!decoder->begin(audioFile, audioOutput)) {
        Serial.printf("Failed to play %s\n", path);
        closeAudio();
    }
    noteStartedAt = millis();
    noteWindowActive = true;
    Serial.printf("Button %u plays sample %u (%s)\n",
                  buttonIndex + 1, root + 1,
                  correct ? "correct" : "remembered wrong key");
}

void setup() {
    Serial.begin(115200);
    delay(800);

    pinMode(AmpPins::SD, OUTPUT);
    digitalWrite(AmpPins::SD, HIGH);
    pinMode(AmpPins::GAIN, INPUT);

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        pinMode(buttons[i].lowPin, OUTPUT);
        digitalWrite(buttons[i].lowPin, LOW);
        pinMode(buttons[i].inputPin, INPUT_PULLUP);
    }

    pinMode(DisplayPins::BLK, OUTPUT);
    digitalWrite(DisplayPins::BLK, HIGH);
    if (!display->begin(40000000)) {
        Serial.println("NV3007 display initialization failed");
    }

    if (!SPIFFS.begin(false)) {
        display->fillScreen(C_RED);
        centeredText("SPIFFS ERROR", 200, 1, C_WHITE);
        Serial.println("SPIFFS mount failed");
        while (true) delay(1000);
    }

    audioOutput = new AudioOutputI2S();
    audioOutput->SetPinout(AmpPins::BCLK, AmpPins::LRC, AmpPins::DIN);
    audioOutput->SetGain(OUTPUT_GAIN);

    randomSeed(esp_random());
    Serial.println("NV3007 score mode started; single-note gain 0.50.");
    assignCurrentNote();
}

void loop() {
    const uint32_t now = millis();
    const int8_t pressed = readNewPress();

    if (noteWindowActive) {
        if (decoder != nullptr && decoder->isRunning() && !decoder->loop()) {
            decoder->stop();
        }
        if (now - noteStartedAt >= NOTE_DURATION_MS) {
            closeAudio();
            noteWindowActive = false;
            if (advanceAfterPlayback) {
                ++scoreNote;
                if (scoreNote >= NOTE_COUNT) scoreNote = 0;
                assignCurrentNote();
            } else {
                drawInterface();
            }
        }
    } else if (pressed >= 0) {
        if (static_cast<uint8_t>(pressed) == expectedButton) {
            startNote(scoreNote, static_cast<uint8_t>(pressed), true);
        } else if (lastAssignedNote[pressed] >= 0) {
            startNote(static_cast<uint8_t>(lastAssignedNote[pressed]), static_cast<uint8_t>(pressed), false);
        } else {
            lastPressedButton = pressed;
            lastPlayedRoot = -1;
            lastPressCorrect = false;
            drawInterface();
            Serial.printf("Button %d has no remembered note yet\n", pressed + 1);
        }
    }
    delay(1);
}
