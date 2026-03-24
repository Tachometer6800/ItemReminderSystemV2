#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_PN532.h>

// OLED Display Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// NFC Module Settings
#define PN532_IRQ   2
#define PN532_RESET 3
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

// Pins
#define RESET_BUTTON 6
#define LED 4

// Update to ad
struct NFCTag {
  uint8_t uid[7];
  uint8_t uidLength;
  const char* name;
  bool scanned;
};

NFCTag knownTags[] = {
  {{ 0x9C, 0xAC, 0xC9, 0x6 }, 4, "Test Card", false },
  {{ 0x1A, 0x7B, 0x4D, 0x6 }, 4, "Keys", false },
};

const int NUM_TAGS = sizeof(knownTags) / sizeof(knownTags[0]);
// ------------------

void showTagList() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);

  int remaining = 0;
  for (int i = 0; i < NUM_TAGS; i++) {
    if (!knownTags[i].scanned) remaining++;
  }

  if (remaining == 0) {
    display.setCursor(0,0);
    display.setTextSize(2);
    display.println("All items");
    display.println("scanned!");
    display.setTextSize(1);
    display.display();
    digitalWrite(LED, LOW);
    return;
  }

  display.print("Remaining (");
  display.print(remaining);
  display.print("/");
  display.print(NUM_TAGS);
  display.println("):");
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);
  display.setCursor(0, 12);

  for (int i = 0; i < NUM_TAGS; i++) {
    if (!knownTags[i].scanned) {
      display.print("- ");
      display.println(knownTags[i].name);
    }
  }

  digitalWrite(LED, HIGH);
  display.display();
}

void resetTags() {
  for (int i = 0; i < NUM_TAGS; i++) {
    knownTags[i].scanned = false;
  }
  Serial.println("Reset!");

  // Show a brief reset confirmation
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println("Reset!");
  display.setTextSize(1);
  display.display();
  delay(1000);

  showTagList();
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial);

  pinMode(RESET_BUTTON, INPUT);
  pinMode(LED, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.setRotation(2);
  display.display();
  delay(1000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing NFC...");
  display.display();

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Didn't find PN53x module");
    display.display();
    while (1);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Found PN532");
  display.print("Firmware: ");
  display.print((versiondata >> 16) & 0xFF, HEX);
  display.print('.');
  display.println((versiondata >> 8) & 0xFF, HEX);
  display.display();

  nfc.SAMConfig();
  delay(1000);

  showTagList();
}

int findTag(uint8_t *scanned, uint8_t scannedLen) {
  for (int i = 0; i < NUM_TAGS; i++) {
    if (knownTags[i].scanned) continue;
    if (knownTags[i].uidLength != scannedLen) continue;
    bool match = true;
    for (uint8_t j = 0; j < scannedLen; j++) {
      if (knownTags[i].uid[j] != scanned[j]) { match = false; break; }
    }
    if (match) return i;
  }
  return -1;
}

void loop(void) {

  if (digitalRead(RESET_BUTTON) == HIGH){
    resetTags();
  }

  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;

  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100)) {

    Serial.print("Scanned UID:");
    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print(" 0x"); Serial.print(uid[i], HEX);
    }
    Serial.println();

    display.clearDisplay();
    display.setCursor(0, 0);

    int matchIndex = findTag(uid, uidLength);

    if (matchIndex >= 0) {
      knownTags[matchIndex].scanned = true;
      display.setTextSize(1);
      display.println("Checked off:");
      display.println("");
      display.setTextSize(2);
      display.println(knownTags[matchIndex].name);
      display.setTextSize(1);
      Serial.print("Checked off: ");
      Serial.println(knownTags[matchIndex].name);
    } else {
      bool alreadyDone = false;
      for (int i = 0; i < NUM_TAGS; i++) {
        if (!knownTags[i].scanned) continue;
        if (knownTags[i].uidLength != uidLength) continue;
        bool match = true;
        for (uint8_t j = 0; j < uidLength; j++) {
          if (knownTags[i].uid[j] != uid[j]) { match = false; break; }
        }
        if (match) { alreadyDone = true; break; }
      }

      if (alreadyDone) {
        display.setTextSize(2);
        display.println("Already");
        display.println("scanned!");
        display.setTextSize(1);
        Serial.println("Already scanned.");
      } else {
        display.setTextSize(2);
        display.println("UNKNOWN");
        display.setTextSize(1);
        display.println("");
        display.print("UID:");
        for (uint8_t i = 0; i < uidLength; i++) {
          display.print(" "); display.print(uid[i], HEX);
        }
        Serial.println("Unknown tag.");
      }
    }

    display.display();
    delay(2000);
    showTagList();
  }
  
}