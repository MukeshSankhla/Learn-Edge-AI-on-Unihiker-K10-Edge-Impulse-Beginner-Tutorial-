/*
Learn Edge AI on UNIHIKER K10
Edge Impulse Beginner Tutorial
Mukesh Sankhla | makerbrains.com
*/

#include "unihiker_k10.h"
#include <Environmental_Sensor_Regression_inferencing.h>  // Edge Impulse model

UNIHIKER_K10 k10;
AHT20 aht20;

uint8_t screen_dir = 2;
int last_comfort_score = -999;    // impossible initial value

void setup() {
    Serial.begin(115200);

    k10.begin();
    k10.initSDFile();
    k10.initScreen(screen_dir);
    k10.creatCanvas();

    aht20.begin();
}

void loop() {

    // 1. Read sensors
    float temperature = aht20.getData(AHT20::eAHT20TempC);
    float humidity    = aht20.getData(AHT20::eAHT20HumiRH);

    // 2. Prepare features
    float features[2] = { temperature, humidity };

    // 3. Create impulse signal wrapper
    ei_signal_t signal;
    signal.total_length = 2;

    signal.get_data = [&](size_t offset, size_t length, float *out) {
        for (size_t i = 0; i < length; i++) {
            out[i] = features[offset + i];
        }
        return EI_IMPULSE_OK;
    };

    // 4. Run inference
    ei_impulse_result_t result;
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

    if (err != EI_IMPULSE_OK) {
        Serial.println("Inference failed!");
        return;
    }

    // 5. Get regression output
    int comfort_score = result.classification[0].value;

    Serial.print("Comfort Score = ");
    Serial.println(comfort_score);

    // 6. REFRESH ONLY IF SCORE CHANGES
    if (comfort_score != last_comfort_score) {

        k10.canvas->canvasClear();
        k10.canvas->canvasDrawImage(0, 0, "S:/weather.png");

        k10.canvas->canvasText("T: " + String(int(temperature)) + " C",
            120, 90, 0xFFFFFF, k10.canvas->eCNAndENFont24, 50, false);

        k10.canvas->canvasText("H: " + String(int(humidity)) + " %",
            120, 120, 0xFFFFFF, k10.canvas->eCNAndENFont24, 50, false);

        k10.canvas->canvasText("S: " + String(comfort_score),
            120, 150, 0xFFFFFF, k10.canvas->eCNAndENFont24, 50, false);

        k10.canvas->updateCanvas();

        last_comfort_score = comfort_score;   // update last score
    }

    delay(200);   // faster updates without flicker
}
