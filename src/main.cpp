#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>



WebServer server(80);                                 // the server uses port 80 (standard port for websites
WebSocketsServer webSocket = WebSocketsServer(81);    // the websocket uses port 81 (standard port for websockets
LiquidCrystal_I2C lcd(0x27, 16, 2);

constexpr unsigned char RELAY_PIN_1 = 16, RELAY_PIN_2 = 17;


bool relayState1 = false, relayState2 = false, lcdState = true;
const char* SSID = "ESP32_Relay";
const char* PASSWORD = "admin123";
const char* webpage = "<html><head> <title>ESP Relay Control</title></head><body> <center> <p class='title'>Relay Status</p> <div class='toggle-wrapper'> <label class='toggle'> <span class='text1'>Relay 1</span> <input type='checkbox' id = 'r1'> <span class='slider'></span> </label> <div class = 'timer'> <span>Time Remaining(min)</span> <label class = 'timerslider'> <input type='checkbox' id = 'chkT1'> <span class='timersliderblock'></span> </label> <input type='number' id = 't1Input' min='1' max = '30'> <span id = 't1Val'>0</span> </div> </div> <div class='toggle-wrapper'> <label class='toggle'> <span class='text1'>Relay 2</span> <input type='checkbox' id = 'r2'> <span class='slider'></span> </label> <div class = 'timer'> <span>Time Remaining(min)</span> <label class = 'timerslider'> <input type='checkbox' id = 'chkT2'> <span class='timersliderblock'></span> </label> <input type='number' id = 't2Input' min='1' max = '30'> <span id = 't2Val'>0</span> </div> </div> <div class='toggle-wrapper'> <label class='toggle'> <span class='text1'>LCD <br>OFF ON</span> <input type='checkbox' id = 'lcd'> <span class='slider'></span> </label> </div> </center> <style> .timerslider .timersliderblock::before { position: relative; content: ''; height: 100%; width: 50%; background-color: green; transition: .4s; } .timerslider input[type='checkbox']:checked + .timersliderblock { transform: translateX(105px); background-color: red; } .timersliderblock { position: relative; height: 100%; width: 50%; background-color: green; transition: transform 0.3s, background-color 1.0s; } .timerslider { position: relative; height: 20px; width: 100%; background-color: rgb(128, 131, 85); cursor: pointer; display: flex; } .timer input[type='checkbox'] { display: none; } .timer{ position: relative; display: flex; flex-direction: column; background-color: aquamarine; border-color: black; border-width: 4px; height: 100%; width: 15%; font-size: 21px; font-weight: bold; } center { background-color: rgb(17, 16, 66); display: flex; flex-direction: column; height: 100vh; } body { background-color: black; } .text1 { position: relative; color: black; font-size: 30px; } .title { color: red; font-size: 52px; } .toggle-wrapper { display: flex; justify-content: center; margin-top: 40px; } .toggle { position: relative; display: inline-block; block-size: 32px; color: black; cursor: pointer; width: 120px; height: 120px; background-color: rgb(102, 76, 145); border: 2px solid rgb(41, 201, 187); } .toggle input[type='checkbox'] { display: none; } .toggle .slider::before { position: absolute; content: ''; height: 40px; width: 60px; left: 0px; bottom: 0px; background-color: green; transition: .4s; } .toggle input[type='checkbox']:checked + .slider::before { transform: translateX(60px); background-color: red; } </style> <script> let Socket; const toggle1 = document.getElementById('r1'); const toggle2 = document.getElementById('r2'); const lcdToggle = document.getElementById('lcd'); const t1Input = document.getElementById('t1Input'); const t2Input = document.getElementById('t2Input'); const t1Value = document.getElementById('t1Val'); const t2Value = document.getElementById('t2Val'); const chkBox1 = document.getElementById('chkT1'); const chkBox2 = document.getElementById('chkT2'); chkBox1.addEventListener('change', function(){ if(chkBox1.checked){ toggle1.checked = chkBox1.checked; t1Value.textContent = t1Input.value; let timeVal = parseInt(t1Input.value, 10); t1Input.value = null; let sendVal = 0x0100 | timeVal; console.log('Value: ' + sendVal ); Socket.send(sendVal); toggle1.dispatchEvent(new Event('change')); } } ); chkBox2.addEventListener('change', function(){ if(chkBox2.checked){ toggle2.checked = chkBox2.checked; t2Value.textContent = t2Input.value; let timeVal = parseInt(t2Input.value, 10); t2Input.value = null; let sendVal = 0x0200 | timeVal; Socket.send(sendVal); toggle2.dispatchEvent(new Event('change')); } } ); toggle1.addEventListener('change', function(){ let sendVal = 0; if(toggle1.checked){ sendVal = 0b00000001; } else{ sendVal = 0b00000000; } Socket.send(sendVal); } ); toggle2.addEventListener('change', function(){ let sendVal = 0; if(toggle2.checked){ sendVal = 0b00000011; } else{ sendVal = 0b00000010; } Socket.send(sendVal); } ); lcdToggle.addEventListener('change', function(){ let sendVal = 0; if(lcdToggle.checked){ sendVal = 0b00001000; } else{ sendVal = 0b00000100; } Socket.send(sendVal); } ); function processMessage(event){ const message = event.data; let intVal = parseInt(message); if(intVal & 0xFF00){ if(intVal & 0b00000010){ toggle1.checked = true; } else toggle1.checked = false; if(intVal & 0b00000001){ toggle2.checked = true; } else toggle2.checked = false; if(intVal & 0b00000100){ lcdToggle.checked = true; } else lcdToggle.checked = false; } } window.onload = function(event) { Socket = new WebSocket('ws://' + window.location.hostname + ':81/'); Socket.onmessage = function(event) { processMessage(event); }; } </script></body></html>";


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




void handleEvent();
void updateLCD();
void updateRelayStateToClients();
void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length);
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
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PASSWORD);


 
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(WiFi.softAPIP());

  server.on("/", []() {                               // define here wat the webserver needs to do
    server.send(200, "text/html", webpage);           //    -> it needs to send out the HTML string "webpage" to the client
  });
  server.begin();

 
  webSocket.begin();                                  // start websocket
  webSocket.onEvent(webSocketEvent);
  lcd.display();
  lcd.backlight();


  
}

void loop() {
  unsigned long long now = millis();
 
  event = EVNT_NOEVNT;
  server.handleClient();
  webSocket.loop();

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
  handleEvent();
}


void handleEvent() {
  if(event & EVNT_UPR2){
    
    relayState1 = false;
    digitalWrite(RELAY_PIN_1, !relayState1);
    updateRelayStateToClients();
  }

  if(event & EVNT_UPR1){
   
    relayState2 = false;
    digitalWrite(RELAY_PIN_2, !relayState2);
    updateRelayStateToClients();
  }

  if(event & EVNT_UPLCD){
    updateLCD();
  }
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(WiFi.softAPIP());
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







uint64_t strToInt64(uint8_t* digits, size_t count){
  if(count > 8) return 0;
  uint64_t retVal = 0;
  
  for(size_t i = 0; i < count; i++){
    retVal += (digits[i] - 48);
    if(i != count-1)
      retVal *= 10;  
  }

  return retVal;
}


void updateRelayStateToClients(){
  
  uint64_t dataPacket = 0xFF00;
  uint8_t rState1 = relayState1 ? 0b00000001 : 0b00000000;
  uint8_t rState2 = relayState2 ? 0b00000010 : 0b00000000;
  uint8_t lcdByte = lcdState ? 0b00000100 : 0b00000000;
  dataPacket |= ((rState1 | rState2) | lcdByte);
  char charArr[32]; 
  snprintf(charArr, sizeof(charArr), "%llu", dataPacket);
  webSocket.broadcastTXT((uint8_t*)charArr, strlen(charArr));


}

void updateRelayStateToClients(byte num){

  uint64_t dataPacket = 0xFF00;
  uint8_t rState1 = relayState1 ? 0b00000001 : 0b00000000;
  uint8_t rState2 = relayState2 ? 0b00000010 : 0b00000000;
  uint8_t lcdByte = lcdState ? 0b00000100 : 0b00000000;
  dataPacket |= ((rState1 | rState2) | lcdByte);
  char charArr[32]; 
  snprintf(charArr, sizeof(charArr), "%llu", dataPacket);
  webSocket.sendTXT(num, (uint8_t*)charArr, strlen(charArr) );
 
}


void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {      // the parameters of this callback function are always the same -> num: id of the client who send the event, type: type of message, payload: actual data sent and length: length of payload
  
 
  switch (type) {             
    case WStype_CONNECTED: {
      updateRelayStateToClients(num);
    }
    break;

    case WStype_DISCONNECTED:
      
    break;
    
    
    case WStype_TEXT:
                         
      
      uint64_t data = strToInt64(payload, length);
      if( data & 0x0100){
        timer1Enable = true;
        timeRMin1 = data & 0xFF;
      }
      else if( data & 0x0200){
        timer2Enable = true;
        timeRMin2 = data & 0xFF;
        
      }
      else if(data & 0b00001000){
       
        lcd.display();
        lcd.backlight();
        lcdState = true;
        updateRelayStateToClients();
      }
      else if (data & 0b00000100){
        lcdState = false;
        lcd.noDisplay();
        lcd.noBacklight(); 
        updateRelayStateToClients();
      }
      else{
        setRelay(data);
        updateRelayStateToClients(); 
      }
      break;
  }
}