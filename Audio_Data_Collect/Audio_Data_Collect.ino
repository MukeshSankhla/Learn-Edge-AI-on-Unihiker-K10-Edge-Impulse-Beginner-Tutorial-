/*
Learn Edge AI on UNIHIKER K10
Edge Impulse Beginner Tutorial
Mukesh Sankhla | makerbrains.com
*/

#include "unihiker_k10.h"
#include <stdlib.h>

UNIHIKER_K10 k10;
uint8_t screen_dir = 2;
Music music;

// ---------------- Settings -----------------
String labels[] = {"Noise", "Gun", "Chainsaw"};
int labelCount = 3;
int currentLabelIndex = 0;

int recordDuration = 1;
String lastSavedFile = "None";

// ---------- Button long press config ----------
unsigned long pressStartA = 0;
unsigned long pressStartB = 0;
bool longPressHandledA = false;
bool longPressHandledB = false;

const unsigned long LONG_PRESS_TIME = 700;

// --------- Double click tracking for B -------------
unsigned long lastClickTimeB = 0;
bool waitingForSecondClickB = false;

// ----------- Generate random 5-digit number ----------
String randomID() {
    int num = random(10000, 99999);
    return String(num);
}

// ----------- Update Screen ----------
void updateScreen() {
    // Clear canvas
    // k10.canvas->canvasClear();

    // Header bar with gradient effect
    k10.canvas->canvasText("Audio Recorder", 25, 15, 0x34495E,
        k10.canvas->eCNAndENFont24, 24, true);

    // Main content area with card-like design
    // Label card
    k10.canvas->canvasText("Current Label", 25, 72, 0x7F8C8D,
        k10.canvas->eCNAndENFont24, 20, true);
    k10.canvas->canvasText(labels[currentLabelIndex], 25, 100, 0x27AE60,
        k10.canvas->eCNAndENFont24, 28, true);

    // Duration card
    k10.canvas->canvasText("Duration", 25, 157, 0x7F8C8D,
        k10.canvas->eCNAndENFont24, 20, true);
    k10.canvas->canvasText(String(recordDuration) + " seconds", 25, 185, 0x3498DB,
        k10.canvas->eCNAndENFont24, 28, true);

    // Last saved file section
    k10.canvas->canvasText("Last Saved", 25, 235, 0x7F8C8D,
        k10.canvas->eCNAndENFont24, 25, true);
    
    // Truncate filename if too long
    String displayName = lastSavedFile;
    if (displayName.length() > 20) {
        displayName = displayName.substring(0, 17) + "...";
    }
    k10.canvas->canvasText(displayName, 20, 270, 0xE59F8F,
        k10.canvas->eCNAndENFont24, 20, true);
    
    k10.canvas->canvasLine(10, 50, 230, 50, 0xBED7D9);
    k10.canvas->canvasLine(10, 140, 230, 140, 0xCEECEE);
    k10.canvas->canvasLine(10, 225, 230, 225, 0xCEECEE);

    k10.canvas->updateCanvas();
}

// ---------------- Setup -----------------
void setup() {
    k10.begin();
    k10.initScreen(screen_dir);
    k10.creatCanvas();
    k10.initSDFile();
    randomSeed(analogRead(0));

    // Modern gradient-like background
    k10.setScreenBackground(0xECF0F1);

    updateScreen();
}

// ---------------- Loop -----------------
void loop() {
    bool btnA = k10.buttonA->isPressed();
    bool btnB = k10.buttonB->isPressed();

    // ============================================================
    //                    BUTTON A
    // ============================================================
    if (btnA && pressStartA == 0) {
        pressStartA = millis();
        longPressHandledA = false;
    }

    if (!btnA && pressStartA > 0) {
        unsigned long pressTime = millis() - pressStartA;

        if (pressTime < LONG_PRESS_TIME) {
            // --- SHORT PRESS A → Record ---
            String fileName = labels[currentLabelIndex] + randomID() + ".wav";
            String fullPath = "S:/" + fileName;

            music.recordSaveToTFCard((char*)fullPath.c_str(), recordDuration);
            lastSavedFile = fileName;
            updateScreen();
        }
        pressStartA = 0;
    }

    // Long press A → Previous label
    if (btnA && !longPressHandledA &&
        millis() - pressStartA >= LONG_PRESS_TIME) {

        currentLabelIndex--;
        if (currentLabelIndex < 0) currentLabelIndex = labelCount - 1;

        longPressHandledA = true;
        updateScreen();
    }

    // ============================================================
    //                    BUTTON B
    // ============================================================
    if (btnB && pressStartB == 0) {
        pressStartB = millis();
        longPressHandledB = false;
    }

    // Button B released → detect click / double click
    if (!btnB && pressStartB > 0) {
        unsigned long pressTime = millis() - pressStartB;

        if (pressTime < LONG_PRESS_TIME) {
            // --- Detect double click  ---
            if (waitingForSecondClickB &&
                millis() - lastClickTimeB <= 300) {

                // ===== DOUBLE CLICK B → reset duration =====
                recordDuration = 1;
                waitingForSecondClickB = false;
                updateScreen();

            } else {
                // first click → start waiting for second click
                waitingForSecondClickB = true;
                lastClickTimeB = millis();
            }
        }

        pressStartB = 0;
    }

    // Timeout → treat as single click
    if (waitingForSecondClickB &&
        millis() - lastClickTimeB > 300) {

        // ===== SINGLE CLICK B → Increase duration =====
        recordDuration++;
        if (recordDuration > 10) recordDuration = 10;
        waitingForSecondClickB = false;
        updateScreen();
    }

    // Long press B → Next label
    if (btnB && !longPressHandledB &&
        millis() - pressStartB >= LONG_PRESS_TIME) {

        currentLabelIndex++;
        if (currentLabelIndex >= labelCount) currentLabelIndex = 0;

        longPressHandledB = true;
        waitingForSecondClickB = false;
        updateScreen();
    }
}
