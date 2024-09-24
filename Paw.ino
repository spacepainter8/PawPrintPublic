#include <Keypad.h>
#include <Wire.h>
#include <hd44780.h>                       
#include <hd44780ioClass/hd44780_I2Cexp.h> 
#include <Adafruit_Fingerprint.h>
#include <Servo.h>



// State enum -> denotes states of the system
enum State {
  START_MENU,
  REGISTERING_USERNAME,
  LOGGING_IN_USERNAME,
  FORG_USERNAME,
  REGISTERING_USERNAME_ERROR,
  REGISTERING_PASSWORD,
  REGISTERING_PASSWORD_ERROR,
  REGISTERING_PASSWORD_CONFIRM,
  REGISTERING_PASSWORD_CONFIRM_ERROR,
  REGISTERING_CHOICE,
  REGISTERING_FINGERPRINT,
  REGISTERING_FINGERPRINT_ERROR,
  REGISTERING_FINGERPRINT_WAIT,
  REGISTERING_FINGERPRINT_CONFIRM,
  REGISTERING_PASSWORD_YES_NO,
  REGISTERING_FINGERPRINT_YES_NO,
  LOGGING_IN_PASSWORD,
  LOGGING_IN_ERROR,
  FORG_USERNAME_ERROR,
  FORG_FINGERPRINT,
  FORG_FINGERPRINT_ERROR,
  FORG_PASSWORD,
  FORG_PASSWORD_ERROR,
  FORG_PASSWORD_CONFIRM,
  FORG_PASSWORD_CONFIRM_ERROR,
  MAIN_MENU,
  ALL_USERS,
  CREDENTIALS,
  USERNAME,
  PASSWORD,
  USERNAME_ERROR,
  PASSWORD_ERROR,
  PASSWORD_CONFIRM,
  PASSWORD_CONFIRM_ERROR,
  FINGERPRINT,
  FINGERPRINT_ERROR,
  FINGERPRINT_WAIT,
  FINGERPRINT_CONFIRM,
  FINGERPRINT_REMOVE,
  INTRUDER,
  INTRUDER_FINGERPRINT
};

const char startMenuItems[3][21] = {
  "Registracija",
  "Uloguj se",
  "Zaboravljena lozinka"
};

const char regularMenuItems[2][15] = {
  "Svi korisnici",
  "Opcije"
};

const char credentialsMenuItems[8][17] = {
  "Promeni kor. ime",
  "Dodaj lozinku",
  "Promeni lozinku",
  "Ukloni lozinku",
  "Dodaj otisak",
  "Ukloni otisak",
  "Obrisi nalog",
  "Promeni status"
};



// system variables
State currentState = START_MENU;
int currentMenuItem = 0;

char username[17] = "";
uint8_t usernameIndex = 0;

char usernameFromList[17] = "";
uint8_t usernameFromListIndex = 0;

char password[17] = "";
uint8_t passwordIndex = 0;

uint8_t passwordConfirmIndex = 0;

int fingerprintID = -1;
// String fingerprintIDString = "";

uint8_t admin = 0;

// KEYPAD configuration
const byte ROWS = 4; 
const byte COLS = 4; 
const char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
const byte rowPins[ROWS] = {9, 8, 7, 6}; 
const byte colPins[COLS] = {5, 4, 3, 2}; 

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// LCD configuration
hd44780_I2Cexp lcd;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;

// FINGERPRINT configuration
SoftwareSerial mySerial(10, 11);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// SERVO configuration
Servo servo;

// MOTION SENSOR 
uint8_t msState = LOW;
unsigned long startTime = 0;
const char intruderDisable[4] = "1234";
uint8_t intruderDisableIndex = 0;

uint8_t incorrectLogin = 0;


// int i = 0;  
// int p = -1;
// char key;
// char c;



void setup(){
  Serial.begin(115200);
  Serial.setTimeout(1);
  pinMode(13, INPUT);
  pinMode(A3, OUTPUT);
  
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.lineWrap();
  finger.begin(57600);

  servo.attach(12);
  servo.write(0);

  // this is so the motion sensor calibrates
  delay(5000);

  lcd.print(startMenuItems[currentMenuItem]);
  
  
  servo.detach();
  // servo.attach(12);
  // servo.write(0);

}

void clear(uint8_t clearOption, State newState){
  
  if (clearOption & 1){
    username[0] = '\0'; usernameIndex = 0;
  }
  if (clearOption & (1 << 1)){
    password[0] = '\0'; passwordIndex = 0;
  }
  if (clearOption & (1 << 2)) if (fingerprintID != -1) finger.deleteModel(fingerprintID);
  if (clearOption & (1 << 3)) fingerprintID = -1;
  if (clearOption & (1 << 4)) passwordConfirmIndex = 0;
  if (clearOption & (1 << 5)) {
    usernameFromList[0] = '\0'; usernameFromListIndex = 0;
  }

  currentState = newState;
  lcd.clear();

}

void simpleErrorHandler(char* errMsg, char* msg){
  lcd.clear();
  lcd.print(errMsg);
  delay(1000);
  lcd.clear();
  if (currentState == INTRUDER_FINGERPRINT) lcd.print(F("ULJEZ DETEKTOVAN!!!!!!!!!!!!!!!!"));
  else lcd.print(msg);
}

void errorHandler(char* errMsg, State newState){
  
  lcd.clear();
  lcd.print(errMsg);
  delay(1000);
  if (currentState == LOGGING_IN_PASSWORD){
    incorrectLogin++;
    if (incorrectLogin >= 3) {
      intruderMode();return;
    }   
  }
  lcd.clear();
  lcd.print(F("Pokusaj ponovo *"));
  lcd.print(F("Vrati se nazad #"));
  currentState = newState;
}

void getUsernameFromList(){
  usernameFromList[0] = '\0';
  usernameFromListIndex = 0;
  while(!Serial.available());
  char c = Serial.read();

  while (c != '!'){
    usernameFromList[usernameFromListIndex++] = c;
    while(!Serial.available());
    c = Serial.read();
  }
  usernameFromList[usernameFromListIndex++] = '\0';

  lcd.clear();
  lcd.print(usernameFromList);

}

uint8_t passwordUpdate(State errorState, State newState, char key){
  if (key != 'D'){
    if (passwordIndex >= 16){
      // password too long, handle the error
      errorHandler("Lozinka maksimalno 16 karaktera", errorState);
      return 1;
    } else {
      // collect password chars
      password[passwordIndex++] = key;
      lcd.print(key);
    }
  } else {
    // finished inputting password
    if (passwordIndex == 0){
      // password is empty, handle the error
      errorHandler("Lozinka ne sme biti prazna", errorState);
      return 1;
    }

    password[passwordIndex++] = '\0';

    // check password validation
    uint8_t alphaCnt = 0;
    uint8_t numCnt = 0;
    uint8_t symbCnt = 0;
    
    int i;
    for (i=0;i<17;i++){
      if (password[i] == '\0') break;
      if (isAlpha(password[i])) alphaCnt++;
      else if (isDigit(password[i])) numCnt++;
      else symbCnt++;
    }

    if (alphaCnt == 0 || numCnt == 0 || symbCnt == 0){
      lcd.clear();
      lcd.print(F("Lozinka mora sadrzati slovo"));
      delay(1000);
      lcd.clear();
      lcd.print(F("broj, i znak"));
      delay(1000);
      lcd.clear();
      lcd.print(F("Pokusaj ponovo *"));
      lcd.print(F("Vrati se nazad #"));
      currentState = errorState;
      return 1;
    }

    // ask user for password confirmation
    lcd.clear();
    lcd.print(F("Potvrdi lozinku:"));
    lcd.setCursor(0,1);
    currentState = newState;

  }

  return 0;
}

void passwordUpdateError(State s1, State s2){
  char key = keypad.getKey();
  if (key && key == '*'){
    clear(0b10, s1);
    lcd.print(F("Lozinka: "));
    lcd.setCursor(0,1);
    return;
  }

  if (key && key == '#'){
    password[0] = '\0';
    passwordIndex = 0;
    
    if (currentState == FORG_PASSWORD_ERROR){
      username[0] = '\0';
      usernameIndex = 0;
      fingerprintID = -1;
    }
   
    currentState = s2;
    currentMenuItem = 0;
    lcd.clear();
    if (currentState == START_MENU)
      lcd.print(startMenuItems[currentMenuItem]);
    else if (currentState == MAIN_MENU) {
      lcd.print(regularMenuItems[currentMenuItem]);
    } else if (currentState == ALL_USERS){
      lcd.print(usernameFromList);
    }
    
    return;
  }
}

void passwordUpdateConfirm(State errorState, State newState, char key){
  if (key != 'D'){
    if (passwordConfirmIndex >= 16
    || password[passwordConfirmIndex++] != key){
      errorHandler("Lozinke se ne poklapaju", errorState);
      return;
    } else lcd.print(key);
  } else {
    if (passwordConfirmIndex == 0){
      errorHandler("Lozinke se ne poklapaju", errorState);
      return;
    } else {
    // all good, update password through python
      Serial.print(7);
      Serial.print(password);

      while(!Serial.available());
      char c = Serial.read();
      if (c == '1'){
        errorHandler("Doslo je do greske", errorState);
        return;
      } else {
       
        password[0] = '\0';
        passwordIndex = 0;
        passwordConfirmIndex = 0;
        fingerprintID = -1;

        lcd.clear();
        lcd.print(F("Lozinka uspesno promenjena"));
        delay(1000);
        currentState = newState;
        lcd.clear();
        currentMenuItem = 0;
        if (currentState == START_MENU) {
          username[0] = '\0';
          usernameIndex = 0;
          lcd.print(startMenuItems[currentMenuItem]);
        } else if (currentState == MAIN_MENU){
          lcd.print(regularMenuItems[currentMenuItem]);
        } else if (currentState == ALL_USERS){
          lcd.print(usernameFromList);
        }
        return;
        
      }
    }
  }
}

void passwordUpdateConfirmError(State s1, State s2){
  char key = keypad.getKey();
  if (key && key == '*'){
    passwordConfirmIndex = 0;
    currentState = s1;
    lcd.clear();
    lcd.print(F("Potvrdi lozinku:"));
    lcd.setCursor(0,1);
    return;
  }

  if (key && key == '#'){
    if (currentState == FORG_PASSWORD_CONFIRM_ERROR){
      username[0] = '\0';
      usernameIndex = 0;
      fingerprintID = -1;
    }
  

    password[0] = '\0';
    passwordIndex = 0;
    passwordConfirmIndex = 0;

    currentState = s2;
    lcd.clear();
    currentMenuItem = 0;
    if (currentState == START_MENU)
      lcd.print(regularMenuItems[currentMenuItem]);
    else if (currentState == MAIN_MENU) {
      lcd.print(startMenuItems[currentMenuItem]);
    } else if (currentState == ALL_USERS){
      lcd.print(usernameFromList);
    }
    return;
  }
}

void fingerprint(State errState, State newState, uint8_t p){
  if (p != FINGERPRINT_OK){
    // error with the sensor
    errorHandler("GRESKA!", errState);
  } else {
    // all good

    p = finger.image2Tz(1);
    if (p != FINGERPRINT_OK){
      errorHandler("GRESKA!", errState);
      return;
    }

    // check whether this fingerprint is already in the database
    p = finger.fingerSearch();
    if (p == FINGERPRINT_OK ){
      errorHandler("Otisak prsta je vec u upotrebi", errState);
      return;
    }

    // wait for user to take off finger
    lcd.clear();
    lcd.print(F("Uklonite prst sa senzora"));
    currentState = newState;
  }
}

void fingerprintError(char key, State s1, State s2){ 
  if (key == '*'){
    // try inputting the fingerprint again
    if (fingerprintID != -1) finger.deleteModel(fingerprintID);
    lcd.clear();
    lcd.print(F("Prislonite prst na senzor"));
    currentState = s1;
    return;
  }

  if (key == '#') {
    // go back to start menu
    if (currentState == REGISTERING_FINGERPRINT_ERROR){
      username[0] = '\0';
      usernameIndex = 0;
      password[0] = '\0';
      passwordIndex = 0;
      passwordConfirmIndex = 0;
    }

    if (fingerprintID != -1) finger.deleteModel(fingerprintID);
    fingerprintID = -1;
    currentState = s2;
    currentMenuItem = 0;
    lcd.clear();
    if (currentState == START_MENU)
      lcd.print(startMenuItems[currentMenuItem]);
    else if (currentState == MAIN_MENU)
      lcd.print(regularMenuItems[currentMenuItem]);
    else if (currentState == ALL_USERS)
      lcd.print(usernameFromList);
    return;
  }
}

void fingerprintWait(State st){
  lcd.clear();
  lcd.print(F("Ponovo prislonite prst"));
  currentState = st;
}

void fingerprintConfirm(State errState, uint8_t p){
    
  if (p != FINGERPRINT_OK){
    errorHandler("GRESKA!", errState);
    return;
  } else {  
    
    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK){
      errorHandler("GRESKA!", errState);
      return;
    }

    // wait for user to take off finger
    p = finger.createModel();
    

    if (p != FINGERPRINT_OK){
      if (p != FINGERPRINT_ENROLLMISMATCH) errorHandler("GRESKA!", errState);
      else errorHandler("Otisci se ne poklapaju", errState);
      return;
    }

    // model successfuly created
    // store the model
    
    // get one of the available fingerprint IDs from Python
    Serial.print(3);
    while (!Serial.available());

    // fingerprintIDString = Serial.readString();
    // fingerprintID = fingerprintIDString.toInt();
    fingerprintID = readNumber();
    

    p = finger.storeModel(fingerprintID);
    if (p != FINGERPRINT_OK){
      errorHandler("GRESKA!", errState);
      return;
    } 
    // model successfully stored

    if (currentState == REGISTERING_FINGERPRINT_CONFIRM){
      if (password[0] == '\0'){
      // ask user whether they want to input a password
      lcd.clear();
      lcd.print(F("Dodajete li lozinku?"));
      lcd.print(F(" Da *   Ne #"));

      currentState = REGISTERING_PASSWORD_YES_NO;
      return;

      } else {
        // password has already been inputted
        // insert user into the system

        Serial.print(1);
        Serial.print(password);
        Serial.print(F("!"));
        Serial.print(fingerprintID);
        password[0] = '\0';
        passwordIndex = 0;
        passwordConfirmIndex = 0;
        fingerprintID = -1;

        while (!Serial.available());
        char c = Serial.read();
        lcd.clear();
        if (c == '0'){
          // all good
          lcd.print(F("Korisnik uspesno registrovan"));
        } else {
          // error
          lcd.print(F("Doslo je do greske"));
        }
        delay(1000);
        currentState = START_MENU;
        msState = LOW;
        startTime = 0;  
        lcd.clear();
        lcd.print(startMenuItems[currentState]);
      }
    } else if (currentState == FINGERPRINT_CONFIRM){
      // add fingerprint into the database
      Serial.print('E');
      Serial.print(fingerprintID);

      while (!Serial.available());
      char c = Serial.read();
      lcd.clear();
      lcd.print(F("Otisak prsta uspesno dodat"));
      delay(1000);

      // credentialEnd
      credentialEnd();
    }

  }
      
}

uint8_t fingerprintSearch(char* st, uint8_t p){

  if (p != FINGERPRINT_OK){
    simpleErrorHandler("DOSLO JE DO GRESKE", st);
    return 1;
  }

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK){
    simpleErrorHandler("DOSLO JE DO GRESKE", st);
    return 1;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_NOTFOUND || (p == FINGERPRINT_OK && finger.confidence<100)){
    simpleErrorHandler("Nepoznat otisak prsta", st);
    return 1;
  } else if (p != FINGERPRINT_OK){
    simpleErrorHandler("DOSLO JE DO GRESKE", st);
    return 1;
  }

  

  fingerprintID = finger.fingerID;
  return 0;
}

uint8_t readNumber(){
  char number[17] = "";
  passwordIndex = 0;
  while(!Serial.available());
  char c = Serial.read();
  while(c != '!'){  
    number[passwordIndex++] = c;
    while(!Serial.available());
    c = Serial.read();
  }

  number[passwordIndex] = '\0';

  uint8_t numOfPrints = atoi(number);
  

  return numOfPrints;

}

void credentialEnd(){
  lcd.clear();
  if (admin == 0){
    currentState = MAIN_MENU;
    currentMenuItem = 0;
    lcd.print(regularMenuItems[0]);
  } else{
    currentState = ALL_USERS;
    lcd.print(usernameFromList);
  }
}

void logout(){
  msState = LOW;
  startTime = 0;
  if (currentState != INTRUDER_FINGERPRINT) lockClose();
  clear(0b111011, START_MENU);
  admin = 0;
  currentMenuItem = 0;
  lcd.print(startMenuItems[currentMenuItem]);
}

void lockOpen(){
  servo.attach(12);
  delay(200);
  int pos = 0;
  while(pos <= 180){
    servo.write(pos);
    pos +=1;
  }
  delay(200);
  servo.detach();
}

void lockClose(){
  servo.attach(12);
  delay(200);
  int pos = 180;
  while (pos >= 0){
    servo.write(pos);
    pos -=1;
  }
  delay(200);
  servo.detach();
}

void intruderMode(){
  lcd.clear();
  lcd.print(F("ULJEZ DETEKTOVAN!!!!!!!!!!!!!!!!"));
  currentState = INTRUDER;
  msState = LOW;
  startTime = 0;
  incorrectLogin = 0;
  tone(A3, 440);
}



void loop(){

  if (currentState == START_MENU){
    // check motion sensor stuff
    if (digitalRead(13) == 1){
      // detecting motion
      if (msState == LOW){
        // 0-1 => motion started
        msState = HIGH;
        startTime = millis();
      } else {
        // ongoing motion, check whether 60s have passed
        if ((millis() - startTime) >= (unsigned long) 60*1000){
          intruderMode();
          return;
        }
      }
    } else {
      // no motion
      if (msState = HIGH){
        // 1-0 -> motion stopped
        msState = LOW;
        startTime = 0;
      }
    }
  }


  switch (currentState) {

    case START_MENU:{
      
      
      uint8_t motionSensor = digitalRead(13);
      uint8_t p = finger.getImage(); 
      if (p != FINGERPRINT_NOFINGER){
        // probe for motion sensor input, prevents cyber attacks
        if (motionSensor == 0){
          intruderMode();
          return;
        }
        // user is trying to log in via fingerprint
        currentMenuItem = 0;
        uint8_t val = fingerprintSearch(startMenuItems[currentMenuItem], p);
        if (val == 1) {
          incorrectLogin++;
          if (incorrectLogin >= 3) intruderMode();
          else {msState = LOW;
                startTime = 0;
                currentState = START_MENU;}
          return;
        }
        // need to ask python to send username 
        Serial.print(4);
        Serial.print(fingerprintID);

        usernameIndex = 0;
        while(!Serial.available());
        char c = Serial.read();

        if (c == '$'){
          fingerprintID = -1;
          currentMenuItem = 0;
          lcd.clear();
          lcd.print(F("Nepoznat otisak prsta"));
          delay(1000);
          lcd.clear();
          lcd.print(startMenuItems[currentMenuItem]);
          return;
        }

        while (c != '!'){
          username[usernameIndex++] = c;
          while(!Serial.available());
          c = Serial.read();
        }
        username[usernameIndex++] = '\0';

        // get isAdmin
        while(!Serial.available());
        c = Serial.read();
        if (c == '0') admin = 0;
        else admin = 1;
        
        lockOpen();

        lcd.clear();
        lcd.print(F("Dobrodosli"));
        lcd.setCursor(0, 1);
        lcd.print(username);
        delay(2000);

        if (admin == 1){
          int i;
          for (i = 0;i<17;i++) usernameFromList[i] = username[i];
        }

        credentialEnd();

        return;
      }

     
      motionSensor = digitalRead(13);
      char key = keypad.getKey();
     
      if (key){
        // probe for motion sensor input, prevents cyber attacks
        if (motionSensor == 0){
          intruderMode();
          return;
        }

        if (key == '#'){
          currentMenuItem = (currentMenuItem + 1) % 3;
        } else if (key == '*'){
          currentMenuItem = (currentMenuItem == 0)?2:(currentMenuItem - 1);
        } else if (key == 'D'){
          currentState = currentMenuItem + 1;
          lcd.clear();
          lcd.print(F("Korisnicko ime: "));
          lcd.setCursor(0, 1);
          return;
        }

        
        lcd.clear();
        lcd.print(startMenuItems[currentMenuItem]);
        
      }

      

      break;
    }

    case REGISTERING_USERNAME:{
      char key = keypad.getKey();
      if (key){
        if (key != 'D'){
          if (usernameIndex >= 16) {
            // username too long, handle the error
            errorHandler("Kor. ime maksimalno 16 karaktera", REGISTERING_USERNAME_ERROR);
            return;
          } else {
            // collect username chars
            username[usernameIndex++] = key;
            lcd.print(key);
          }
        } else {
          // finished inputting username
          if (usernameIndex == 0){
            // username is empty, handle the error
            errorHandler("Kor. ime ne sme biti prazno", REGISTERING_USERNAME_ERROR);
            return;
          }
          username[usernameIndex++] = '\0';
          Serial.print(0);
          Serial.print(username);
          username[0] = '\0';
          usernameIndex = 0;

          // check whether username is unique
          while(!Serial.available());
          char c = Serial.read();

          if (c == '0'){
            // username is unique
            finger.getParameters();
            finger.getTemplateCount();
            if (finger.templateCount < 128){
              lcd.clear();
              lcd.print(F("Otisak prsta *"));
              lcd.setCursor(0, 1);
              lcd.print(F("Lozinka #"));
              currentState = REGISTERING_CHOICE; 
              return;
            } else {
              lcd.clear();
              lcd.print(F("Lozinka:"));
              lcd.setCursor(0, 1);
              currentState = REGISTERING_PASSWORD;
              return;
            }
          } else {
            // username is NOT unique
            errorHandler("Kor. ime mora biti jedinstveno", REGISTERING_USERNAME_ERROR);
            return;
          }

          
        }
      }

      break;
    }

    case REGISTERING_USERNAME_ERROR:{
      char key = keypad.getKey();
      if (key && key == '*'){
        clear(0b1, REGISTERING_USERNAME);
        lcd.print(F("Korisnicko ime: "));
        return;
      }

      if (key && key == '#'){
        currentMenuItem = 0;
        clear(0b1, START_MENU);
        lcd.print(startMenuItems[currentMenuItem]);
        return;
      }

      break;
    }

    case REGISTERING_PASSWORD:{
      char key = keypad.getKey();
      if (key){
        if (key != 'D'){
          if (passwordIndex >= 16){
            // password too long, handle the error
            errorHandler("Lozinka maksimalno 16 karaktera", REGISTERING_PASSWORD_ERROR);
            return;
          } else {
            // collect password chars
            password[passwordIndex++] = key;
            lcd.print(key);
          }
        } else {
          // finished inputting password
          if (passwordIndex == 0){
            // password is empty, handle the error
            errorHandler("Lozinka ne sme biti prazna", REGISTERING_PASSWORD_ERROR);
            return;
          }

          password[passwordIndex++] = '\0';

          // check password validation
          uint8_t alphaCnt = 0;
          uint8_t numCnt = 0;
          uint8_t symbCnt = 0;
          
          int i;
          for (i=0;i<17;i++){
            if (password[i] == '\0') break;
            if (isAlpha(password[i])) alphaCnt++;
            else if (isDigit(password[i])) numCnt++;
            else symbCnt++;
          }

          if (alphaCnt == 0 || numCnt == 0 || symbCnt == 0){
            lcd.clear();
            lcd.print(F("Lozinka mora sadrzati slovo"));
            delay(1000);
            errorHandler("broj, i znak", REGISTERING_PASSWORD_ERROR);
            return;
          }

          // ask user for password confirmation
          lcd.clear();
          lcd.print(F("Potvrdi lozinku:"));
          lcd.setCursor(0,1);
          currentState = REGISTERING_PASSWORD_CONFIRM;

        }
      }
      break;
    }

    case REGISTERING_PASSWORD_ERROR:{
      char key = keypad.getKey();
      if (key && key == '*'){
        clear(0b10, REGISTERING_PASSWORD);
        lcd.print(F("Lozinka: "));
        lcd.setCursor(0,1);
        return;
      }

      if (key && key == '#'){
        currentMenuItem = 0;
        clear(0b1111, START_MENU);
        lcd.print(startMenuItems[currentMenuItem]);
        return;
      }

      break;
    }

    case REGISTERING_PASSWORD_CONFIRM:{
      char key = keypad.getKey();
      if (key){
        if (key != 'D'){
          if (passwordConfirmIndex >= 16
          || password[passwordConfirmIndex++] != key){
            errorHandler("Lozinke se ne poklapaju", REGISTERING_PASSWORD_CONFIRM_ERROR);
            return;
          } else lcd.print(key);
        } else {
          if (passwordConfirmIndex == 0){
            errorHandler("Lozinke se ne poklapaju", REGISTERING_PASSWORD_CONFIRM_ERROR);
            return;
          } else {
          // all good, write the user into the database

          // check fingerprint stuff first

            if (fingerprintID == -1){
              // fingerprint hasn't been inputted
              lcd.clear();
              lcd.print(F("Otisak prsta?"));
              lcd.setCursor(0, 1);
              lcd.print(F("Da *    Ne #"));

              currentState = REGISTERING_FINGERPRINT_YES_NO;
              return;
            } else {
              // fingerprint has already been inputted
              // insert user into the database
              Serial.print(1);
              Serial.print(password);
              Serial.print(F("!"));
              Serial.print(fingerprintID);
              password[0] = '\0';
              passwordIndex = 0;
              passwordConfirmIndex = 0;
              fingerprintID = -1;

              while (!Serial.available());
              char  c = Serial.read();
              lcd.clear();
              if (c == '0'){
                // all good
                lcd.print(F("Korisnik uspesno registrovan"));
              } else {
                // error
                lcd.print(F("Doslo je do greske"));
              }
              delay(1000);
              currentState = START_MENU;
              msState = LOW;
              startTime = 0;
              lcd.clear();
              lcd.print(startMenuItems[currentState]);

            }

            

          }
        }
      }
      break;
    }

    case REGISTERING_PASSWORD_CONFIRM_ERROR:{
      char key = keypad.getKey();
      if (key && key == '*'){
        clear(0b10000, REGISTERING_PASSWORD_CONFIRM);
        lcd.print(F("Potvrdi lozinku:"));
        lcd.setCursor(0,1);
        return;
      }

      if (key && key == '#'){
        currentMenuItem = 0;
        clear(0b11111, START_MENU);
        lcd.print(startMenuItems[currentMenuItem]);
        return;
      }

      break;
    }

    case LOGGING_IN_USERNAME:{
      // user is inputting username for logging in
      char key = keypad.getKey();
      if (key){
        if (key != 'D'){
          if (usernameIndex >= 16) return;
          else {
            username[usernameIndex++] = key;
            lcd.print(key);
          }
        } else {
          // finished inputting username
          username[usernameIndex++] = '\0';

          // ask user for password input
          lcd.clear();
          lcd.print(F("Lozinka:"));
          lcd.setCursor(0, 1);
          currentState = LOGGING_IN_PASSWORD;
          return;
        }
      }

      break;
    }

    case LOGGING_IN_PASSWORD:{
      // user is inputting password for logging in

      char key = keypad.getKey();
      if (key){
        if (key != 'D'){
          if (passwordIndex >= 16) return;
          else {
            password[passwordIndex++] = key;
            lcd.print('*');
          }
        } else {
          // finished inputting password
          password[passwordIndex++] = '\0';

          // now send stuff to python for credential check
          Serial.print(2);
          Serial.print(username);
          Serial.print('!');
          Serial.print(password);

          while(!Serial.available());
          char c = Serial.read();
         

          if (c == '2' || c == '3'){
            if (c == '2') errorHandler("Korisnik ne postoji", LOGGING_IN_ERROR);
            else errorHandler("Pogresna lozinka", LOGGING_IN_ERROR);
            return;
          } else {
            admin = (c == '0')?0:1;
            lcd.clear();
            lcd.print(F("Dobrodosli"));
            lcd.setCursor(0, 1);
            lcd.print(username);
            delay(2000);

            if (admin == 1){
              int i;
              for (i = 0;i<17;i++) usernameFromList[i] = username[i];
            }

            credentialEnd();

            lockOpen();

            return;
          }

        }
      }

      break;
    }

    case LOGGING_IN_ERROR:{
      char key = keypad.getKey();
      if (key && key == '*'){
        clear(0b11, LOGGING_IN_USERNAME);
        lcd.print(F("Korisnicko ime:"));
        lcd.setCursor(0,1);
        return;
      }

      if (key && key == '#'){
        currentMenuItem = 0;
        clear(0b11, START_MENU);
        lcd.print(startMenuItems[currentMenuItem]);
        return;
      }

      break;
    }

    case REGISTERING_CHOICE:{
      char key = keypad.getKey();

      if (key && key == '*'){
        // registering via fingerprint
        lcd.clear();
        lcd.print(F("Prislonite prst na senzor"));
        currentState = REGISTERING_FINGERPRINT;
        return;
      }

      if (key && key == '#'){
        // registering via password
        lcd.clear();
        lcd.print(F("Lozinka:"));
        lcd.setCursor(0, 1);
        currentState = REGISTERING_PASSWORD;
        return;
      }

      break;
    }

    case REGISTERING_FINGERPRINT:{
      // detecting fingerprint for the first time

      uint8_t p = finger.getImage();
      if (p != FINGERPRINT_NOFINGER){
        // something's happening with the sensor
        fingerprint(REGISTERING_FINGERPRINT_ERROR, REGISTERING_FINGERPRINT_WAIT, p);
      }
      break;
    }

    case REGISTERING_FINGERPRINT_ERROR:{
      char key = keypad.getKey();
      if (key){
        fingerprintError(key, REGISTERING_FINGERPRINT, START_MENU);
      }
      break;
    }

    case REGISTERING_FINGERPRINT_WAIT:{
      uint8_t p = finger.getImage();
      if (p == FINGERPRINT_NOFINGER){
        fingerprintWait(REGISTERING_FINGERPRINT_CONFIRM);
      }
      break;
    }

    case REGISTERING_FINGERPRINT_CONFIRM:{
      uint8_t p = finger.getImage();

      if (p != FINGERPRINT_NOFINGER){
        fingerprintConfirm(REGISTERING_FINGERPRINT_ERROR, p);
      }

      break;
    }

    case REGISTERING_PASSWORD_YES_NO:{
      char key = keypad.getKey();

      if (key && key == '*'){
        lcd.clear();
        lcd.print(F("Lozinka:"));
        lcd.setCursor(0, 1);
        currentState = REGISTERING_PASSWORD;
        return;
      }

      if (key && key == '#'){
        Serial.print(1);
        Serial.print(password);
        Serial.print(F("!"));
        Serial.print(fingerprintID);
        password[0] = '\0';
        passwordIndex = 0;
        passwordConfirmIndex = 0;
        fingerprintID = -1;

        while (!Serial.available());
        char c = Serial.read();
        lcd.clear();
        if (c == '0'){
        // all good
          lcd.print(F("Korisnik uspesno registrovan"));
        } else {
          // error
          lcd.print(F("Doslo je do greske"));
        }
        
        delay(1000);
        currentState = START_MENU;
        msState = LOW;
        startTime = 0;
        lcd.clear();
        lcd.print(startMenuItems[currentState]);
      }


      
      
      break;
    }

    case REGISTERING_FINGERPRINT_YES_NO:{
      char key = keypad.getKey();

      if (key && key == '*'){
        lcd.clear();
        lcd.print(F("Prislonite prst na senzor"));
        currentState = REGISTERING_FINGERPRINT;
        return;
      }




      if (key && key == '#'){
        Serial.print(1);
        Serial.print(password);
        Serial.print(F("!"));
        Serial.print(fingerprintID);
        password[0] = '\0';
        passwordIndex = 0;
        passwordConfirmIndex = 0;
        fingerprintID = -1;

        while (!Serial.available());
        char c = Serial.read();
        lcd.clear();
        if (c == '0'){
        // all good
          lcd.print(F("Korisnik uspesno registrovan"));
        } else {
          // error
          lcd.print(F("Doslo je do greske"));
        }
        
        delay(1000);
        currentState = START_MENU;
        msState = LOW;
        startTime = 0;
        lcd.clear();
        lcd.print(startMenuItems[currentState]);
      }


      
      
      break;
    }

    case FORG_USERNAME:{
      char key = keypad.getKey();
      if (key){
        if (key != 'D'){
          if (usernameIndex >= 16) return;
          else {
            username[usernameIndex++] = key;
            lcd.print(key);
          }
        } else {
          // finished inputting username
          username[usernameIndex++] = '\0';

          // python: check whether user exists and if user has fingerprint in the system
          //   (we are looking for 2FA kind of system)

          Serial.print(5);
          Serial.print(username);
          // 0 -> all good, 1 -> no user in the system, 2 -> user has no fingerprint in the system
          while(!Serial.available());
          char c = Serial.read();
          if (c == '1' || c == '2'){
            lcd.clear();
            if (c == '1'){
              lcd.print(F("Korisnik ne postoji u sistemu"));
            } else {
              lcd.print(F("Korisnik nema otisak prsta"));
              delay(1000);
              lcd.clear();
              lcd.print(F("Kontaktirajte admina"));
            }
            // lcd.print((res == '1')?"Korisnik ne postoji u sistemu":"Korisnik nema otisak prsta");
            delay(1000);
            lcd.clear();
            lcd.print(F("Pokusaj ponovo *"));
            lcd.setCursor(0, 1);
            lcd.print(F("Vrati se nazad #"));
            currentState = FORG_USERNAME_ERROR;
            return;
          } else {
            // prompt the user for fingerprint
            lcd.clear();
            lcd.print(F("Prislonite prst na senzor"));
            currentState = FORG_FINGERPRINT;
          }

          return;
        }
      }

      break;
    }

    case FORG_USERNAME_ERROR:{
      char key = keypad.getKey();
      if (key && key == '*'){
        clear(0b1, FORG_USERNAME);
        lcd.print(F("Korisnicko ime: "));
        return;
      }

      if (key && key == '#'){
        currentMenuItem = 0;
        clear(0b1, START_MENU);
        lcd.print(startMenuItems[currentMenuItem]);
        return;
      }

      break;
    }

    case FORG_FINGERPRINT:{
      uint8_t p = finger.getImage();
      if (p != FINGERPRINT_NOFINGER){
        // user has interacted with the sensor
        if (p != FINGERPRINT_OK){
          errorHandler("DOSLO JE DO GRESKE", FORG_FINGERPRINT_ERROR);
          return;
        }

        p = finger.image2Tz();
        if (p != FINGERPRINT_OK){
          errorHandler("DOSLO JE DO GRESKE", FORG_FINGERPRINT_ERROR);
          return;
        }

        p = finger.fingerSearch();
        if (p == FINGERPRINT_NOTFOUND || (p == FINGERPRINT_OK && finger.confidence<100)){
          errorHandler("Nepoznat otisak prsta", FORG_FINGERPRINT_ERROR);
          return;
        } else if (p != FINGERPRINT_OK){
          errorHandler("DOSLO JE DO GRESKE", FORG_FINGERPRINT_ERROR);
          return;
        }

        fingerprintID = finger.fingerID;

        // fingerprint recognized
        // ask python to match it to the given user

        Serial.print(6);
        Serial.print(fingerprintID);

        // 1 -> authorization failed, 0 -> fingerprint ok
        while(!Serial.available());
        char c = Serial.read();
        if (c == '1'){
          // authorization failed
          lcd.clear();
          lcd.print(F("Otisak prsta ne odgovara"));
          delay(1000);
          lcd.clear();
          lcd.print(F("datom korisnickom imenu"));
          delay(1000);
          lcd.clear();
          lcd.print(F("Pokusaj ponovo *"));
          lcd.setCursor(0, 1);
          lcd.print(F("Vrati se nazad #"));
          currentState = FORG_FINGERPRINT_ERROR;
          return;
        } else {
          // fingerprint ok

          // start the password change
          lcd.clear();
          lcd.print(F("Lozinka:"));
          lcd.setCursor(0, 1);
          currentState = FORG_PASSWORD;
          return;
        }

      }
      
      break;
    }

    case FORG_FINGERPRINT_ERROR:{
      char key = keypad.getKey();
      if (key && key == '*'){
        clear(0b1000, FORG_FINGERPRINT);
        lcd.print(F("Prislonite prst na senzor"));
        return;
      }

      if (key && key == '#'){
        currentMenuItem = 0;
        clear(0b1001, START_MENU);
        lcd.print(startMenuItems[currentMenuItem]);
        return;
      }

      break;
    }

    case FORG_PASSWORD:{
      char key = keypad.getKey();
      if (key){
        passwordUpdate(FORG_PASSWORD_ERROR, FORG_PASSWORD_CONFIRM, key);
      }
      break;
    }

    case FORG_PASSWORD_ERROR:{
      passwordUpdateError(FORG_PASSWORD, START_MENU);
      break;
    }

    case FORG_PASSWORD_CONFIRM:{
      char key = keypad.getKey();
      if (key){
        passwordUpdateConfirm(FORG_PASSWORD_CONFIRM_ERROR, 
          START_MENU, key);
      }
      break;
    }

    case FORG_PASSWORD_CONFIRM_ERROR:{
      passwordUpdateConfirmError(FORG_PASSWORD_CONFIRM, START_MENU);

      break;
    }


    case MAIN_MENU:{
      char key = keypad.getKey();
      if (key){
        if (key == '#'){
          currentMenuItem = (currentMenuItem + 1) % 2;
        } else if (key == '*'){
          currentMenuItem = (currentMenuItem == 0)?1:(currentMenuItem - 1);
        } else if (key == 'D'){
          currentState = MAIN_MENU + currentMenuItem + 1;
          if (currentState == ALL_USERS) {
            lcd.clear();
            lcd.print(username);
            int i;
            for (i = 0;i<17;i++) usernameFromList[i] = username[i];
          } else {
            currentState == CREDENTIALS;
            currentMenuItem = 0;
            lcd.clear();
            lcd.print(credentialsMenuItems[0]);
          }
          return;
        } else if (key == '0'){
          logout();
          return;
        }

        lcd.clear();
        lcd.print(regularMenuItems[currentMenuItem]);
        
      }
      break;
    }

    case ALL_USERS:{

      char key = keypad.getKey();
      if (key){
        if (key == '*'){
          // previous user

          // ask python for previous user from the list
          Serial.print(8);
          Serial.print(usernameFromList);

          // receive the answer
          getUsernameFromList();

        } else if (key == '#'){
          // next user

          // ask python for next user from list
          Serial.print(9);
          Serial.print(usernameFromList);

          // receive the answer
          getUsernameFromList();
        } else if (key == '0' && admin == 0){
          // back to main menu
          currentState = MAIN_MENU;
          currentMenuItem = 0;
          lcd.clear();
          lcd.print(regularMenuItems[0]);
          return;

        } else if (key == '0' && admin == 1){
          logout();
        } else if (admin == 1 && key == 'D'){
          currentState = CREDENTIALS;
          currentMenuItem = 0;
          lcd.clear();
          lcd.print(credentialsMenuItems[0]);
        }
      }
      break;
    }

    case CREDENTIALS:{
      char key = keypad.getKey();
      if (key){
        if (key == '#'){
          if (admin == 0) currentMenuItem = ((currentMenuItem + 1) % 7);
          else currentMenuItem = ((currentMenuItem + 1) % 8);
        } else if (key == '*'){
          if (admin == 0) currentMenuItem = (currentMenuItem == 0)?6:(currentMenuItem - 1);
          else currentMenuItem = (currentMenuItem == 0)?7:(currentMenuItem - 1);
        } else if (key == 'D'){
          lcd.clear();
          if (currentMenuItem == 0) {
            lcd.print(F("Korisnicko ime: "));
            usernameFromList[0] = '\0';
            usernameFromListIndex = 0;
            currentState = USERNAME;
          } else if (currentMenuItem == 1 || currentMenuItem == 2) {

            // check whether user already has password in system
            Serial.print('B');
            while(!Serial.available());
            char c = Serial.read();
            if ((c == '1' && currentMenuItem == 1) || (c == '0' && currentMenuItem == 2)){
              lcd.clear();
              if (c == '1') lcd.print(F("Korisnik vec ima lozinku"));
              else lcd.print(F("Korisnik nema lozinku"));
              delay(1000);

              credentialEnd();

              return;
            }
            

            lcd.print(F("Lozinka:"));
            lcd.setCursor(0, 1);
            password[0] = '\0';
            passwordIndex = 0;
            passwordConfirmIndex = 0;
            currentState = PASSWORD;
          } else if (currentMenuItem == 3){
            // removing password
            // check whether user has password at all
            Serial.print('B');
            while(!Serial.available());
            char c = Serial.read();
            if (c == '0'){
              lcd.clear();
              lcd.print(F("Korisnik nema lozinku"));
            } else {
              // can't let password be the only way for the user to log in
              //  ergo user must have a fingerprint in the system 

              Serial.print('C');
              while(!Serial.available());
              c = Serial.read();
              if (c == '1'){
                lcd.clear();
                lcd.print(F("Korisnik nema otisak prsta"));
              } else {
                Serial.print('D');
                while(!Serial.available());
                c = Serial.read();
                lcd.clear();
                lcd.print(F("Lozinka uspesno obrisana"));
              }
            }
            delay(1000);

            credentialEnd();
          } else if (currentMenuItem == 4 || currentMenuItem == 5){
            // adding and removing a fingerprint

            fingerprintID = -1;
            // fingerprintIDString = "";
            lcd.clear();
            lcd.print(F("Prislonite prst na senzor"));
            if (currentMenuItem == 4) currentState = FINGERPRINT;
            else currentState = FINGERPRINT_REMOVE;
          } else if (currentMenuItem == 6){
            // delete user
            Serial.print('G');

            // get number of prints to delete
            uint8_t numOfPrints = readNumber();

            // delete the fingerprints from the system
            uint8_t i = 0;
            for (;i<numOfPrints;i++){
              fingerprintID = readNumber();
              uint8_t p = finger.deleteModel(fingerprintID);
            }

            while(!Serial.available());
            char c = Serial.read();

            fingerprintID = -1;

            lcd.clear();
            lcd.print(F("Nalog uspesno obrisan"));
            delay(1000);

            // for admin ask python whether we actually need to log out or not
            if (admin == 1){
              while(!Serial.available());
              c = Serial.read();
              if (c == '0') logout();
              else {
                currentState = ALL_USERS;
                lcd.clear();
                lcd.print(username);
              }
            }
            else logout();

          } else if (currentMenuItem == 7){
            // toggle admin status for user
            Serial.print('H');

            while(!Serial.available());
            char c = Serial.read();

            if (c == '0'){
              // admin changed admin status for themselves
              admin = 1 - admin;
            }

            lcd.clear();
            lcd.print(F("Status admina uspesno promenjen"));
            delay(1000);
            credentialEnd();

          }
          return;
        } else if (key == '0'){
          
          if (admin == 1){
            int i;
            for (i = 0;i<17;i++) usernameFromList[i] = username[i];
          }

          credentialEnd();
          
          return;
        }

     
        lcd.clear();
        lcd.print(credentialsMenuItems[currentMenuItem]);
        
      }
      break;
    }

    case USERNAME:{
      char key = keypad.getKey();
      if (key){
        if (key != 'D'){
          if (usernameFromListIndex >= 16) {
            // username too long, handle the error
            errorHandler("Kor. ime maksimalno 16 karaktera", USERNAME_ERROR);
            return;
          } else {
            // collect username chars
            usernameFromList[usernameFromListIndex++] = key;
            lcd.print(key);
          }
        } else {
          // finished inputting username
          if (usernameFromListIndex == 0){
            // username is empty, handle the error
            errorHandler("Kor. ime ne sme biti prazno", USERNAME_ERROR);
            return;
          }
          usernameFromList[usernameFromListIndex++] = '\0';
          Serial.print('A');
          Serial.print(usernameFromList);

          // check whether username is unique
          while(!Serial.available());
          char c = Serial.read();

          if (c == '0'){
            // username is unique
            lcd.clear();
            lcd.print(F("Kor. ime uspesno promenjeno"));
            delay(1000);

            credentialEnd();
            
            int i = 0;
            for(;i<17;i++) username[i] = usernameFromList[i];
            
          } else {
            // username is NOT unique
            errorHandler("Kor. ime mora biti jedinstveno", USERNAME_ERROR);
          }

          return;

          
        }
      }

      break;
    }

    case USERNAME_ERROR:{
      char key = keypad.getKey();
      if (key && key == '*'){
        clear(0b100000, USERNAME);
        lcd.print(F("Korisnicko ime: "));
        return;
      }

      if (key && key == '#'){
        currentMenuItem = 0;  

        
        if (admin == 0){
          clear(0b100000, MAIN_MENU);
          lcd.print(regularMenuItems[0]);
        } else {
          clear(0b100000, ALL_USERS);
          lcd.print(username);
          int i;
          for (i = 0;i<17;i++) usernameFromList[i] = username[i];
        }
        return;
      }

      break;
    }

    case PASSWORD:{
      // adding a password or changing it

      char key = keypad.getKey();
      if (key){
        passwordUpdate(PASSWORD_ERROR, PASSWORD_CONFIRM, key);
      }
      break;
    }

    case PASSWORD_ERROR:{
      if (admin == 1) passwordUpdateError(PASSWORD, ALL_USERS); 
      else passwordUpdateError(PASSWORD, MAIN_MENU);
      break;
    }

    case PASSWORD_CONFIRM:{
      char key = keypad.getKey();
      if (key){
        if (admin == 1) passwordUpdateConfirm(PASSWORD_CONFIRM_ERROR, ALL_USERS, key);
        else passwordUpdateConfirm(PASSWORD_CONFIRM_ERROR, MAIN_MENU, key);
      }
      break;
    }

    case PASSWORD_CONFIRM_ERROR:{
      if (admin == 1) passwordUpdateConfirmError(PASSWORD_CONFIRM, ALL_USERS);
      else passwordUpdateConfirmError(PASSWORD_CONFIRM, MAIN_MENU);
      break;
    }

    case FINGERPRINT:{
      // detecting fingerprint for the first time

      uint8_t p = finger.getImage();
      if (p != FINGERPRINT_NOFINGER){
        // something's happening with the sensor
        fingerprint(FINGERPRINT_ERROR, FINGERPRINT_WAIT, p);
      }
      break;
    }

    case FINGERPRINT_ERROR:{
      char key = keypad.getKey();
      if (key){
        if (admin == 1) fingerprintError(key, FINGERPRINT, ALL_USERS); 
        else fingerprintError(key, FINGERPRINT, MAIN_MENU);
      }
      break;
    }

    case FINGERPRINT_WAIT:{
      uint8_t p = finger.getImage();
      if (p == FINGERPRINT_NOFINGER){
        fingerprintWait(FINGERPRINT_CONFIRM);
      }
      break;
    }

    case FINGERPRINT_CONFIRM:{
      uint8_t p = finger.getImage();

      if (p != FINGERPRINT_NOFINGER){
        fingerprintConfirm(FINGERPRINT_ERROR, p);
      }

      break;
    }

    case FINGERPRINT_REMOVE:{
      uint8_t p = finger.getImage();
      if (p != FINGERPRINT_NOFINGER){
        currentMenuItem = 0;
        if (admin == 1){
          uint8_t val = fingerprintSearch(usernameFromList, p);
          if (val == 1) {
            currentState = ALL_USERS;
            return;
          };
        } else {
          uint8_t val = fingerprintSearch(regularMenuItems[currentMenuItem], p);
          if (val == 1) {
            currentState = MAIN_MENU;
            return;
          };
        }
        
        // fingerprintID now contains the fingerprint's ID from the system

        // check whether fingerprint is registered to the logged in user
        // problem -> user has no other fingerprint in the system and has no password
        Serial.print('F');
        Serial.print(fingerprintID);

        while(!Serial.available());
        char c = Serial.read();
        // 1 -> fingerprint user mismatch, 2 -> no other logging in method
        lcd.clear();
        if (c == '1' || c == '2'){
          if (c == '1') lcd.print(F("Otisak prsta ne pripada Vama"));
          else lcd.print(F("Ne postoji drugi kredencijal"));
        } else {
          p = finger.deleteModel(fingerprintID);

          if (p == FINGERPRINT_OK){
            // deleted successfully
            lcd.print(F("Otisak prsta uspesno obrisan"));
          } else {
            // didn't manage to delete
            lcd.print(F("Doslo je do greske"));
          }
        }

        delay(1000);


        credentialEnd();

      }
      
      break;
    }

    case INTRUDER:{
      char key = keypad.getKey();
      if (key){
        if (intruderDisable[intruderDisableIndex++] == key){
          if (intruderDisableIndex == 4){
            lcd.clear();
            lcd.print(F("Otisak prsta:"));
            currentState = INTRUDER_FINGERPRINT;
            intruderDisableIndex = 0;
          }
        } else intruderDisableIndex = 0;
      }
      break;
    }

    case INTRUDER_FINGERPRINT:{
      uint8_t p = finger.getImage(); 
      if (p != FINGERPRINT_NOFINGER) {
        uint8_t val = fingerprintSearch("", p);
        if (val == 1){
          currentState = INTRUDER;
        } else {
          // check whether the fingerprint corresponds to an admin
          Serial.print('I');
          Serial.print(fingerprintID);
          while(!Serial.available());
          char c = Serial.read();
          
          if (c == '1'){
            lcd.clear();
            lcd.print(F("ALARM DEAKTIVIRAN"));
            delay(1000);
            logout();
            noTone(A3);
          } else {
            currentState = INTRUDER;
            lcd.clear();
            lcd.print(F("ULJEZ DETEKTOVAN!!!!!!!!!!!!!!!!"));
          }
          
        }

        fingerprintID = -1;
      }
      break;
    }



  }


  

}
