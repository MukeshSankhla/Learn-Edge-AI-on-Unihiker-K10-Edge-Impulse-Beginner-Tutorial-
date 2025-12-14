/*
Learn Edge AI on UNIHIKER K10
Edge Impulse Beginner Tutorial
Mukesh Sankhla | makerbrains.com
*/

#include <Motion_Data_Classification_inferencing.h>   // Edge Impulse library
#include "unihiker_k10.h"

UNIHIKER_K10 k10;
uint8_t screen_dir = 2;

// Sampling rate must match your impulse design (Hz)
#define SAMPLE_RATE_HZ 47
#define INTERVAL_MS (1000 / SAMPLE_RATE_HZ)

// Input buffer for Edge Impulse model
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
static unsigned long last_sample_time = 0;
static int feature_index = 0;

void setup() {
    Serial.begin(115200);
    k10.begin();
    k10.initSDFile();
    k10.initScreen(screen_dir);
    k10.creatCanvas();
    k10.setScreenBackground(0xFFFFFF);

    k10.rgb->brightness(0);
    k10.rgb->write(-1, 0x000000);

    k10.canvas->canvasText("UNIHIKER Gesture Classifier", 2, 0x000000);
    k10.canvas->canvasText("Loading model...", 3, 0x000000);
    k10.canvas->updateCanvas();
    delay(1500);

    k10.setScreenBackground(0xFFFFFF);
    k10.canvas->canvasText("Ready for motion!", 2, 0x000000);
    k10.canvas->updateCanvas();

    last_sample_time = millis();
}

void loop() {
    if (millis() - last_sample_time >= INTERVAL_MS) {
        last_sample_time = millis();

        float accx = k10.getAccelerometerX();
        float accy = k10.getAccelerometerY();
        float accz = k10.getAccelerometerZ();

        features[feature_index++] = accx;
        features[feature_index++] = accy;
        features[feature_index++] = accz;

        if (feature_index >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
            run_inference();
            feature_index = 0;
        }
    }
}

void run_inference() {
    ei_impulse_result_t result = {0};

    signal_t signal;
    numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        Serial.println("Error running classifier!");
        return;
    }

    // Find top prediction
    size_t best_index = 0;
    float best_value = 0;
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if (result.classification[ix].value > best_value) {
            best_value = result.classification[ix].value;
            best_index = ix;
        }
    }

    const char *gesture = result.classification[best_index].label;

    // =============================
    //   RGB COLOR + IMAGE DISPLAY
    // =============================
    uint32_t color = 0x000000;
    const char *image_path = "S:/default.png";

    if (strcmp(gesture, "Idle") == 0) {
        color = 0x00FF00;
        image_path = "S:/EMJ1.png";
    } 
    else if (strcmp(gesture, "UD") == 0) {
        color = 0xFF00FF;
        image_path = "S:/EMJ2.png";
    } 
    else if (strcmp(gesture, "Shake") == 0) {
        color = 0xFF0000;
        image_path = "S:/EMJ3.png";
    } 
    else if (strcmp(gesture, "LR") == 0) {
        color = 0x0000FF;
        image_path = "S:/EMJ4.png";
    }

    k10.rgb->brightness(10);
    k10.rgb->write(-1, color);

    // =============================
    //   SCREEN: SHOW IMAGE + SCORES
    // =============================
    k10.setScreenBackground(0xFFFFFF);
    k10.canvas->canvasDrawImage(0, 0, image_path);


    // char buffer[64];
    // int row = 6;

    // for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    //     snprintf(buffer, sizeof(buffer), "%s: %.1f%%",
    //              result.classification[ix].label,
    //              result.classification[ix].value * 100);

    //     uint32_t text_color = (ix == best_index) ? color : 0x000000;
    //     k10.canvas->canvasText(buffer, row++, text_color);
    // }

    k10.canvas->updateCanvas();

    // Serial Monitor Output
    Serial.println("\n=== Results ===");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        Serial.print(result.classification[ix].label);
        Serial.print(": ");
        Serial.print(result.classification[ix].value * 100, 1);
        Serial.println("%");
    }
    Serial.println("--------------------------");
}
