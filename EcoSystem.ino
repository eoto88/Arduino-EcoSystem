/*
 
 Created 5 July 2014
 by Eric Turcotte
 
 */
 
#include <Time.h>
#include <TimeAlarms.h>
#include <Wire.h>
#include <DS1307RTC.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <dht.h>
#include <OneWire.h>
#include <DallasTemperature.h>

boolean DEBUG = true;

/************ ETHERNET/UDP ************/
EthernetClient client;
byte mac[] = { 0x0, 0x11, 0x95, 0x5A, 0x9E, 0x91 };
IPAddress ip(192, 168, 2, 21);
String pass = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx";
String data;
String host = "yourhost.com";
EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
char timeServer[] = "time.nist.gov";  // NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

/************ TIME ************/
tmElements_t tm;

/************ SENSORS ************/
const int ONE_WIRE_BUS_PIN = 7;
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature tankTemperatureSensor(&oneWire);
DeviceAddress tankTemperatureAddress = { 0x28, 0x23, 0xBA, 0x0A, 0x06, 0x00, 0x00, 0xC9 };
dht DHT;
#define DHT21_PIN 2

/************ LED STRIP ************/
boolean ledProblemState = false;
const int greenPin = 13;
const int redPin = 12;
const int bluePin = 11;

/************ RELAYS ************/
const int lightRelay = 25;
const int pumpRelay = 27;
const int RELAY_ON = HIGH;
const int RELAY_OFF = LOW;

/************ VARIABLES ************/
boolean lightOn = false;
boolean pumpOn = false;
const int resetPin = 5;
int countErrors = 0;

void setup() {
  /************ RESET ************/
  digitalWrite(resetPin, HIGH);
  pinMode(resetPin, OUTPUT);
  
  Serial.begin(9600);

  /************ LED STRIP ************/
  pinMode(greenPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  analogWrite(greenPin, 0);
  analogWrite(redPin, 0);
  analogWrite(bluePin, 0);
  
//  pinMode(53, OUTPUT);                 // set the SS pin as an output (necessary?)
//  digitalWrite(53, LOW);               // ? (not sure)
//  pinMode(4, OUTPUT);                  // SD select pin
//  digitalWrite(4, HIGH);               // Explicitly disable SD
//  pinMode(10, OUTPUT);                  // Ethernet select pin
//  digitalWrite(10, LOW);               // Explicitly enable Network

  /************ RELAYS ************/
  pinMode(lightRelay, OUTPUT);
  pinMode(pumpRelay, OUTPUT);s
  
  /************ ETHERNET/UDP ************/
  Ethernet.begin(mac, ip);

  log("info", "System startup.");
    
  /************ TIME ************/
  setTime();
  Alarm.delay(2000);
  setAlarmsAndTimers();
  setSyncProvider(RTC.get);
  
  // Verify if we have to start or stop the light at this time
  if(hour() >= 10 || hour() < 1) {
    StartLightRelay();
  } else if(hour() >= 1) {
    StopLightRelay();
  }
  TogglePumpRelay();
  PostTemperatureAndHumity();
  
  /************ SENSORS ************/
  tankTemperatureSensor.begin();
  tankTemperatureSensor.setResolution(tankTemperatureAddress, 10);
}

void loop() {  
  //digitalClockDisplay();
  Alarm.delay(1000); // wait one second between clock display

  ledAnimation();
}

void ledAnimation() {
  if(ledProblemState) {
    for (byte i = 1; i < 255; i++) {
      analogWrite(greenPin, 0);
      analogWrite(redPin, i);
      analogWrite(bluePin, 0);
      Alarm.delay(10);
    }
    for (byte i = 255; i > 0; i--) {
      analogWrite(greenPin, 0);
      analogWrite(redPin, i);
      analogWrite(bluePin, 0);
      Alarm.delay(10);
    }
  } else {
    for (byte i = 100; i < 200; i++) {
      analogWrite(greenPin, i + 55);
      analogWrite(redPin, i - 100);
      analogWrite(bluePin, i);
      Alarm.delay(25);
    }
    for (byte i = 200; i > 100; i--) {
      analogWrite(greenPin, i + 55);
      analogWrite(redPin, i - 100);
      analogWrite(bluePin, i);
      Alarm.delay(25);
    }
  }
}

void resetBoard() {
  digitalWrite(resetPin, LOW);
}

/* START - Light*/
void StartLightRelay() {
  lightOn = true;
  UpdateLightRelay();
  log("info", "Start light");
}

void StopLightRelay() {
  lightOn = false;
  UpdateLightRelay();
  log("info", "Stop light");
}

void UpdateLightRelay() {
  String lightState = lightOn ? "1" : "0";
  postData("lightState=" + lightState, "lightState");
  if( lightOn ) {
    digitalWrite(lightRelay, RELAY_ON);
  } else {
    digitalWrite(lightRelay, RELAY_OFF);
  }
}
/* END - Light*/

void TogglePumpRelay() {
  pumpOn = !pumpOn;
  String pumpState = pumpOn ? "1" : "0";
  postData("pumpState=" + pumpState, "pumpState");
  if( pumpOn ) {
    digitalWrite(pumpRelay, RELAY_ON);
  } else {
    digitalWrite(pumpRelay, RELAY_OFF);
  }
}

void PostStillALive() {
  postData("", "heartbeat");
}

String strTankTemperature() {
  tankTemperatureSensor.requestTemperatures();
  float currentTemp = tankTemperatureSensor.getTempC(tankTemperatureAddress);
  char buffer[10];
  return dtostrf(currentTemp, 3, 1, buffer);
}

void PostTemperatureAndHumity() {
  int chk = DHT.read21(DHT21_PIN);
  float humidity = DHT.humidity;        // In %
  float temperature = DHT.temperature;  // In Celcius
  if (isnan( temperature ) || isnan( humidity )) {
    log("error", "Failed to read humidity and temperature from DHT");
  } else {
    char bufferHumidity[10];
    dtostrf(humidity, 3, 1, bufferHumidity);
    
    char bufferTemperature[10];
    dtostrf(temperature, 3, 1, bufferTemperature);
    
    postData("roomTemperature=" + (String)bufferTemperature + "&humidity=" + (String)bufferHumidity + "&tankTemperature=" + strTankTemperature(), "data");
  }
}

void log(String type, String message) {
  postData("type=" + (type) + "&message=" + (message), "log");
}

boolean postData(String data, String action) {
  if(action.length() > 0) {
    if(data.length() > 0) {
      data += "&";
    }
    
    data += "action=" + (action);
    data += "&datetime=" + (String)getDatetime();
    data += "&pass=" + (pass);
    if(DEBUG) {
      data += "&debug=true";
    }
  }

  if( client.connected() ) {
    Serial.println("Connected");
    Alarm.delay(1000);
  }

  if ( client.connect( host.c_str(), 80) ) {
    ledProblemState = false;
    if( DEBUG ) {
      Serial.println("Connected to " + host + "...");
    }

    client.print("POST /api/instance HTTP/1.1\n");
        
    client.print("Host: "+ host +"\n");                          
    client.print("Connection: close\n");
     
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    
    client.print(data.length());                                            
    client.print("\n\n");
    client.print(data);
    
    if( DEBUG ) {
      Serial.println(data);
    }
    client.stop();
    if( DEBUG ) {
      Serial.println(F("disconnected"));
    }
  } else {
    countErrors++;
    if(countErrors >= 10) {
      resetBoard();
    }
    ledProblemState = true;
    if( DEBUG ) {
      Serial.println("Problem while connecting to " + host + "...");
      Serial.println("connected : " + (String)(client.connected() ? "true" : "false") + "");
    }
  }
}

void setTime() {
  time_t time = ntpUnixTime();
  if((time > 0) && RTC.set(time)) {
    setAlarmsAndTimers();
    log("info", "Time set.");
  } else {
    log("error", "Error while setting time.");
    Alarm.timerOnce(120, setTime);
  }
}

void setAlarmsAndTimers() {
  Alarm.alarmRepeat(7,0,0, setTime);  // Everyday 3:00am (hour + offset => 3 + 4 = 8)
  Alarm.alarmRepeat(9,0,0, StartLightRelay);  // Everyday 5:00am (hour + offset => 5 + 4 = 9)
  Alarm.alarmRepeat(1,0,0, StopLightRelay);  // Everyday 9:00pm (hour + offset => 21 + 4 = 1)
 
  Alarm.timerRepeat(30, PostStillALive);
  Alarm.timerRepeat(300, TogglePumpRelay); // Every five minutes [300 seconds]
  Alarm.timerRepeat(600, PostTemperatureAndHumity); // Every ten minutes [600 seconds]
}

time_t getDatetime() {
  if (RTC.read(tm)) {
    return makeTime(tm);
  } else {
    ledProblemState = true;
    if (RTC.chipPresent()) {
      log("error", "The DS1307 is stopped. Please run the SetTime example to initialize the time and begin running.");
    } else {
      log("error", "DS1307 read error! Please check the circuitry.");
    }
    delay(9000);
  }
}

void digitalClockDisplay() {
  if (RTC.read(tm)) {
    Serial.print(tm.Hour);
    printDigits(tm.Minute);
    printDigits(tm.Second);
    Serial.println(); 
  }
}

void printDigits(int digits) {
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

time_t ntpUnixTime() {
  long int unixTime = 0;
  Udp.begin(localPort);
  sendNTPpacket(timeServer); // send an NTP packet to a time server

  // wait to see if a reply is available
  Alarm.delay(1000);
  if (Udp.parsePacket()) {
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    // the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = ");
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unixTime = secsSince1900 - seventyYears;
    Serial.print("Unix time = ");
    Serial.println(unixTime);
  }
  Udp.flush();
  return unixTime;
}

// send an NTP request to the time server at the given address
void sendNTPpacket(char* address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
