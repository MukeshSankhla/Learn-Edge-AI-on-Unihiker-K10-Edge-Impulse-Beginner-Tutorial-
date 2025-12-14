#include "unihiker_k10.h"
#include "arduino_image_cache.h"

UNIHIKER_K10 k10;
uint8_t      screen_dir=2;
AHT20        aht20;

void setup() {
	k10.begin();
	k10.initScreen(screen_dir);
	k10.creatCanvas();
}
void loop() {
	k10.canvas->canvasDrawBitmap(0,0,240,320,image_data1);
	k10.canvas->canvasText((String(aht20.getData(AHT20::eAHT20TempC)) + String("C")), 116, 85, 0xFFFFFF, k10.canvas->eCNAndENFont24, 50, false);
	k10.canvas->canvasText((String(aht20.getData(AHT20::eAHT20HumiRH)) + String("%")), 116, 120, 0xFFFFFF, k10.canvas->eCNAndENFont24, 50, false);
	k10.canvas->canvasText("PARTLY_CLOUDY", 25, 210, 0xFFFFFF, k10.canvas->eCNAndENFont24, 50, false);
	k10.canvas->updateCanvas();
}