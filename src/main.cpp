#include <Arduino.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>






constexpr unsigned char RELAY_PIN_1 = 16, RELAY_PIN_2 = 17;


bool relayState1 = false, relayState2 = false, lcdState = true;
const char* SSID = "ALMA";
const char* PASSWORD = "(EVIL)(ALMA)(1322)";

constexpr unsigned char EVNT_NOEVNT = 0b00000000;
constexpr unsigned char EVNT_UPLCD = 0b00000001;
constexpr unsigned char EVNT_UPR1 = 0b00000010;
constexpr unsigned char EVNT_UPR2 = 0b00000100;

bool timer1Enable = false;
bool timer2Enable = false;


unsigned char event = 0;
unsigned long long prevTime = 0;
unsigned long long elapseTime = 0, secondsAccum = 0, minuteAccum = 0;
char timeRSec1 = 0, timeRSec2 = 0;
unsigned short timeRMin1 = 0, timeRMin2 = 0;
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiServer server(1322);
void handleData(byte* buffer, uint32_t sz, WiFiClient*);
void handleEvent(WiFiClient*);
void updateLCD();
void updateRelayStateToClients(WiFiClient*);
void setup() {
  Serial.begin(115200);
  
  lcd.init();
  
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("Connecting");
  lcd.noDisplay();
  lcd.noBacklight(); 
  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  digitalWrite(RELAY_PIN_1, HIGH);
  digitalWrite(RELAY_PIN_2, HIGH);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  while(WiFi.status() != WL_CONNECTED){
    delay(150);
  }
 
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());
  lcd.display();
  lcd.backlight();


  server.begin(); 
}

void loop() {



  unsigned long long now = millis();
  event = EVNT_NOEVNT;
  WiFiClient client = server.available();
  if(client){
    updateRelayStateToClients(&client);
    while(client.connected()) {
      now = millis();
      event = EVNT_NOEVNT;
      byte buffer[8] = {};
      
      if(client.available()){
        client.readBytes(buffer, 8);
       
        uint64_t* b = (uint64_t*)buffer;
        handleData(buffer, 8, &client);
        
      }


      if(secondsAccum >= 1000){
        secondsAccum = 0;

        if(timer1Enable) {
          if(timeRMin1 > 0 || timeRSec1 > 0){
            if(timeRSec1 > 0) {
              timeRSec1 -= 1;
              event |= EVNT_UPLCD;
            }     
            else {
              timeRSec1 = 60;
              timeRMin1--;
            }

          }
          else {
            event |= EVNT_UPR1;
            event |= EVNT_UPLCD;
            timer1Enable = false;
          }

        }
        
        if(timer2Enable){
          if(timeRMin2 > 0 || timeRSec2 > 0){
            if(timeRSec2 > 0) {
              timeRSec2 -= 1;
              event |= EVNT_UPLCD;
            }     
            else {
              timeRSec2 = 60;
              timeRMin2--;
            }
            
          }
          else {
            timer2Enable = false;
            event |= EVNT_UPR2;
            event |= EVNT_UPLCD;
          }
        }
        
        
        
      }


      elapseTime = now - prevTime;
      secondsAccum += elapseTime;
      prevTime = now;
      handleEvent(&client);







    }

    
  }



  if(secondsAccum >= 1000){
    secondsAccum = 0;

    if(timer1Enable) {
      if(timeRMin1 > 0 || timeRSec1 > 0){
        if(timeRSec1 > 0) {
          timeRSec1 -= 1;
          event |= EVNT_UPLCD;
        }     
        else {
          timeRSec1 = 60;
          timeRMin1--;
        }

      }
      else {
        event |= EVNT_UPR1;
        event |= EVNT_UPLCD;
        timer1Enable = false;
      }

    }
    
    if(timer2Enable){
      if(timeRMin2 > 0 || timeRSec2 > 0){
        if(timeRSec2 > 0) {
          timeRSec2 -= 1;
          event |= EVNT_UPLCD;
        }     
        else {
          timeRSec2 = 60;
          timeRMin2--;
        }
        
      }
      else {
        timer2Enable = false;
        event |= EVNT_UPR2;
        event |= EVNT_UPLCD;
      }
    }
    
    
    
  }
  
  elapseTime = now - prevTime;
  secondsAccum += elapseTime;
  prevTime = now;
  handleEvent(&client);
  
 
  
  
  
  
}


void handleEvent(WiFiClient* clPtr) {
  if(event & EVNT_UPR2){
    
    relayState1 = false;
    digitalWrite(RELAY_PIN_1, !relayState1);
    updateRelayStateToClients(clPtr);
  }

  if(event & EVNT_UPR1){
   
    relayState2 = false;
    digitalWrite(RELAY_PIN_2, !relayState2);
    updateRelayStateToClients(clPtr);
  }

  if(event & EVNT_UPLCD){
    updateLCD();
  }
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());
  lcd.setCursor(0,0);

  if(timeRMin1 > 0 || timeRSec1 > 0){
    lcd.print(timeRMin1); lcd.print(":");
    lcd.print((int)timeRSec1); lcd.print("  ");
  } 
  else {
    lcd.print("R1:");
    lcd.print(relayState2 ? "ON" : "OFF");
  }
  if(timeRMin2 > 0 || timeRSec2 > 0){
    lcd.print("  ");
    lcd.print(timeRMin2); lcd.print(":");
    lcd.print((int)timeRSec2);
  }
  else{
    lcd.print(" R2:");
    lcd.print(relayState1 ? "ON" : "OFF");
  }

}

void setRelay(uint64_t data){
  if(data & 0b00000010){ 
    if(data & 0b00000001){
      relayState1 = true;
    }
    else{
      if(timeRSec1 <= 0) // it will remain on if the there is time left
        relayState1 = false;

    }
  }
  else{
    if(data & 0b00000001){
      relayState2 = true;
    }
    else{
      if(timeRSec2 <= 0)
        relayState2 = false;
    }
  }

  
  digitalWrite(RELAY_PIN_1, !relayState1);
  digitalWrite(RELAY_PIN_2, !relayState2);
  updateLCD();
}






void updateRelayStateToClients(WiFiClient* clPtr){
  
  uint64_t dataPacket = 0;
  uint8_t rState1 = relayState1 ? 0b00000001 : 0b00000000;
  uint8_t rState2 = relayState2 ? 0b00000010 : 0b00000000;
  uint8_t lcdByte = lcdState ? 0b00000100 : 0b00000000;
  
  dataPacket |= (((rState1 | rState2) | lcdByte));
  String val(dataPacket);
  clPtr->write(val.c_str()); 


}



void handleData(byte* buffer, uint32_t sz, WiFiClient* clPtr){
     
      String val((const uint8_t*)buffer, 8);

      
      uint64_t data = val.toInt();
      
      if( data & 0x0100){
        timer1Enable = true;
        timeRMin1 = data & 0xFF;
        setRelay(0b01);
        
      }
      else if( data & 0x0200){
        timer2Enable = true;
        timeRMin2 = data & 0xFF;
        setRelay(0b11);
      }
      else if(data & 0b00001000){
       
        lcd.display();
        lcd.backlight();
        lcdState = true;
        updateRelayStateToClients(clPtr);
      }
      else if (data & 0b00000100){
        lcdState = false;
        lcd.noDisplay();
        lcd.noBacklight(); 
        updateRelayStateToClients(clPtr);
      }
      else{
        
        setRelay(data);
        updateRelayStateToClients(clPtr); 
      }
}

