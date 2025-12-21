/*
Learn Edge AI on UNIHIKER K10
Edge Impulse Beginner Tutorial
Mukesh Sankhla | makerbrains.com
*/
/*
 * ESP32-S3 + GC2145 Camera - Edge Impulse Image Capture
 * With ILI9341 Display and XL9535 GPIO Expander
 */

#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Wire.h>

/* =========================================================
   Display Pins (ILI9341)
   ========================================================= */
#define TFT_CS      14
#define TFT_DC      13
#define TFT_MOSI    21
#define TFT_MISO    -1  // Not used
#define TFT_SCLK    12
// TFT_RST and TFT_BLK are controlled via XL9535

/* =========================================================
   SD Card Pins (SPI)
   ========================================================= */
#define SD_CS       40
#define SD_MOSI     42
#define SD_MISO     41
#define SD_SCK      44

/* =========================================================
   I2C Pins (for XL9535)
   ========================================================= */
#define I2C_SDA     47  // P20/SDA
#define I2C_SCL     48  // P19/SCL

/* =========================================================
   XL9535 GPIO Expander
   ========================================================= */
#define XL9535_ADDR 0x20  // I2C address from schematic

/* =========================================================
   Camera Pin Mapping (GC2145)
   ========================================================= */
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     7
#define SIOD_GPIO_NUM     47
#define SIOC_GPIO_NUM     48

#define Y9_GPIO_NUM       6
#define Y8_GPIO_NUM       15
#define Y7_GPIO_NUM       16
#define Y6_GPIO_NUM       18
#define Y5_GPIO_NUM       9
#define Y4_GPIO_NUM       11
#define Y3_GPIO_NUM       10
#define Y2_GPIO_NUM       8

#define VSYNC_GPIO_NUM    4
#define HREF_GPIO_NUM     5
#define PCLK_GPIO_NUM     17

/* =========================================================
   XL9535 I2C Registers
   ========================================================= */
#define XL9535_INPUT_PORT0    0x00
#define XL9535_INPUT_PORT1    0x01
#define XL9535_OUTPUT_PORT0   0x02
#define XL9535_OUTPUT_PORT1   0x03
#define XL9535_INVERT_PORT0   0x04
#define XL9535_INVERT_PORT1   0x05
#define XL9535_CONFIG_PORT0   0x06
#define XL9535_CONFIG_PORT1   0x07

/* =========================================================
   XL9535 Pin Assignments (from schematic)
   ========================================================= */
#define XL9535_LCD_BLK    0   
#define XL9535_CAMERA_RST 1   
#define XL9535_BTN_B      P11  
#define XL9535_BTN_A      P5 

/* =========================================================
   XL9535 Helper Functions
   ========================================================= */
void xl9535_write_reg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(XL9535_ADDR);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t xl9535_read_reg(uint8_t reg) {
    Wire.beginTransmission(XL9535_ADDR);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom(XL9535_ADDR, 1);
    return Wire.read();
}

void xl9535_pin_mode(uint8_t pin, uint8_t mode) {
    uint8_t reg = (pin < 8) ? XL9535_CONFIG_PORT0 : XL9535_CONFIG_PORT1;
    uint8_t bit = pin % 8;
    uint8_t config = xl9535_read_reg(reg);
    
    if (mode == INPUT || mode == INPUT_PULLUP) {
        config |= (1 << bit);  // Set bit to 1 for input
    } else {
        config &= ~(1 << bit); // Clear bit to 0 for output
    }
    
    xl9535_write_reg(reg, config);
}

void xl9535_digital_write(uint8_t pin, uint8_t value) {
    uint8_t reg = (pin < 8) ? XL9535_OUTPUT_PORT0 : XL9535_OUTPUT_PORT1;
    uint8_t bit = pin % 8;
    uint8_t output = xl9535_read_reg(reg);
    
    if (value) {
        output |= (1 << bit);   // Set bit
    } else {
        output &= ~(1 << bit);  // Clear bit
    }
    
    xl9535_write_reg(reg, output);
}

uint8_t xl9535_digital_read(uint8_t pin) {
    uint8_t reg = (pin < 8) ? XL9535_INPUT_PORT0 : XL9535_INPUT_PORT1;
    uint8_t bit = pin % 8;
    uint8_t input = xl9535_read_reg(reg);
    
    return (input >> bit) & 0x01;
}

/* =========================================================
   Objects
   ========================================================= */
SPIClass spiSD(FSPI);
SPIClass spiDisplay(HSPI);
Adafruit_ILI9341 tft = Adafruit_ILI9341(&spiDisplay, TFT_DC, TFT_CS, -1);  // No hardware RST pin

/* =========================================================
   Labels Configuration
   ========================================================= */
String labels[] = {"BACKGROUND","IN", "CN", "USA", "HK"};
int labelCount = 5;
int currentLabelIndex = 0;

String lastPhotoFile = "None";

/* =========================================================
   Button timing
   ========================================================= */
unsigned long pressStartA = 0;
unsigned long pressStartB = 0;
bool longPressHandledA = false;
bool longPressHandledB = false;

const unsigned long LONG_PRESS_TIME = 700;

/* =========================================================
   Colors (Corrected for RGB565)
   ========================================================= */

// Helper macro to convert 24-bit RGB to 16-bit RGB565
// Usage: RGB565(0xRR, 0xGG, 0xBB)
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// Defined using the macro so you can keep using standard hex values:
#define COLOR_BG        RGB565(0xEC, 0xF0, 0xF1) // Off-White
#define COLOR_HEADER    RGB565(0x34, 0x49, 0x5E) // Dark Blue-Grey
#define COLOR_TEXT      RGB565(0x7F, 0x8C, 0x8D) // Grey
#define COLOR_LABEL     RGB565(0x27, 0xAE, 0x60) // Green
#define COLOR_FILENAME  RGB565(0xE5, 0x9F, 0x8F) // Peach/Salmon
#define COLOR_LINE1     RGB565(0xBE, 0xD7, 0xD9) // Light Blue-Grey
#define COLOR_LINE2     RGB565(0xCE, 0xEC, 0xEE) // Very Light Blue
#define COLOR_SUCCESS   RGB565(0x00, 0xFF, 0x00) // Bright Green
#define COLOR_ERROR     RGB565(0xFF, 0x00, 0x00) // Red
#define COLOR_CAPTURE   RGB565(0x00, 0x00, 0xFF) // Blue

/* =========================================================
   Random ID Generator
   ========================================================= */
String randomID() {
    return String(random(10000, 99999));
}

/* =========================================================
   Update Screen Display
   ========================================================= */
void updateScreen() {
    tft.fillScreen(COLOR_BG);

    // Header bar with gradient effect
    tft.setTextSize(2);
    tft.setTextColor(COLOR_HEADER);
    tft.setCursor(25, 15);
    tft.println("Photo Recorder");

    // Main content area with card-like design
    // Label card
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(25, 72);
    tft.println("Current Label");
    
    tft.setTextSize(3);
    tft.setTextColor(COLOR_LABEL);
    tft.setCursor(25, 100);
    tft.println(labels[currentLabelIndex]);

    // Last saved file section
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor(25, 235);
    tft.println("Last Photo");
    
    tft.setTextSize(2);
    tft.setTextColor(COLOR_FILENAME);
    tft.setCursor(20, 270);
    if (lastPhotoFile.length() > 20) {
        tft.println(lastPhotoFile.substring(0, 17) + "...");
    } else {
        tft.println(lastPhotoFile);
    }

    // Lines
    tft.drawFastHLine(10, 50, 220, COLOR_LINE1);
    tft.drawFastHLine(10, 140, 220, COLOR_LINE2);
    tft.drawFastHLine(10, 225, 220, COLOR_LINE2);
}

/* =========================================================
   Initialize Display
   ========================================================= */
bool initDisplay() {
    Serial.println("Initializing display...");
    
    // Initialize display SPI
    spiDisplay.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    
    // Reset display via XL9535 (Camera_RST line)
    xl9535_pin_mode(XL9535_CAMERA_RST, OUTPUT);
    xl9535_digital_write(XL9535_CAMERA_RST, LOW);
    delay(10);
    xl9535_digital_write(XL9535_CAMERA_RST, HIGH);
    delay(10);
    
    // Initialize display
    tft.begin();
    tft.setRotation(2);  // Adjust rotation as needed
    
    // Turn on backlight via XL9535
    xl9535_pin_mode(XL9535_LCD_BLK, OUTPUT);
    xl9535_digital_write(XL9535_LCD_BLK, HIGH);
    
    tft.fillScreen(COLOR_BG);
    Serial.println("Display initialized!");
    return true;
}

/* =========================================================
   Initialize XL9535 GPIO Expander
   ========================================================= */
bool initXL9535() {
    Serial.println("Initializing XL9535...");
    
    Wire.begin(I2C_SDA, I2C_SCL);
    
    // Test I2C connection
    Wire.beginTransmission(XL9535_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("XL9535 not found!");
        return false;
    }
    
    // Configure button pins as inputs (set config bits to 1)
    xl9535_pin_mode(XL9535_BTN_A, INPUT_PULLUP);  // P14
    xl9535_pin_mode(XL9535_BTN_B, INPUT_PULLUP);  // P02
    
    // Configure LCD backlight as output
    xl9535_pin_mode(XL9535_LCD_BLK, OUTPUT);      // P00
    
    // Configure camera reset as output
    xl9535_pin_mode(XL9535_CAMERA_RST, OUTPUT);   // P01
    
    Serial.println("XL9535 initialized!");
    return true;
}

/* =========================================================
   Initialize SD Card
   ========================================================= */
bool initSDCard() {
    Serial.println("Initializing SD card...");
    
    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    
    if (!SD.begin(SD_CS, spiSD)) {
        Serial.println("SD Card Mount Failed");
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return false;
    }

    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) Serial.println("MMC");
    else if (cardType == CARD_SD) Serial.println("SDSC");
    else if (cardType == CARD_SDHC) Serial.println("SDHC");
    else Serial.println("UNKNOWN");

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);
    Serial.println("SD Card initialized successfully!");
    
    return true;
}

/* =========================================================
   Initialize Camera
   ========================================================= */
bool initCamera() {
    Serial.println("Initializing camera...");
    
    if (!psramFound()) {
        Serial.println("ERROR: PSRAM not found!");
        Serial.println("Please enable PSRAM in Tools > PSRAM menu");
        return false;
    }
    Serial.printf("PSRAM found: %d bytes free\n", ESP.getFreePsram());
    
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }

    sensor_t * s = esp_camera_sensor_get();
    if (s == NULL) {
        Serial.println("Failed to get camera sensor");
        return false;
    }

    // Adjust sensor settings
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 0);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 0);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);

    Serial.println("Camera initialized successfully!");
    return true;
}

/* =========================================================
   Capture and Save Image
   ========================================================= */
void captureAndSaveImage() {
    Serial.println("\n--- Capturing Image ---");
    
    // Show capturing message
    tft.fillScreen(COLOR_BG);
    tft.setTextSize(3);
    tft.setTextColor(COLOR_CAPTURE);
    tft.setCursor(20, 140);
    tft.println("Capturing...");
    
    // Capture RGB565 frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        tft.fillScreen(COLOR_BG);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_ERROR);
        tft.setCursor(10, 140);
        tft.println("Capture Failed!");
        delay(1500);
        updateScreen();
        return;
    }
    
    Serial.printf("Image captured: %dx%d, size: %d bytes\n", 
                  fb->width, fb->height, fb->len);

    // Convert RGB565 to JPEG
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;
    
    Serial.println("Converting RGB565 to JPEG...");
    bool converted = frame2jpg(fb, 85, &jpg_buf, &jpg_len);
    
    if (!converted) {
        Serial.println("JPEG conversion failed");
        esp_camera_fb_return(fb);
        tft.fillScreen(COLOR_BG);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_ERROR);
        tft.setCursor(10, 140);
        tft.println("Convert Failed!");
        delay(1500);
        updateScreen();
        return;
    }
    
    Serial.printf("Conversion successful! JPEG size: %d bytes\n", jpg_len);

    // Generate filename: Label + Random ID
    String fileName = labels[currentLabelIndex] + randomID() + ".jpg";
    String fullPath = "/" + fileName;

    // Save to SD card
    File file = SD.open(fullPath.c_str(), FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        free(jpg_buf);
        esp_camera_fb_return(fb);
        tft.fillScreen(COLOR_BG);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_ERROR);
        tft.setCursor(20, 140);
        tft.println("Save Failed!");
        delay(1500);
        updateScreen();
        return;
    }

    // Write JPEG data
    size_t bytesWritten = file.write(jpg_buf, jpg_len);
    file.close();
    
    // Free memory
    free(jpg_buf);
    esp_camera_fb_return(fb);

    if (bytesWritten == jpg_len) {
        Serial.printf("Image saved successfully: %s\n", fullPath.c_str());
        Serial.printf("File size: %d bytes\n", bytesWritten);
        lastPhotoFile = fileName;
        
        // Show success message
        tft.fillScreen(COLOR_BG);
        tft.setTextSize(3);
        tft.setTextColor(COLOR_SUCCESS);
        tft.setCursor(60, 140);
        tft.println("Saved!");
        delay(1000);
    } else {
        Serial.println("Error: Incomplete write to SD card");
        tft.fillScreen(COLOR_BG);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_ERROR);
        tft.setCursor(20, 140);
        tft.println("Write Error!");
        delay(1500);
    }
    
    updateScreen();
    Serial.println("--- Capture Complete ---\n");
}

/* =========================================================
   Setup
   ========================================================= */
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=================================");
    Serial.println("ESP32-S3 Camera + ILI9341 Display");
    Serial.println("=================================\n");

    randomSeed(analogRead(0));

    // Initialize XL9535 FIRST (needed for display control)
    if (!initXL9535()) {
        Serial.println("XL9535 init failed!");
        while(1) delay(1000);
    }

    // Initialize Display (uses XL9535 for backlight and reset)
    if (!initDisplay()) {
        Serial.println("Display init failed!");
        while(1) delay(1000);
    }

    // Show initializing message
    tft.setTextSize(2);
    tft.setTextColor(COLOR_HEADER);
    tft.setCursor(20, 140);
    tft.println("Initializing...");

    // Initialize SD Card (retry every 4 seconds if failed)
    while (!initSDCard()) {
        tft.fillScreen(COLOR_BG);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_ERROR);
        tft.setCursor(10, 140);
        tft.println("SD Card Failed!");
        tft.setCursor(10, 170);
        tft.println("Retrying in 4s...");
        delay(4000);
    }


    // Initialize Camera
    if (!initCamera()) {
        tft.fillScreen(COLOR_BG);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_ERROR);
        tft.setCursor(10, 140);
        tft.println("Camera Failed!");
        while(1) delay(1000);
    }

    // Show ready screen
    tft.fillScreen(COLOR_BG);
    tft.setTextSize(3);
    tft.setTextColor(COLOR_SUCCESS);
    tft.setCursor(60, 140);
    tft.println("Ready!");
    delay(1000);

    updateScreen();
}

/* =========================================================
   Main Loop
   ========================================================= */
void loop() {
    // Read button states from XL9535 (buttons are active LOW with pullup)
    bool btnA = !xl9535_digital_read(XL9535_BTN_A);  // P14
    bool btnB = !xl9535_digital_read(XL9535_BTN_B);  // P02

    // =========================================================
    //                  BUTTON A  (Capture / Prev Label)
    // =========================================================
    if (btnA && pressStartA == 0) {
        pressStartA = millis();
        longPressHandledA = false;
    }

    // Release → short press or long press
    if (!btnA && pressStartA > 0) {
        unsigned long pressTime = millis() - pressStartA;

        if (pressTime < LONG_PRESS_TIME) {
            // -------- SHORT PRESS A → Capture photo --------
            captureAndSaveImage();
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


    // =========================================================
    //                  BUTTON B (Next Label)
    // =========================================================
    if (btnB && pressStartB == 0) {
        pressStartB = millis();
        longPressHandledB = false;
    }

    if (!btnB && pressStartB > 0) {
        unsigned long pressTime = millis() - pressStartB;

        if (pressTime < LONG_PRESS_TIME) {
            // -------- SHORT PRESS B → Next label --------
            currentLabelIndex++;
            if (currentLabelIndex >= labelCount) currentLabelIndex = 0;
            updateScreen();
        }

        pressStartB = 0;
    }

    // Long press B → Next label
    if (btnB && !longPressHandledB &&
        millis() - pressStartB >= LONG_PRESS_TIME) {

        currentLabelIndex++;
        if (currentLabelIndex >= labelCount) currentLabelIndex = 0;

        longPressHandledB = true;
        updateScreen();
    }
    
    delay(50);  // Debounce delay
}
