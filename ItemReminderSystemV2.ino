#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_PN532.h>

// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// PN532 NFC Module settings
#define PN532_IRQ   2
#define PN532_RESET 3
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

#define RESET_BUTTON 6
#define LED 4
#define BUZZER 5

// --- Known tags ---
// Store store items in storage instead of RAM
const char name0[] PROGMEM = "Test Card";
const char name1[] PROGMEM = "Keys";
const char name2[] PROGMEM = "Presto Card";

struct NFCTag {
  uint8_t uid[7];
  uint8_t uidLength;
  const char* name;
  bool scanned;
};

NFCTag knownTags[] = {
  {{ 0x9C, 0xAC, 0xC9, 0x6 }, 4, name0, false },
  {{ 0x1A, 0x7B, 0x4D, 0x6 }, 4, name1, false },
  {{ 0x4, 0x25, 0x44, 0x32, 0x2C, 0x75, 0x80 }, 7, name2, false },

};

const int NUM_TAGS = sizeof(knownTags) / sizeof(knownTags[0]);
// ------------------

bool lastButtonState = LOW;

// Helper to print a PROGMEM string to the display
void displayPrint_P(const char* str) {
  char buf[32];
  strncpy_P(buf, str, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  display.print(buf);
}

void displayPrintln_P(const char* str) {
  char buf[32];
  strncpy_P(buf, str, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  display.println(buf);
}

void showTagList() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);

  int remaining = 0;
  for (int i = 0; i < NUM_TAGS; i++) {
    if (!knownTags[i].scanned) remaining++;
  }

  if (remaining == 0) {
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println(F("All items"));
    display.println(F("scanned!"));
    display.setTextSize(1);
    display.display();
    digitalWrite(LED, LOW);
    tone(BUZZER, 440, 1000);
    return;
  }

  display.print(F("Remaining ("));
  display.print(remaining);
  display.print(F("/"));
  display.print(NUM_TAGS);
  display.println(F("):"));
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);
  display.setCursor(0, 12);

  for (int i = 0; i < NUM_TAGS; i++) {
    if (!knownTags[i].scanned) {
      display.print(F("- "));
      displayPrintln_P(knownTags[i].name);
    }
  }

  digitalWrite(LED, HIGH);
  display.display();
}

void resetTags() {
  for (int i = 0; i < NUM_TAGS; i++) {
    knownTags[i].scanned = false;
  }
  Serial.println(F("Reset!"));

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(F("Reset!"));
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
  pinMode(BUZZER, OUTPUT);

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
  display.println(F("Initializing NFC..."));
  display.display();

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Didn't find PN53x"));
    display.display();
    while (1);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Found PN532"));
  display.print(F("Firmware: "));
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
  bool currentButtonState = digitalRead(RESET_BUTTON);
    if (digitalRead(RESET_BUTTON) == HIGH) {
      resetTags();
    }

  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;

  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100)) {

    Serial.print(F("Scanned UID:"));
    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print(F(" 0x")); Serial.print(uid[i], HEX);
    }
    Serial.println();

    display.clearDisplay();
    display.setCursor(0, 0);

    int matchIndex = findTag(uid, uidLength);

    if (matchIndex >= 0) {
      knownTags[matchIndex].scanned = true;
      display.setTextSize(1);
      display.println(F("Checked off:"));
      display.println();
      display.setTextSize(2);
      displayPrintln_P(knownTags[matchIndex].name);
      display.setTextSize(1);
      Serial.print(F("Checked off: "));
      Serial.println((__FlashStringHelper*)knownTags[matchIndex].name);
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
        display.println(F("Already"));
        display.println(F("scanned!"));
        display.setTextSize(1);
        Serial.println(F("Already scanned."));
      } else {
        display.setTextSize(2);
        display.println(F("UNKNOWN"));
        display.setTextSize(1);
        display.println();
        display.print(F("UID:"));
        for (uint8_t i = 0; i < uidLength; i++) {
          display.print("0x");
          display.print(uid[i], HEX);
          display.print(" ");
        }
        Serial.println(F("Unknown tag."));
      }
    }

    display.display();
    delay(2000);
    showTagList();
  }
}
