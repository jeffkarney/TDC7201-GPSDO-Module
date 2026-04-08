#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>

#define TDC7201_ADDRESS 0x68

// I2C setup
void I2C_init() {
    Wire.begin();
}

// TDC7201 measurement reading function
uint32_t readTDC7201() {
    Wire.beginTransmission(TDC7201_ADDRESS);
    Wire.write(0x00); // Register address for measurement
    Wire.endTransmission();
    Wire.requestFrom(TDC7201_ADDRESS, 4);
    uint32_t count = 0;
    for (int i = 0; i < 4; i++) {
        count |= Wire.read() << (i * 8);
    }
    return count;
}

// Phase difference calculation
float calculatePhaseDifference(uint32_t count) {
    return count * 55.0; // Convert count to ps
}

// Measurement task every 100ms
void measurementTask() {
    static unsigned long lastMeasurement = 0;
    if (millis() - lastMeasurement > 100) {
        lastMeasurement = millis();
        uint32_t measurement_count = readTDC7201();
        float phase_difference_ps = calculatePhaseDifference(measurement_count);
        // Store or send the measurements as needed
    }
}

// REST API structure
WebServer server(80);

void handleMeasurements() {
    uint32_t measurement_count = readTDC7201();
    float phase_difference_ps = calculatePhaseDifference(measurement_count);
    String json = "{\"phase_difference_ps\": " + String(phase_difference_ps) + ", \"measurement_count\": " + String(measurement_count) + ", \"uncertainty_ps\": 0.0, \"uncertainty_breakdown\": {\"comparator_offset_ps\": 0.5, \"comparator_jitter_ps\": 0.05, \"tdc_resolution_ps\": 0.055, \"delay_line_error_ps\": 0.5, \"temperature_drift_ps\": 0.1}}";
    server.send(200, "application/json", json);
}

void setup() {
    // Initialize Serial, I2C, and WiFi
    Serial.begin(115200);
    I2C_init();
    WiFi.begin("yourSSID", "yourPASSWORD");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    server.on("/api/measurements", handleMeasurements);
    server.begin();
}

void loop() {
    measurementTask();
    server.handleClient();
}
