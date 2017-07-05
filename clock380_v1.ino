/*
Скетч для Clock380
https://geektimes.ru/post/289389/
https://github.com/denruss/clock380

Используемые библиотеки
https://github.com/arduino-libraries/NTPClient  Version 3.1.0
https://github.com/NachtRaveVL/PCA9685-Arduino  Version 1.2.9 (Important: With other versions may not work, or work incorrectly)
https://github.com/Makuna/Rtc                   Version 2.0
https://github.com/Makuna/Task                  Version 1.1.3
https://github.com/claws/BH1750

*/

#include <Wire.h>
#include <PCA9685.h>
#include <BH1750.h>
#include <RtcDS1307.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Task.h>


char ssid[] = "***";  //  your network SSID (name)
char pass[] = "***";       // your network password


WiFiUDP ntpUDP;

TaskManager taskManager;
void UpdateDisplay(uint32_t deltaTime);
void SyncTime(uint32_t deltaTime);

FunctionTask taskDisplay(UpdateDisplay, MsToTaskTime(1000)); // in ms
FunctionTask taskSyncTime(SyncTime, MsToTaskTime(5*60*60*1000)); // in ms (каждые 5 часов)  5*60*60*1000

// You can specify the time server pool and the offset, (in seconds)
// additionaly you can specify the update interval (in milliseconds).
#define TIMEZONE 3
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", TIMEZONE*3600);

RtcDS1307<TwoWire> Rtc(Wire);

BH1750 lightMeter;

PCA9685 pwmController1, pwmController2;

float pwmFrequency = 1526.0; //supports 24Hz to 1526Hz

uint16_t lux_level[5] = {2, 10, 22, 64, 128}; // освещенность ночью....освещенность днём

uint16_t threshold = 0;   // порог переключения

uint16_t bright_level[6] = {2, 16, 32, 64, 128, 192}; // яркость ночью....яркость днём (не превышать 256)

uint16_t bright_off = 0;   // выкл

uint16_t comp = 4;   // компенсация свечения точек

RtcDateTime now, prev;

void setup()
{
    Serial.begin(115200);

    pinMode(12, OUTPUT); 
    digitalWrite(12, LOW);
    
    pinMode(15, OUTPUT); // включение V_HIGH (около 35V)
    digitalWrite(15, HIGH);

    Wire.begin();                       // Wire must be started first
    Wire.setClock(400000);              // Supported baud rates are 100kHz, 400kHz, and 1000kHz
    
    Rtc.Begin();
    
    lightMeter.begin();

    timeClient.begin();
    
    if (!Rtc.IsDateTimeValid()) 
    {
        Serial.println("RTC lost confidence in the DateTime!");

        Rtc.SetDateTime(0);
    }

    if (!Rtc.GetIsRunning())
    {
        Serial.println("RTC was not actively running, starting now");
        Rtc.SetIsRunning(true);
    }
    
    taskManager.StartTask(&taskDisplay); // start with turning it on
    taskManager.StartTask(&taskSyncTime); // start with turning it on

    pwmController1.resetDevices();       // Software resets all PCA9685 devices on Wire line

    pwmController1.init(B000000, 0x04);        // Address pins A5-A0
    pwmController1.setPWMFrequency(pwmFrequency); 
    pwmController2.init(B000001, 0x04);        // Address pins A5-A0
    pwmController2.setPWMFrequency(pwmFrequency); //

    delay(30);

    //Test();
    
    lightMeter.readLightLevel(); //
    UpdateDisplay(0);
    SyncTime(0);



}

uint16_t GetBright(uint8_t print)
{
  static uint16_t bright = bright_level[3];
  uint16_t lux = lightMeter.readLightLevel(); //
  
  
  if ((lux < lux_level[0])) bright = bright_level[0];
  if ((lux > lux_level[0] + threshold) && (lux < lux_level[1] - threshold)) bright = bright_level[1];
  if ((lux > lux_level[1] + threshold) && (lux < lux_level[2] - threshold)) bright = bright_level[2];
  if ((lux > lux_level[2] + threshold) && (lux < lux_level[3] - threshold)) bright = bright_level[3];
  if ((lux > lux_level[3] + threshold) && (lux < lux_level[4] - threshold)) bright = bright_level[4];
  if ((lux > lux_level[4] + threshold)) bright = bright_level[5];

  if (print == 1) {
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx");

    Serial.print("Bright value: ");
    Serial.print(bright);
    Serial.println(" ");
  }

  return bright;
}


void UpdateDisplay(uint32_t deltaTime)
{
  static uint8_t dot = 1;
  uint16_t bright = GetBright(1);
  
  if ((now.Hour() >= 21) && (now.Hour() <= 23) || (now.Hour() >= 0) && (now.Hour() <= 5) ) ShowDots(1, 1, bright + comp); // с 21.00 до 6.00 не мигает двоеточие, а светится точка
  else ShowDots(dot, 2, bright + comp);

  if (dot == 1) dot = 0; else dot = 1;
  
  ShowTime(bright);
}

void SyncTime(uint32_t deltaTime)
{
  unsigned long epoch;
  uint16_t bright = GetBright(0);
  uint8_t shift = 0;
  
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.forceSleepWake();
  delay(10);
  WiFi.begin(ssid, pass);
          
  // Wait for connection
  for (int i = 0; i < 20; i++)
  {
    if ( WiFi.status() != WL_CONNECTED ) {
      delay ( 500 );
      Serial.print ( "." );

      // сигнализируем что коннектимся
      bright = GetBright(0);
      switch (shift)
        {
          case 0: { ShowDots(0, 1, bright + comp); shift = 1; break; }
          case 1: { ShowDots(1, 1, bright + comp); shift = 2; break; }
          case 2: { ShowDots(1, 2, bright + comp); shift = 3; break; }
          case 3: { ShowDots(1, 3, bright + comp); shift = 0; break; }

        }
      
      delay ( 500 );
    }
  }

  if ( WiFi.status() != WL_CONNECTED ) {
    Serial.println("No WiFi connection");
    WiFi.forceSleepBegin();
    delay(10);
    }
  else {
    Serial.println("");
          
    Serial.println("WiFi connected, IP address:");
    Serial.println(WiFi.localIP());

    delay (10);

    Serial.print("Connecting to NTP. \n");
    // Wait for connection
    for (int i = 0; i < 2; i++)
    {
      if ( !timeClient.update() ) {
        delay ( 250 );
        Serial.print ( "." );
        delay ( 250 );
      }
      else {
         Serial.print("Connect, get time: \n");
         epoch = timeClient.getEpochTime();
         //timeClient.setTimeOffset(3);
            
         Rtc.SetDateTime(RtcDateTime(epoch));
    
         Serial.println(timeClient.getFormattedTime());
         
         // сигнализируем тремя точками что получили время
         bright = GetBright(0);
         ShowDots(0, 1, bright + comp);
         delay (500);
         ShowDots(1, 3, bright + comp);
         delay (500);
         ShowDots(0, 1, bright + comp);
         delay (500);
         ShowDots(1, 3, bright + comp);         
         delay (500);
  
         break;
       }
      }
    
    WiFi.disconnect();
    WiFi.forceSleepBegin();
    delay(10);
  }
}
  


void ShowTime(uint16_t bright)
{
  now = Rtc.GetDateTime();

  if (now != prev) {    
  
     ShowDigit(now.Hour()/10, 0, bright);
     ShowDigit(now.Hour()%10, 1, bright);
     ShowDigit(now.Minute()/10, 2, bright);
     ShowDigit(now.Minute()%10, 3, bright);
  }

prev = now; 

}

void ShowDigit(uint8_t digit, uint8_t position, uint16_t bright)
{
                     //A,B,C,D,E,G,F   
uint8_t  digits[70] = {1,1,1,1,1,0,1, //0
                       0,1,1,0,0,0,0, //1
                       1,1,0,1,1,1,0, //2
                       1,1,1,1,0,1,0, //3
                       0,1,1,0,0,1,1, //4
                       1,0,1,1,0,1,1, //5
                       1,0,1,1,1,1,1, //6
                       1,1,1,0,0,0,0, //7
                       1,1,1,1,1,1,1, //8
                       1,1,1,1,0,1,1  //9
                       };

uint8_t  connections[28] = {1,0,4,3,5,6,2, //1
                            15,12,9,10,13,11,14, //2
                            1,0,4,3,5,6,2, //3
                            15,12,9,10,13,11,14  //4                            
                       };
                       
if (bright < lux_level[0]) bright = bright_level[0]; //защита
if (bright > lux_level[4]) bright = bright_level[5]; //защита
                       
for (uint8_t i = 0; i < 7; i++) { 
  
      switch (position)  
      {
         case 0: { if (digits[i+digit*7] == 1) { pwmController1.setChannelPWM(connections[i + position*7], bright); }  else { pwmController1.setChannelPWM(connections[i + position*7], bright_off); }  break; } 
         case 1: { if (digits[i+digit*7] == 1) { pwmController1.setChannelPWM(connections[i + position*7], bright); }  else { pwmController1.setChannelPWM(connections[i + position*7], bright_off); }  break; } 
         case 2: { if (digits[i+digit*7] == 1) { pwmController2.setChannelPWM(connections[i + position*7], bright); }  else { pwmController2.setChannelPWM(connections[i + position*7], bright_off); }  break; } 
         case 3: { if (digits[i+digit*7] == 1) { pwmController2.setChannelPWM(connections[i + position*7], bright); }  else { pwmController2.setChannelPWM(connections[i + position*7], bright_off); }  break; } 
      }
        
  }

}


void ShowDots(uint8_t EN, uint8_t N, uint16_t bright)
{ 

  switch (N)  
  {
    case 1: { pwmController1.setChannelPWM(7, bright); pwmController1.setChannelPWM(8, bright_off); break; }
    case 2: { pwmController1.setChannelPWM(8, bright); pwmController1.setChannelPWM(7, bright_off); break; }
    case 3: { pwmController1.setChannelPWM(7, bright); pwmController1.setChannelPWM(8, bright); break; }
  }
  
    
  if (EN == 0) { pwmController1.setChannelPWM(7, bright_off); pwmController1.setChannelPWM(8, bright_off); }

}

void Test(void)
{
  for (uint8_t i = 0; i < 9; i++) { 
    ShowDigit(i, 0, bright_level[2]);
    ShowDigit(i, 1, bright_level[2]);
    ShowDigit(i, 2, bright_level[2]);
    ShowDigit(i, 3, bright_level[2]);
    delay(300);
  }
   // delay(300);

   while(1);

}


void loop()
{

  taskManager.Loop();

} 



