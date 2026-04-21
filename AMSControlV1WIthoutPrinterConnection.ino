#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <Keypad.h>


#define SS_PIN 53
#define RST_PIN 5
MFRC522 rfid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key mifaKey; 
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {22, 23, 24, 25}; 
byte colPins[COLS] = {26, 27, 28, 29}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


int step = 0; 
String inputBuffer = "";
byte selectedMaterial = 0;
byte selectedColor = 0;
bool slotWaiting = false;
String lastScannedInfo = "";
String lastScannedUID = ""; 
unsigned long displayTimer = 0;
bool showingData = false;

String amsSlots[5] = {"Leer", "Leer", "Leer", "Leer", "Leer"}; 
String amsUIDs[5] = {"", "", "", "", ""};


void showIdle();
void updateDisplay();
void writeToCard();
void readCardData();
void showAllSlots();

String getMaterial(int m) {
  switch(m) {
    case 1: return "PLA"; case 2: return "PETG"; case 3: return "TPU";
    case 4: return "ABS"; case 5: return "ASA"; case 6: return "PET";
    default: return "??";
  }
}

String getColor(int c) {
  switch(c) {
    case 1: return "Weiss"; case 2: return "Schwarz"; case 3: return "Blau"; 
    case 4: return "Gruen"; case 5: return "Gelb"; case 6: return "Hellgrau";
    case 7: return "Dunkelgrau"; case 11: return "Orange"; case 12: return "Pink";
    case 13: return "Rot"; case 14: return "Lila"; case 20: return "Gold"; 
    case 30: return "Transp.";
    default: return String(c);
  }
}

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  for (byte i = 0; i < 6; i++) mifaKey.keyByte[i] = 0xFF; // Standard-Passwort
  lcd.begin(16, 2);
  showIdle();
}

void loop() {
  char k = keypad.getKey();


  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (step == 3) {
      writeToCard(); // Wenn wir im Schreibmodus sind: SCHREIBEN
    } else if (step == 0 && !slotWaiting && !showingData) {
      readCardData(); // Sonst: LESEN
    }
  }


  if (showingData && !slotWaiting && (millis() - displayTimer > 5000)) {
    showingData = false;
    showIdle();
  }


  if (k) {

    if (showingData && k == '#' && !slotWaiting) {
      slotWaiting = true;
      lcd.setCursor(0, 1);
      lcd.print("Slot waehlen... ");
    }

    else if (step == 0 && !slotWaiting && !showingData && k == 'D') {
      showAllSlots();
    }

    else if (slotWaiting && ((k >= '1' && k <= '4') || k == 'A')) {
      int slotIdx = (k == 'A') ? 4 : (k - '1');
      for(int i=0; i<5; i++) {
        if(amsUIDs[i] == lastScannedUID) { amsUIDs[i] = ""; amsSlots[i] = "Leer"; }
      }
      amsSlots[slotIdx] = lastScannedInfo;
      amsUIDs[slotIdx] = lastScannedUID;
      lcd.clear();
      lcd.print("Slot " + String(k) + " aktiv!");
      lcd.setCursor(0, 1);
      lcd.print(lastScannedInfo);
      delay(2000);
      slotWaiting = false;
      showingData = false;
      showIdle();
    }
  
    else if (k == '*') {
      if (step == 0) {
        step = 1; slotWaiting = false; showingData = false; inputBuffer = "";
        lcd.clear(); lcd.print("1:PLA 2:PETG 3:TPU"); lcd.setCursor(0, 1); lcd.print("Mat: ");
      } else if (step == 1) {
        selectedMaterial = inputBuffer.toInt(); step = 2; inputBuffer = "";
        lcd.clear(); lcd.print("Farbcode:"); lcd.setCursor(0, 1); lcd.print("Code: ");
      } else if (step == 2) {
        selectedColor = inputBuffer.toInt(); step = 3;
        lcd.clear(); lcd.print("SCAN ZUM"); lcd.setCursor(0, 1); lcd.print("SPEICHERN...");
      }
    }

    else if (k == '#') {
      if (step > 0 || slotWaiting) showIdle();
      else if (inputBuffer.length() > 0) { inputBuffer.remove(inputBuffer.length() - 1); updateDisplay(); }
    }

    else if (k >= '0' && k <= '9' && step > 0) {
      inputBuffer += k;
      updateDisplay();
    }
  }
}

void showAllSlots() {
  for (int i = 0; i < 5; i++) {
    String name = (i == 4) ? "Ext. A" : "Slot " + String(i + 1);
    lcd.clear();
    lcd.print(name + ":");
    lcd.setCursor(0, 1);
    lcd.print(amsSlots[i]);
    delay(1500); 
  }
  showIdle();
}

void updateDisplay() {
  lcd.setCursor(6, 1); lcd.print("    "); 
  lcd.setCursor(6, 1); lcd.print(inputBuffer);
}

void showIdle() {
  step = 0; slotWaiting = false; showingData = false; inputBuffer = "";
  lcd.clear();
  lcd.print("AMS Control");
  lcd.setCursor(0, 1);
  lcd.print("* Prog   D Liste");
}

void readCardData() {
  lastScannedUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    lastScannedUID += String(rfid.uid.uidByte[i], HEX);
  }
  byte block = 4; byte buffer[18]; byte size = sizeof(buffer);
  MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &mifaKey, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) { rfid.PICC_HaltA(); rfid.PCD_StopCrypto1(); return; }
  
  status = rfid.MIFARE_Read(block, buffer, &size);
  if (status == MFRC522::STATUS_OK) {
    lastScannedInfo = getMaterial(buffer[0]) + " " + getColor(buffer[1]);
    int foundSlot = -1;
    for(int i=0; i<5; i++) { if(amsUIDs[i] == lastScannedUID) { foundSlot = i; break; } }
    lcd.clear();
    if (foundSlot != -1) {
      String slotName = (foundSlot == 4) ? "A" : String(foundSlot + 1);
      lcd.print("S" + slotName + ": " + lastScannedInfo);
    } else { lcd.print(lastScannedInfo); }
    lcd.setCursor(0, 1); lcd.print("# -> Zuweisen");
    showingData = true; displayTimer = millis();
  }
  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
}

void writeToCard() {
  byte block = 4;
  byte data[] = {selectedMaterial, selectedColor, 0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  MFRC522::StatusCode status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &mifaKey, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) { 
    lcd.clear(); lcd.print("Fehler Auth!"); delay(1000); showIdle(); 
    rfid.PICC_HaltA(); rfid.PCD_StopCrypto1(); return; 
  }
  status = rfid.MIFARE_Write(block, data, 16);
  if (status == MFRC522::STATUS_OK) {
    lcd.clear(); lcd.print("Gespeichert!"); delay(2000); 
  } else {
    lcd.clear(); lcd.print("Schreibfehler!"); delay(2000);
  }
  showIdle();
  rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
}