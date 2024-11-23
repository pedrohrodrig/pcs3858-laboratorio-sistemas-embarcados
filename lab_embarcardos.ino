#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <ESP_Mail_Client.h>

#define EEPROM_SIZE 5
LiquidCrystal_I2C lcd(0x27, 16, 2);

//Setup keyboard
const byte ROWS = 4;
const byte COLUMNS = 4;

char keys[ROWS][COLUMNS] = {
  {'1', '2', '3', 'A'}, 
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte pin_rows[ROWS]      = {14, 27, 26, 25}; 
byte pin_column[COLUMNS] = {33, 32, 18, 19};  

//Setup SMTP
#define WIFI_SSID "ASV"
#define WIFI_PASSWORD "Santana21"

//Setup Email
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "esplab7@gmail.com"
#define AUTHOR_PASSWORD "zhxmcmqtrysqtueq"
#define RECIPIENT_EMAIL "victorsenny012@gmail.com"

SMTPSession smtp;
SMTP_Message email_message;

const int solenoidPin = 15;

Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROWS, COLUMNS );

const int maxInput = 32;
const int halfMaxInput = maxInput / 2;
const int passwordLength = 4;

char inputArray[maxInput]; 
char password[maxInput];
char verifyPassword[maxInput];
int arrayIndex = 0; 
int passwordIndex = 0;
int attempts = 0;
int verifyPasswordIndex = 0;
char firstline[halfMaxInput];
char secondline[halfMaxInput];

bool isPasswordDefined;
bool registerMode;
bool isChangingPassword = false;
bool isOlderValid = false;

void smtpCallback(SMTP_Status status);

void setup() {
  EEPROM.begin(EEPROM_SIZE);
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  Serial.println();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected.");
  Serial.println();
  if (EEPROM.read(0) == 1) {
    isPasswordDefined = true;
    for (int i = 0; i < maxInput; i++) {
      password[i] = EEPROM.read(i+1);
    }
  }
  pinMode(solenoidPin, OUTPUT);
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    if (key == 'A') {
      lcd.clear();
      registerMode = !registerMode;
      clearArray(registerMode, isPasswordDefined);
      if (isPasswordDefined) {
        isChangingPassword = !isChangingPassword;
      }
      if (registerMode) {
        if (!isPasswordDefined) {
          lcd.print("Modo: Registrar");
          delay(2000);
        } else { 
          lcd.print("Modo: Mudar");
          lcd.setCursor(0, 1);
          lcd.print("Senha antiga: ");
          delay(2000);
        }
        clearArray(false, isPasswordDefined);
      } else {
        lcd.print("Modo: Inserir");
        delay(2000);
      }
    } else if (key == 'B') {
      if (attempts >= 3) {
        sendEmail();
      }
      if (registerMode && !isPasswordDefined) {
        if (passwordIndex == passwordLength) {
          for (int i = 0; i < passwordLength; i++) {
            EEPROM.write(i+1, password[i]);
          }
          EEPROM.write(0, 1);
          EEPROM.commit();
          lcd.print("Senha definida!");
          delay(2000);
          registerMode = !registerMode;
          isPasswordDefined = true;
          clearArray(registerMode, isPasswordDefined);
        } else {
          lcd.print("Muito curta");
          delay(2000);
        }
      } else {
        if (!isPasswordDefined) {
          lcd.print("Senha indefinida!");
          delay(2000);
        } else {
          if (isOlderValid) {
            if (isChangingPassword) {
              isOlderValid = false;
              isChangingPassword = false;
              clearInput();
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Nova senha ");
              lcd.setCursor(0, 1);
              lcd.print("definida!");
              delay(2000);
              registerMode = !registerMode;
            }
          } else {
            bool isEqual = tryPassword();
            if (isEqual) {
              if (isChangingPassword) {
                lcd.print("Nova senha: ");
                isOlderValid = true;
                clearArray(true, isPasswordDefined);
                clearPassword();
              } else {
                digitalWrite(solenoidPin, HIGH);
                lcd.print("Bem vindo! ");
                delay(10000);
                digitalWrite(solenoidPin, LOW);
              }
              delay(2000);
              clearArray(false, isPasswordDefined);
            } else {
              lcd.print("Senha incorreta!");
              delay(2000);
              clearArray(false, isPasswordDefined);
              attempts++;
            }
          } 
        }
      }
    } else if (key == 'C') {
      clearArray(registerMode, isPasswordDefined);
      clearLCD();
    } else if (key == 'D') {
      deleteLastCharacter(false, isPasswordDefined);
    } else {
      if (key >= '0' && key <= '9') {
        if (registerMode) {
          if (isChangingPassword) {
            if (isOlderValid) {
              if (passwordIndex < passwordLength) {
                definePassword(key);
              } else {
                displayErrorMessage("Input is full");
              }
            } else {
              if (arrayIndex < maxInput) {
                addCharacter(key);
              } else {
                displayErrorMessage("Input is full");
              }          
            }
          } else {
            if (passwordIndex < passwordLength) {
              definePassword(key);
            } else {
              displayErrorMessage("Input is full");
            }
          }
        } else {
          if (arrayIndex < maxInput) {
            addCharacter(key);
          } else {
            displayErrorMessage("Input is full");
          }
        }
      }
    }
    splitArray(); 
    printToLCD();
  }
}

void addCharacter(char key) {
  if (arrayIndex < maxInput) {
    inputArray[arrayIndex] = '*';
    verifyPassword[arrayIndex++] = key;
  }
}

void definePassword(char key) {
  if (passwordIndex < passwordLength) {
    inputArray[passwordIndex] = '*';
    password[passwordIndex++] = key;
  }
}

bool tryPassword() {
  for (int i= 0; i < passwordLength; i++) {
    if (password[i] != verifyPassword[i]) {
      return false;
    }
  }
  return true;
}

void deleteLastCharacter(bool registerMode, bool isPasswordDefined) {
  if (registerMode && isPasswordDefined) {
    if (passwordIndex > 0) {
      passwordIndex--;
      password[arrayIndex] = '\0';
    }
  } else {
    if (arrayIndex > 0) {
      arrayIndex--;
      inputArray[arrayIndex] = '\0';
    }
  }
}

void clearArray(bool registerMode, bool isPasswordDefined) {
  if (registerMode && !isPasswordDefined) {
    clearPassword();
  } else {
    verifyPasswordIndex = 0;
    for (int i = 0; i < passwordLength; i++) {
      verifyPassword[i] ='\0';
    }
  }
  clearInput();
}

void clearPassword() {
  passwordIndex = 0;
  for (int i = 0; i < passwordLength; i++) {
    password[i] = '\0';
  }
}

void clearInput() {
  arrayIndex = 0;
  for (int i = 0; i < maxInput; i++) {
    inputArray[i] = '\0';
  }
}

void splitArray() {
  for (int i = 0; i < halfMaxInput; i++) {
    firstline[i] = inputArray[i];
    secondline[i] = inputArray[i + halfMaxInput];
  }
}

void printToLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(firstline);
  lcd.setCursor(0, 1);
  lcd.print(secondline);
}

void displayErrorMessage(const char *message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Error: ");
  lcd.setCursor(0, 1);
  lcd.print(message);
  delay(2000);
  lcd.clear();
}

void clearLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Input");
  lcd.setCursor(0, 1);
  lcd.print("Cleared");
  delay(2000);
  lcd.clear();
}

void sendEmail(){
  MailClient.networkReconnect(true);

  smtp.debug(1);
  smtp.callback(smtpCallback);

  Session_Config config;

  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "";

  SMTP_Message message;

  message.sender.name = F("Lockify");
  message.sender.email = AUTHOR_EMAIL;
  message.subject = F("Acesso não autorizado");
  message.addRecipient(F("Victor"), RECIPIENT_EMAIL);

  String textMsg = "Foi detectada uma tentativa indevida de acesso ao Lockfy.";
  message.text.content = textMsg.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

  if (!smtp.connect(&config)){
    ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (!smtp.isLoggedIn()){
    Serial.println("\nNot yet logged in.");
  }
  else{
    if (smtp.isAuthenticated())
      Serial.println("\nSuccessfully logged in.");
    else
      Serial.println("\nConnected with no Auth.");
  }

  if (!MailClient.sendMail(&smtp, &message))
    ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());

  if (EEPROM.read(0) == 1) {
    isPasswordDefined = true;
    registerMode = false;
    for (int i = 0; i < passwordLength; i++) {
      password[i] = EEPROM.read(i+1);
    }
  } else {
    isPasswordDefined = false;
    registerMode = true;
  }
}

void smtpCallback(SMTP_Status status){
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()){
    // ESP_MAIL_PRINTF used in the examples is for format printing via debug Serial port
    // that works for all supported Arduino platform SDKs e.g. AVR, SAMD, ESP32 and ESP8266.
    // In ESP8266 and ESP32, you can use Serial.printf directly.

    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);

      // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
      // your device time was synched with NTP server.
      // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
      // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)
      
      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    // You need to clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
  }
}
