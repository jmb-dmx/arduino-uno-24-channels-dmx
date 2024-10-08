// Arduino Uno Console 24 canaux dmx 

#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <Conceptinetics.h>

int dimmer_val[24];
int channel_num = 1;
unsigned long buttonPressTime = 0;
bool buttonHeld = false;
bool eraseMemory = false;
unsigned long confirmationStartTime = 0;
const unsigned long confirmationTimeout = 5000;
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

byte right[8] = {
  B00000,
  B00100,
  B00010,
  B11111,
  B00010,
  B00100,
  B00000,
  B00000
};

byte block[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

int lcd_key = 0;
int adc_key_in = 0;
#define btnRIGHT 0
#define btnUP 1
#define btnDOWN 2
#define btnLEFT 3
#define btnSELECT 4
#define btnNONE 5

int read_LCD_buttons() {
  adc_key_in = analogRead(0);

  if (adc_key_in < 50) return btnRIGHT;
  if (adc_key_in < 250) return btnUP;
  if (adc_key_in < 450) return btnDOWN;
  if (adc_key_in < 650) return btnLEFT;
  if (adc_key_in < 850) return btnSELECT;
  return btnNONE;
}

#define DMX_MASTER_CHANNELS 24
#define RXEN_PIN 2

DMX_Master dmx_master(DMX_MASTER_CHANNELS, RXEN_PIN);

void setup() {
  dmx_master.setChannelRange(1, 24, 0);

  for (int i = 0; i < 24; i++) {
    dimmer_val[i] = EEPROM.read(i);
    if (dimmer_val[i] < 0 || dimmer_val[i] > 255) {
      dimmer_val[i] = 0;
    }
    dmx_master.setChannelValue(i + 1, dimmer_val[i]);
  }
  lcd.createChar(2, right);
  lcd.createChar(3, block);
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Console DMX 24CH");
  lcd.setCursor(0, 1);
  lcd.print("   J-Michel B");
  delay(1000);

  for (int positionCounter = 0; positionCounter < 16; positionCounter++) {
    lcd.scrollDisplayRight();
    delay(30);
  }

  lcd.begin(16, 2);
  dmx_send();
}

void loop() {
  lcd_key = read_LCD_buttons();

  if (lcd_key == btnSELECT) {
    if (!buttonHeld) {
      buttonPressTime = millis();
      buttonHeld = true;
      eraseMemory = false;
      confirmationStartTime = millis();
    } else if (millis() - buttonPressTime > 3000 && !eraseMemory) {
      eraseMemory = true;
      lcd.clear();

      // Animation de remplissage de l'écran
      for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 16; j++) {
          lcd.setCursor(j, i);
          lcd.write(byte(3));
          delay(50);
        }
      }

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("EFFACER MEMOIRE?");
      lcd.setCursor(0, 1);
      lcd.print(" *Appuyer sur ");
      lcd.write(byte(2));
      confirmationStartTime = millis();

      while (millis() - confirmationStartTime < confirmationTimeout) {
        int confirm_key = read_LCD_buttons();
        if (confirm_key == btnRIGHT) {
          for (int i = 0; i < 24; i++) {
            EEPROM.write(i, 0);
            dimmer_val[i] = 0;
            dmx_master.setChannelValue(i + 1, 0);
          }
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("MEMOIRE EFFACEE");
          delay(1000);
          buttonHeld = false;
          break;
        }
      }
      
      if (millis() - confirmationStartTime >= confirmationTimeout) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ANNULATION");
        lcd.setCursor(0, 1);
        lcd.print("EFFACER MEMOIRE");
        delay(1000);
        lcd.clear();
        buttonHeld = false;
      }
    }
  } else if (buttonHeld) {
    buttonHeld = false;
    if (!eraseMemory) {
      EEPROM.write(channel_num - 1, dimmer_val[channel_num - 1]);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("   SAUVEGARDE  ");
      lcd.setCursor(0, 1);
      lcd.print("CH:");
      lcd.setCursor(4, 1);
      lcd.print(channel_num);
      lcd.setCursor(8, 1);
      lcd.print("LVL:");
      lcd.setCursor(13, 1);
      lcd.print(dimmer_val[channel_num - 1]);
      delay(500);
    }
  }

  if (lcd_key == btnRIGHT || lcd_key == btnLEFT) {
    unsigned long holdTime = millis() - buttonPressTime;
    int increment = holdTime > 1000 ? 5 : 1;

    if (lcd_key == btnRIGHT) {
      dimmer_val[channel_num - 1] = constrain(dimmer_val[channel_num - 1] + increment, 0, 255);
    } else if (lcd_key == btnLEFT) {
      dimmer_val[channel_num - 1] = constrain(dimmer_val[channel_num - 1] - increment, 0, 255);
    }
    delay(200);
  }

  switch (lcd_key) {
    case btnUP:
      if (channel_num < 24) {
        channel_num++;
      } else {
        channel_num = 1;
      }
      lcd.setCursor(4, 1);
      lcd.print("   ");
      delay(200);
      break;

    case btnDOWN:
      if (channel_num > 1) {
        channel_num--;
      } else {
        channel_num = 24;
      }
      lcd.setCursor(4, 1);
      lcd.print("   ");
      delay(200);
      break;

    case btnNONE:
      lcd.print("       ");
      break;
  }
  dmx_send();
}

void dmx_send() {
  dmx_master.enable();
  lcd.setCursor(0, 0);
  lcd.print("Console DMX 24CH");
  lcd.setCursor(0, 1);
  lcd.print("CH: ");
  lcd.setCursor(8, 1);
  lcd.print("LVL:");
  lcd.setCursor(4, 1);
  lcd.print(channel_num);

  char buffer[4];
  sprintf(buffer, "%03d", dimmer_val[channel_num - 1]);

  lcd.setCursor(13, 1);
  lcd.print(buffer);

  dmx_master.setChannelValue(channel_num, dimmer_val[channel_num - 1]);
}
