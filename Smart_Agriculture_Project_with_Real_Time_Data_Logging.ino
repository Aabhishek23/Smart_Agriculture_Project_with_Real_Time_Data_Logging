#include <LiquidCrystal_I2C.h>
#include "ThingSpeak.h"
#include <WiFi.h>

#define LED_BUILTIN 2

// Wi-Fi details
char ssid[] = "5NET";
char pass[] = "Ttaniya@23";

// ThingSpeak details
unsigned long Channel_ID = 2392583;
const char* myWriteAPIKey = "I2MY5VSRJZKQ6O19";
const char* myReadAPIKey = "JBTTO2XDE2V7ELL1";

// Field numbers
const int Field_Number_1 = 1;
const int Field_Number_2 = 2; // Assuming you're using a second field for rain sensor

WiFiClient client;
const int moistureSensorPin = 36;
const int sensorMinValue = 0;
const int sensorMaxValue = 4095;

const int rainSensorPin = 34; // GPIO34 corresponds to A6 on some ESP32 boards

int lcdColumns = 16;
int lcdRows = 2;

LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);
void displayData(int value, String stringValue, int line = 0);



hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool updateDisplay = false;
volatile int moistureValue = 0;
volatile int rainValue = 0;




void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    moistureValue = moisture_sansore();
    rainValue = rain_sensore();
    updateDisplay = true;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    ThingSpeak.begin(client);
    connectToWiFi();


    pinMode(LED_BUILTIN, OUTPUT);

    lcd.init();
    lcd.backlight();

    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 5000000, true);  // 5 seconds interval
    timerAlarmEnable(timer);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();
    }

    if (updateDisplay) {
        portENTER_CRITICAL(&timerMux);
        int currentMoistureValue = moistureValue;
        int currentRainValue = rainValue;
        updateDisplay = false;
        portEXIT_CRITICAL(&timerMux);

        // Display the new moisture value on the LCD
        displayData(currentMoistureValue, "Soil Moisture:",0);
        delay(3000);
        displayData(currentRainValue, "Rain Level:", 0);
        delay(3000);
        // Update ThingSpeak with the new sensor values if connected to WiFi
        if (WiFi.status() == WL_CONNECTED) {
            upload(currentMoistureValue, currentRainValue);
        }
    }
}

void connectToWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected.");
}

int moisture_sansore() {
    int sensorValue = analogRead(moistureSensorPin);
    int moisturePercentage = map(sensorValue, sensorMinValue, sensorMaxValue, 100, 0);
    return moisturePercentage;
}

int rain_sensore() {
    // Read the analog value from rain sensor connected to GPIO34 (A6)
    int sensorValue = analogRead(rainSensorPin);
    // Convert the analog value to a percentage (0-100%)
    int rainPercentage = map(sensorValue, 0, 4095, 100, 0);
    return rainPercentage;
}

void upload(int moistureValue, int rainValue) {
    ThingSpeak.setField(Field_Number_1, moistureValue);
    ThingSpeak.setField(Field_Number_2, rainValue);
    ThingSpeak.writeFields(Channel_ID, myWriteAPIKey);

    Serial.print("Soil Moisture (in Percentage) = ");
    Serial.print(moistureValue);
    Serial.println("%");

    Serial.print("Rain Level (in Percentage) = ");
    Serial.print(rainValue);
    Serial.println("%");

    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
}

void displayData(int value, String stringValue, int line) {
    lcd.clear();
    lcd.setCursor(0, line);
    lcd.print(stringValue);
    lcd.setCursor(0, line + 1);
    lcd.print(value);
    lcd.print("%");
}
