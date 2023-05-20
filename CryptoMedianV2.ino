#define TOUCH_MODULES_CST_SELF
#include "TFT_eSPI.h" /* Please use the TFT library provided in the library. */
#include "TouchLib.h"
#include "Wire.h"
#include "pin_config.h"
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "esp_sleep.h"
#include "fonts.h"
#include "MapFloat.h"
#include <TickTwo.h>


WiFiMulti wifiMulti;


int Zeilenabstand = 26; //normal = 20; bei Schriftgröße 4 passen 8 Zeilen auf das Display. Bei 7 Zeilen auf 25 gehen
int Pixeloffset  = +10; //Verschiebung nach oben

// Binance API endpoint and symbols to get prices for
String SYMBOL0 = "JASMYBTC";
String SYMBOL1 = "JASMYBUSD";
String SYMBOL2 = "BTCBUSD";
const int limit = 24;
String Interval = "1h";
const int period = limit-1;
String endpoint = "https://api.binance.com/api/v3/klines?symbol=";
String endpointWithLimit = endpoint + SYMBOL0 + "&interval=" +  Interval + "&limit=" + String(limit);
float median;
float klinprices[limit];
float prices[6];
String symbols[6];

// Binance API credentials
const char* binance_api_key = "YOUR_API_KEY";
const char* binance_api_secret = "YOUR_API_SECRET";


//----------------------- Akkuanzeige-----------------------------------------
int Vbattindikatorposition;
int Spannungsfarbe=TFT_ORANGE;
float Vbatt ;
float Vbattprozent ;
int Batteriebetrieb_Max ;
int Batteriebetrieb_Min ;
int Ladevorgang_Akkuvoll;
int Ladevorgang_Akkuleer;
int Ladeprozente;

// Displaysettings
int bright=4;
int brightnesses[7]={35,70,105,140,175,210,250};

// Declaration für TickTwo
void getPricesKlines ();
void getBinancePrice ();
void calculatemedian ();
void calc_battery_symbol ();
void readout_battery ();
void Display_Brightness ();

TickTwo timer1(getPricesKlines, 30000); // alle 30s Klines laden
TickTwo timer2(getBinancePrice, 900); // jede 900ms aktuelle werte laden
TickTwo timer3(calculatemedian, 5000);// jede 5s median rechnen
TickTwo timer4(calc_battery_symbol, 300); // batteriesymbol alle 200ms aufrischen um scrolleffekt zu erreichen
TickTwo timer5(readout_battery, 2000); // alle 2s batterie auslesen und mitteln
TickTwo timer6(Display_Brightness, 1500); // alle 1s Display_Brightness berechnen und PWM setzen

// Display
TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);


void Display_Brightness() {
  int Helligkeit=mapFloat(Vbatt, 1480, 2500, 50, 255);
  if (Helligkeit >255) {Helligkeit=255;}
  analogWrite(PIN_LCD_BL, Helligkeit); //PWM setzen 
}


//what it says on the tin
void printArray(int *a, int x)
{
  for (int i = 0; i < x; i++)
  {
    Serial.print(String(i+1) + "=");
    Serial.print(a[i], DEC);
    Serial.print(" ");
  }
  Serial.println();
}

void readout_battery (){
  // Vbatt = analogRead(PIN_BAT_VOLT);
  int vbattarray[40]; //50 mal werte lesen
  int n = sizeof(vbattarray) / sizeof(vbattarray[0]); //Berechnen Sie die Anzahl der Elemente im Array
  for (int i = 0; i < n; i++) { //Füllen Sie das Array mit Anlalogmessungen
    vbattarray[i] = analogRead(PIN_BAT_VOLT);
    // vbattarray[i] = analogRead(PIN_ADC2_11);
    
    // Serial.println("array:" + String(i) +"="+ String(vbattarray[i]));
  }
  // printArray(vbattarray, n); 
  qsort(vbattarray, n, sizeof(int), cmpfunc); //Sortieren Sie das Array in aufsteigender Reihenfolge
  int numOutliers = round(0.2 * n);// Identifizieren Sie die Anzahl der 40% Ausreißer (jeweils vorne und hinten), damit 60% aller Werte überbleiben
  // Serial.println("numOutliers:" + String(numOutliers));
  // printArray(vbattarray, n); 
  for (int i = numOutliers; i < n-numOutliers; i++) { //Entferne aller Ausreißerwerte vorne und hinten aus dem Array
   vbattarray[i-numOutliers] = vbattarray[i]; }
  n -= numOutliers*2; // Serial.println(String(vbattarray));
  // printArray(vbattarray, n); 

  //------------Mittelwertberechung
  int sum = 0;
    for (int i = 0; i < n; i++) {
      sum += vbattarray[i];
    }
  Vbatt = static_cast<float>(sum) / n;
}

// Function to get klines from Binance API
void getPricesKlines() {

  // Make HTTP request to Binance API
  
  HTTPClient http;
  http.begin(endpointWithLimit);
  int httpResponseCode = http.GET();

  // Check if request was successful
  if (httpResponseCode == 200) {
    // Parse JSON response
    String payload = http.getString();
    // Serial.println("Response:" + payload);
    
    DynamicJsonDocument doc(300*limit);
    deserializeJson(doc, payload);
    // Extract prices from JSON
    for (int i = 0; i < (limit); i++) {
      klinprices[i] = doc[i][4].as<float>(); //close price is position 4
      // Serial.println(String(i) + " " + String(klinprices[i], 9));
    }
  }  
}

 // comparison function used by qsort
int cmpfunc(const void* a, const void* b) {
  return (*(int*)a - *(int*)b);
}
 
 // Median berechnen
void calculatemedian() {
   
   float array[limit]; //Initialisieren Sie das Array mit Klines Größe
   int n = sizeof(array) / sizeof(array[0]); //Berechnen Sie die Anzahl der Elemente im Array

  //Füllen Sie das Array mit Klines Werten
  for (int i = 0; i < n; i++) {
    array[i] = klinprices[i];
    // Serial.println("array:" + String(i) +"="+ String(array[i]));
  }

  //Sortieren Sie das Array in aufsteigender Reihenfolge
 qsort(array, n, sizeof(int), cmpfunc);
  
  

  if (n % 2 == 0) {    //f (n % 2 == 0) prüft, ob die Variable n durch 2 ohne Rest teilbar ist. Wenn der Rest der Division gleich Null ist, bedeutet dies, dass n eine gerade Zahl ist.
    median = (array[n/2-1] + array[n/2]) / 2; //Da das Array bereits sortiert ist, ist das Element an der Indexposition n/2 das Median-Element
  }
  else {
    median = array[n/2]; //Da das Array bereits sortiert ist, ist das Element an der Indexposition n/2 das Median-Elemen
  }
  Serial.println("Medianwert: " + String(median,8));
}

void getBinancePrice () {

  HTTPClient http;
  String url = "https://api.binance.com/api/v3/ticker/price?symbols=[%22" + SYMBOL0 + "%22,%22" +  SYMBOL1 + "%22,%22" + SYMBOL2 + "%22]";

  http.begin(url);
  int httpResponseCode = http.GET();

  if (httpResponseCode == HTTP_CODE_OK) {
    String payload = http.getString();
    // Serial.println("Response:" + payload);

    // Parse JSON
    const size_t capacity = JSON_ARRAY_SIZE(6) + 6*JSON_OBJECT_SIZE(1) + 800;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, payload);

    // Extract prices
  
    // Serial.println("Prices:");

    int i = 0;
    for (JsonVariant value : doc.as<JsonArray>()) {
      prices[i] = value["price"].as<float>();
      symbols[i] = value["symbol"].as<const char*>();
      // Serial.println(symbols[i]+": "+ prices[i]);
      i++;
    }
      
    // Print prices and symbols
    char buffer[6];
    Serial.print(symbols[0]+": ");    //BTC
    dtostrf(prices[0], 4, 2, buffer);
    Serial.println(buffer);

    // Serial.print(symbols[1]+": ");    //M/B
    // dtostrf(prices[1], 6, 6, buffer);
    // Serial.println(buffer);
    
    // Serial.print(symbols[2]+": ");    //MLN
    // dtostrf(prices[2], 4, 2, buffer);
    // Serial.println(buffer);
    
  } 
  else {
    Serial.println("Error: " + httpResponseCode);
  }
  http.end();

}

void calc_battery_symbol () {
  Ladevorgang_Akkuvoll = 2853;
  Ladevorgang_Akkuleer = 2656;
  Batteriebetrieb_Max = 2475;  //Analogwert wenn Akku ganz voll
  Batteriebetrieb_Min = 1480;  //Analogwert, wenn Display zu zittern beginnt


  Vbattprozent = mapFloat(Vbatt, Batteriebetrieb_Min, Batteriebetrieb_Max, 0, 100);
  Ladeprozente = mapFloat(Vbatt, Ladevorgang_Akkuleer, Ladevorgang_Akkuvoll, 0, 100);
  Serial.println("Vbatt%: "+ String(Vbatt,0) + " " + String(Vbattprozent,0));
  
  // if (Vbattprozent > 100) { Vbattprozent = 0; } // activate for position testing of indicator point 

  // Spannungindikator
  if (Vbatt>=2550)  {    // USB Ladung erkannt da Vbatt > 2500
      Vbattindikatorposition = (Vbattindikatorposition >= 20) ? 0 : Vbattindikatorposition + 4;  //Scrolle um 2 pixel weiter bis 20, dann wieder 0
      Spannungsfarbe=TFT_YELLOW; Vbattprozent = Ladeprozente; 
            }
  else if ((Vbattprozent<=100) && (Vbattprozent>=90)) { Vbattindikatorposition=18; //10-50% grün
      Spannungsfarbe=TFT_GREEN;}
  else if ((Vbattprozent  <90) && (Vbattprozent>=80)) { Vbattindikatorposition=16;  //10-50% grün
      Spannungsfarbe=TFT_GREEN;}
  else if ((Vbattprozent  <80) && (Vbattprozent>=70)) { Vbattindikatorposition=14; //10-50% grün
      Spannungsfarbe=TFT_GREEN;}
  else if ((Vbattprozent  <70) && (Vbattprozent>=60)) { Vbattindikatorposition=12; //10-50% grün
      Spannungsfarbe=TFT_GREEN;}
  else if ((Vbattprozent  <60) && (Vbattprozent>=50)) { Vbattindikatorposition=10; //10-50% grün
      Spannungsfarbe=TFT_GREEN;}
  else if ((Vbattprozent  <50) && (Vbattprozent>=30)) { Vbattindikatorposition=5;  //30-49% orange
      Spannungsfarbe=TFT_ORANGE;}
  else if (Vbattprozent<30)                           { Vbattindikatorposition=0;  //<30 leer
      Spannungsfarbe=TFT_RED;    }
  
  if (Vbattprozent > 100) { Vbattprozent = 100; }

   // Batteriezeichen----------------
  Pixeloffset=-5;
  sprite.drawString(String(Vbatt,0) + " "  + String(Vbattprozent,0) + "% ",210,(Zeilenabstand*0),2);
  // sprite.drawString(String(Vbattprozent,0) + "%  ",245,(Zeilenabstand*0),2);
  sprite.drawCircle(10+280,18+Pixeloffset*2,7,TFT_WHITE);
  sprite.drawCircle(10+300,18+Pixeloffset*2,7,TFT_WHITE);
  sprite.fillCircle(10+280,18+Pixeloffset*2,6,TFT_BLACK);
  sprite.fillCircle(10+300,18+Pixeloffset*2,6,TFT_BLACK);
  sprite.drawLine(10+278,11+Pixeloffset*2,10+302,11+Pixeloffset*2,TFT_WHITE);
  sprite.drawLine(10+278,25+Pixeloffset*2,10+302,25+Pixeloffset*2,TFT_WHITE);
  sprite.fillRect(10+278,12+Pixeloffset*2,24,13,TFT_BLACK);
  sprite.fillCircle(10+280+Vbattindikatorposition,18+Pixeloffset*2,4,Spannungsfarbe); 
  sprite.pushSprite(0,0);   
}


void setup() {
  
  Serial.begin(115200);
  delay(10);
  
  wifiMulti.addAP("x, "x");  
  wifiMulti.addAP("x", "x");
  wifiMulti.addAP("x-x", "x");
  
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH); // Akku an
  pinMode(PIN_TOUCH_RES, OUTPUT);
  digitalWrite(PIN_TOUCH_RES, LOW);
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH);
  delay(500);
  digitalWrite(PIN_TOUCH_RES, HIGH);
  pinMode(PIN_BUTTON_1,INPUT_PULLUP);
  pinMode(PIN_BUTTON_2,INPUT_PULLUP);

  tft.begin();
  tft.setRotation(3);
  sprite.createSprite(320,170);
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);

  if(wifiMulti.run() == WL_CONNECTED) {
      }
  Serial.print("rebooted...");

  timer1.start();
  timer2.start();
  timer3.start();
  timer4.start();
  timer5.start();
  timer6.start(); //Display_Brightness

  readout_battery ();
  calc_battery_symbol ();
}


void loop() {
  
  // ReadSonoff ();
  
  timer1.update();  //  1 getPricesKlines ();
  timer2.update();  //  2 getBinancePrice ();
  timer3.update();  //  3 calculatemedian ();
  timer4.update();  //  4 calc_battery_symbol ();
  timer5.update();  //  5 readout_battery ();
  timer6.update();  //  6Display_Brightness
   
  symbols[0].toLowerCase();
  symbols[1].toLowerCase();
  symbols[2].toLowerCase();

  Pixeloffset=10;

                sprite.drawString(symbols[1]         ,10,(Zeilenabstand*1+Pixeloffset),4);
  sprite.drawString((String(prices[1], 6)   + "$"   ),160,(Zeilenabstand*1+Pixeloffset),4);

                sprite.drawString(symbols[0] + "  "  ,10,(Zeilenabstand*2+Pixeloffset),4);
  sprite.drawString((String(prices[0], 2)   + "$")   ,160,(Zeilenabstand*2+Pixeloffset),4);

                sprite.drawString(symbols[2] + "  "   ,10,(Zeilenabstand*3+Pixeloffset),4);
  sprite.drawString((String(prices[2], 8)            ),160,(Zeilenabstand*3+Pixeloffset),4);

                 sprite.drawString("calculated =" ,10,(Zeilenabstand*4+Pixeloffset),4);
  sprite.drawString((String((prices[1]/prices[0]),10)),160,(Zeilenabstand*4+Pixeloffset),4);
  
                sprite.drawString(String("median =")  ,10,(Zeilenabstand*5+Pixeloffset),4);
  sprite.drawString((String(1.00*median, 8)         ),160,(Zeilenabstand*5+Pixeloffset),4);
  
  if(digitalRead(PIN_BUTTON_1)==0) {
     digitalWrite(PIN_POWER_ON, LOW); // Akku off
     esp_deep_sleep_start();
  }
    
  if(digitalRead(PIN_BUTTON_2)==0) {
     digitalWrite(PIN_POWER_ON, LOW); // Akku off
     esp_deep_sleep_start();
  }
  
  sprite.pushSprite(0,0);   
}
