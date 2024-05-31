#include <LiquidCrystal_I2C.h>
#include "ThingSpeak.h"
#include <WiFi.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include "Melody.h"

// Pin definitions
#define LED_BUILTIN 2
#define moistureSensorPin 36
#define rainSensorPin 34

// Define the pin where the DHT11 data pin is connected
#define DHTPIN 4  // GPIO4 (D4)

// Define the type of DHT sensor
#define DHTTYPE DHT11

// Relay pins for motor control
const int relayPin1 = 32; // GPIO32
const int relayPin2 = 25; // GPIO25

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// WiFi and ThingSpeak credentials
const char ssid[] = "5NET";
const char pass[] = "Ttaniya@23";
const unsigned long Channel_ID = 2392583;
const char* myWriteAPIKey = "FOCKHMNT3EXY1QUA";

const int Field_Number_1 = 1;
const int Field_Number_2 = 2;
const int Field_Number_3 = 3;  // Humidity field
const int Field_Number_4 = 4;  // Temperature field

float humidity = 0;
// Read temperature as Celsius (the default)
float temperature = 0;

WiFiClient client;
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int sensorMinValue = 0;
const int sensorMaxValue = 4095;
volatile bool updateDisplay = false;
volatile int moistureValue = 0;
volatile int rainValue = 0;

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Custom character for degree symbol
byte degreeSymbol[8] = {
    0b00111,
    0b00101,
    0b00111,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

// Function declaration
void IRAM_ATTR onTimer();

void setup() {
    Serial.begin(115200);
    // Initialize the DHT sensor
    dht.begin();
    WiFi.mode(WIFI_STA);
    ThingSpeak.begin(client);
    connectToWiFi();

    pinMode(LED_BUILTIN, OUTPUT);

    // Initialize relay pins
    pinMode(relayPin1, OUTPUT);
    pinMode(relayPin2, OUTPUT);
    digitalWrite(relayPin1, LOW);
    digitalWrite(relayPin2, LOW);

    lcd.init();
    lcd.backlight();
    
    // Create custom degree symbol
    lcd.createChar(0, degreeSymbol);

    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 5000000, true);  // 5 seconds interval
    timerAlarmEnable(timer);
}

// Timer interrupt service routine
void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    moistureValue = moistureSensor();
    rainValue = rainSensor();
    updateDisplay = true;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();
    }

    // Now read the DHT11 sensor and display its values
    dht_11();

    if (updateDisplay) {
        portENTER_CRITICAL(&timerMux);
        int currentMoistureValue = moistureValue;
        int currentRainValue = rainValue;
        updateDisplay = false;
        portEXIT_CRITICAL(&timerMux);

        // Control the motor based on soil moisture
        controlMotor(currentMoistureValue);

        // Display the new sensor values on the LCD
        displayData(currentMoistureValue, "Soil Moisture:", "%");
        delay(3000);
        displayData(currentRainValue, "Rain Level:", "%");
        delay(3000);

        displayData(humidity, "Humidity:", "%");
        delay(3000);
        displayData(temperature, "Temperature:", (char)0);  // Use custom degree symbol
        delay(3000);

        lcd.clear();

        // Update ThingSpeak with the new sensor values if connected to WiFi
        if (WiFi.status() == WL_CONNECTED) {
            upload(currentMoistureValue, currentRainValue, humidity, temperature);
        }
    }
}

void connectToWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, pass);
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
        delay(500);
        Serial.print(".");
        retryCount++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected.");
        playInternetConnectedMelody();
    } else {
        Serial.println("Failed to connect to WiFi.");
    }
}

int moistureSensor() {
    int sensorValue = analogRead(moistureSensorPin);
    return map(sensorValue, sensorMinValue, sensorMaxValue, 100, 0);
}

int rainSensor() {
    int sensorValue = analogRead(rainSensorPin);
    return map(sensorValue, 0, 4095, 100, 0);
}

void upload(int moistureValue, int rainValue, float humidity, float temperature) {
    ThingSpeak.setField(Field_Number_1, moistureValue);
    ThingSpeak.setField(Field_Number_2, rainValue);
    ThingSpeak.setField(Field_Number_3, humidity);
    ThingSpeak.setField(Field_Number_4, temperature);
    ThingSpeak.writeFields(Channel_ID, myWriteAPIKey);

    Serial.print("Soil Moisture (in Percentage) = ");
    Serial.print(moistureValue);
    Serial.println("%");

    Serial.print("Rain Level (in Percentage) = ");
    Serial.print(rainValue);
    Serial.println("%");

    Serial.print("Humidity = ");
    Serial.print(humidity);
    Serial.println("%");

    Serial.print("Temperature = ");
    Serial.print(temperature);
    Serial.println("°C");

    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);

    beep();
}

void displayData(float value, String label, char unit) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(label);
    lcd.setCursor(0, 1);
    lcd.print(value);
    lcd.write(unit);
}

void displayData(float value, String label, String unit) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(label);
    lcd.setCursor(0, 1);
    lcd.print(value);
    lcd.print(unit);
}

void dht_11() {
    // Read humidity
    humidity = dht.readHumidity();
    // Read temperature as Celsius (the default)
    temperature = dht.readTemperature();
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }
    // Print the results to the Serial Monitor
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" °C");
}

void controlMotor(int moistureValue) {
    if (moistureValue < 25) {
        // Turn motor on
        digitalWrite(relayPin1, HIGH);
        digitalWrite(relayPin2, LOW);
        Serial.println("Motor ON");
        displayData(1, "Motor ON:", ".");
        delay(3000);

    } else if (moistureValue > 60) {
        // Turn motor off
        digitalWrite(relayPin1, LOW);
        digitalWrite(relayPin2, LOW);
        Serial.println("Motor OFF");
        displayData(0, "Motor OFF:", ".");
        delay(3000);
    }
}
