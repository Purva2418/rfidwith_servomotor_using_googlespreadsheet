
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <Servo.h>

// RFID pin configuration
#define RST_PIN D1  // Reset pin for RFID module
#define SS_PIN D2   // Slave select pin for RFID module

// Servo pin
#define SERVO_PIN D4

// Buzzer pin (changed to D8 to avoid conflict with RFID pins)
#define BUZZER_PIN D8

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;
MFRC522::StatusCode status;
Servo gateServo;

// WiFi credentials
#define WIFI_SSID "Device name"
#define WIFI_PASSWORD "wifi password"

// Google Sheets URL
const String sheet_url = "google_sheet_url";

// Authorized UID (change to your UID)
byte authorizedUID[4] = {0x61, 0xBD, 0xAC, 0x7B}; // Example: 61 BD AC 7B

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\nBooting...");

    gateServo.attach(SERVO_PIN);
    gateServo.write(0);  // Closed

    // Initialize Buzzer pin
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);  // Buzzer is off initially

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long startAttemptTime = millis();
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n WiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n Failed to connect to WiFi.");
    }

    Serial.println("Initializing SPI and RFID...");
    SPI.begin();
    mfrc522.PCD_Init();
    delay(1000);

    byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
    Serial.print("RFID Version: 0x");
    Serial.println(version, HEX);

    if (version == 0x00 || version == 0xFF) {
        Serial.println(" ERROR: RFID module not responding. Check wiring and power.");
    } else {
        Serial.println(" RFID module initialized successfully!");
    }
}

void loop() {
    Serial.println("Waiting for card...");

    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        delay(500);
        return;
    }

    Serial.println("\n Card detected!");

    // Print UID
    Serial.print("UID: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i], HEX);
        if (i < mfrc522.uid.size - 1) Serial.print(" ");
    }
    Serial.println();

    // Check UID
    bool authorized = true;
    for (int i = 0; i < 4; i++) {
        if (mfrc522.uid.uidByte[i] != authorizedUID[i]) {
            authorized = false;
            break;
        }
    }

    if (authorized) {
        Serial.println(" Authorized Card");

        if (WiFi.status() == WL_CONNECTED) {
            WiFiClientSecure client;
            client.setInsecure(); // Accept self-signed certs

            String requestUrl = sheet_url + "Authorized_User";
            requestUrl.trim();

            Serial.println("Sending to Google Sheets: " + requestUrl);

            HTTPClient https;
            if (https.begin(client, requestUrl)) {
                int httpCode = https.GET();
                if (httpCode > 0) {
                    Serial.printf(" Server Response: %d\n", httpCode);
                } else {
                    Serial.printf("HTTP Request Failed: %s\n", https.errorToString(httpCode).c_str());
                }
                https.end();
            } else {
                Serial.println(" Failed to connect to Google Sheets URL.");
            }
        }

        // Open gate
        gateServo.write(90);
        delay(2000);
        gateServo.write(0);
    } else {
        Serial.println(" Unauthorized Card - Access Denied");
        
        // Activate Buzzer for unauthorized access
        digitalWrite(BUZZER_PIN, HIGH);  // Buzzer on
        delay(1000);  // Buzzer sounds for 1 second
        digitalWrite(BUZZER_PIN, LOW);   // Buzzer off
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(3000);
}

