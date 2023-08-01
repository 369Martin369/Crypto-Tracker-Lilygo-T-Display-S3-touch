#define TOUCH_MODULES_CST_SELF
#include <Arduino.h>
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
#include "mbedtls/md.h"
#include <stdlib.h> // Für qsort-Funktion
#include <string.h> // Für memset-Funktion
// #include <iostream>
// #include <chrono>

WiFiMulti wifiMulti;

int Zeilenabstand = 26; //normal = 20; bei Schriftgröße 4 passen 8 Zeilen auf das Display. Bei 7 Zeilen auf 25 gehen
int Pixeloffset  = +10; //Verschiebung nach oben

// Binance API endpoint and symbols to get prices for
String SYMBOL0 = "BTCEUR";
String SYMBOL1 = "WAVESEUR";
String SYMBOL2 = "MLNUSDT";

const int limit = 30;
String Interval = "1m";
const int period = limit-1;
float klinprices[limit];
float median;
String Servertime;
// int assetTotal;
int countAssets;
int countPrices;

float Kurs_EURUSDT;
float Kurs_BTCEUR; 
float Wallet_EUR;
float Wallet_BTC;
float Wallet_USDT;
float Wallet_Summe_EUR;



float lunc_free;
float lunc_locked;
float btc_free;
float btc_locked;
float busd_free;
float busd_locked;
float bnb_free;
float bnb_locked;
float mln_free;
float mln_locked;

// // Binance API credentials

String binance_key = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
String binance_secret = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

// Größe des JSON-Dokuments anpassen
const size_t JSON_SIZE = 1024;

// Deklaration der params-Variablen
DynamicJsonDocument params(JSON_SIZE);

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

// MAX_VALUES Paare
const int MAX_VALUES = 24;
double prices[MAX_VALUES];
String symbols[MAX_VALUES];
const char* Symbols_list[] = {"BONDBTC",  "BONDUSDT", "QIBTC",    "QIUSDT",   "PEPEUSDT", "FTTUSDT", 
                              "MATICBTC", "MATICEUR", "TROYUSDT", "DOGEUSDT", "SUIBTC",   "JASMYUSDT", 
                              "ETHEUR",   "EURUSDT",  "BTCEUR",   "BNBEUR",   "BNBBTC",   "XRPEUR",
                              "XRPBTC",   "LUNAUSDT", "LUNCUSDT", "MLNBTC",   "MLNUSDT",  "WAVESEUR"};                    // Add currency pairs here


// Declaration für TickTwo
void getPricesKlines ();
void getBinancePrice ();
void Wallet_Info ();
void List_all_Coins ();
void calc_battery_symbol ();
void readout_battery ();
void Display_Brightness ();


TickTwo timer1(getPricesKlines, 30000); // alle 30s Klines laden
TickTwo timer2(getBinancePrice, 1000); // alle 30s Klines laden
TickTwo timer3(Wallet_Info, 2000); // alle 5s Wallet lesen und ausgeben
TickTwo timer4(List_all_Coins, 10000); // alle 10s die Liste aller coins ausdrucken (und in EEPROM abspeichern)
TickTwo timer5(calc_battery_symbol, 3000); // batteriesymbol alle 200ms aufrischen um scrolleffekt zu erreichen
TickTwo timer6(readout_battery, 50000); // alle 5s batterie auslesen und mitteln
TickTwo timer7(Display_Brightness, 15000); // alle 1s Display_Brightness berechnen und PWM setzen


// Display
TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);


// Definition der Coin-Struktur
struct Coin {
  String KursTimestamp;
  String Token;
  float coin_free;
  float locked;
  float CoinCty;
  String WechselpaarEUR;
  String WechselpaarBTC;
  String WechselpaarUSDT;
  float WechselpaarKursEUR;
  int Trend1m;
  int Trend3m;
  int Trend5m;
  int Trend15m;
  int Trend30m;
  int Trend1h;
  int Trend2h;
  int Trend4h;
  int Trend6h;
  int Trend8h;
  int Trend12h;
  int Trend1D;
  int Trend3D;
  int Trend1w;
  int Trend1M;
  float WechselpaarKursBTC;
  float WechselpaarKursUSDT;
  float WertEUR;
  float WertBTC;
  float WertUSDT;
};

// Enum für das Feld, nach dem sortiert werden soll
enum SortField {
  KursTimestamp,
  Token,
  coin_free, 
  locked,
  CoinCty,
  WechselpaarEUR,
  WechselpaarBTC,
  WechselpaarUSDT,
  WechselpaarKursEUR,
  Trend1m,
  Trend3m,
  Trend5m,
  Trend15m,
  Trend30m,
  Trend1h,
  Trend2h,
  Trend4h,
  Trend6h,
  Trend8h,
  Trend12h,
  Trend1D,
  Trend3D,
  Trend1w,
  Trend1M,
  WechselpaarKursBTC,
  WechselpaarKursUSDT,
  WertEUR,
  WertBTC,
  WertUSDT
};

// Globale Variable, um das aktuelle Sortierfeld zu speichern
SortField currentSortField = WertEUR;


// Erstellen des Arrays von 15 Coins in der Spot Wallet
Coin coins[15];

void Display_Brightness() {
  int Helligkeit=mapFloat(Vbatt, 1480, 2500, 100, 250);
  if (Helligkeit >255) {Helligkeit=250;}
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

  String url = "https://api.binance.com/api/v3/klines?symbol=";
  url += SYMBOL0 + "&interval=" +  Interval + "&limit=" + String(limit);
  
  HTTPClient http;
  http.begin(url);
  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
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
  calculatemedian ();  
}

 // comparison function used by qsort
int cmpfunc(const void* a, const void* b) {
  return (*(int*)a - *(int*)b);
}
 
// ---------Median berechnen ---------------
void calculatemedian() {
  float array[limit]; //Initialisieren Sie das Array mit Klines Größe
  int n = sizeof(array) / sizeof(array[0]); //Berechnen Sie die Anzahl der Elemente im Array
  for (int i = 0; i < n; i++) {   //Füllen Sie das Array mit Klines Werten
    array[i] = klinprices[i];
    // Serial.println("array:" + String(i) +"="+ String(array[i]));
  }
  qsort(array, n, sizeof(int), cmpfunc);//Sortieren Sie das Array in aufsteigender Reihenfolge
  if (n % 2 == 0) {    //f (n % 2 == 0) prüft, ob die Variable n durch 2 ohne Rest teilbar ist. Wenn der Rest der Division gleich Null ist, bedeutet dies, dass n eine gerade Zahl ist.
    median = (array[n/2-1] + array[n/2]) / 2; //Da das Array bereits sortiert ist, ist das Element an der Indexposition n/2 das Median-Element
  }
  else {
    median = array[n/2]; //Da das Array bereits sortiert ist, ist das Element an der Indexposition n/2 das Median-Elemen
  }
  Serial.println("Medianwert: " + String(median,8));
}

void getBinancePrice () {
  Serial.println("--------Event loading Binance Prices------");
  
  HTTPClient http;
  int i=0;
  String url = "https://api.binance.com/api/v3/ticker/price?symbols=[";
    
  for (uint8_t i = 0; i < MAX_VALUES; ++i) {
    if (i > 0) {
      url += ",";
    }
    url += "%22" + String(Symbols_list[i]) + "%22";
  }
  url += "]";
  // Serial.println(url);

  http.begin(url);
  int httpResponseCode = http.GET();

  if (httpResponseCode == HTTP_CODE_OK) {
    String payload = http.getString();
    // Serial.println("Response:" + payload);

    // const size_t capacity = (MAX_VALUES * JSON_OBJECT_SIZE(1)) + 100;
    const size_t capacity = 2048;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc, payload);

    int i = 0;
    for (JsonVariant value : doc.as<JsonArray>()) {
      prices[i] = value["price"].as<float>();
      symbols[i] = value["symbol"].as<const char*>();
      Serial.print(String(i) + " " + symbols[i]); Serial.printf(": %.8f\n", prices[i]);
      i++; 
    }
  } 
  else {Serial.println("Error: " + httpResponseCode);}
  http.end();
}

String hmacSha256(const String& key, const String& data) {
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  const size_t payloadLength = data.length();
  const size_t keyLength = key.length();
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, reinterpret_cast<const unsigned char*>(key.c_str()), keyLength);
  mbedtls_md_hmac_update(&ctx, reinterpret_cast<const unsigned char*>(data.c_str()), payloadLength);
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);
  String hash;
  for(int i= 0; i< sizeof(hmacResult); i++){
      char str[3];
      sprintf(str, "%02x", (int)hmacResult[i]);
      hash += str;
  }
  // Serial.println("Hash: " + hash);
  return hash;
}


void Binance_Time () {
  // Startzeitpunkt erfassen
    // auto start = std::chrono::high_resolution_clock::now();

  HTTPClient http;
  String url = "https://api.binance.com/api/v3/time";
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    Servertime = response.substring(14, response.length() - 1); //schneidet text von Position 14 weg und bis -1 aus
    // Serial.print("Binance-Servertime:");
    // Serial.println(Servertime);
  }
  // Endzeitpunkt erfassen
    // auto end = std::chrono::high_resolution_clock::now();

    // Berechnung der Durchlaufzeit in Mikrosekunden (us)
    // auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // std::cout << "Durchlaufzeit der Funktion: " << duration.count() << " Mikrosekunden" << std::endl;
}

void Wallet_Info () {
  
  // Löschen des Arrays vor jedem Ladevorgang
  memset(coins, 0, sizeof(coins));

  Binance_Time (); // Generate timestamp by loading Servertime
  HTTPClient http;
  
  String query = "timestamp=" + String(Servertime); 
  String signature = hmacSha256(binance_secret, query); // hashing string with secret to generate signature
  // Construct the API endpoint URL
  String url = "https://api.binance.com/api/v3/account";
  url += "?timestamp=";
  url += Servertime;
  url += "&signature=";
  url += signature;
  // Serial.println(url);

  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("X-MBX-APIKEY", binance_key);
  
  int httpCode = http.GET();
  // int httpCode = http.POST(url);

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    DynamicJsonDocument doc(65536);
    DeserializationError error = deserializeJson(doc, response); // das klappt super!!!
    
    if (error) {
     Serial.print("deserializeJson() failed: ");
     Serial.println(error.c_str());
     return;}
    
    // Declare variables outside the loop
    const char* balance_asset;
    const char* balance_free;
    const char* balance_locked; 
    countAssets = 0;
    Wallet_EUR = 0 ;
    Wallet_BTC = 0 ;
    Wallet_USDT = 0 ; 
    
    Serial.println("<--Wallet Info:----------------------------------------------");
    bool firstlinetrue=true;
    for (JsonObject balance : doc["balances"].as<JsonArray>()) {
      String balance_asset = balance["asset"]; // "BTC", "LTC", "ETH", "NEO", "BNB", "QTUM", "EOS", ...
      float balance_free = atof(balance["free"]); // "0.00000623", "0.00000000", "0.00000000", "0.00000000", ...
      float balance_locked = atof(balance["locked"]); // "0.00000000", "0.00000000", "0.00000000", ...
      float assetTotal = balance_free + balance_locked;
      if (assetTotal > 0.00001) {   // Calculate total assets if greater than 0
      
        // Fülle Coin Struct von Wallet
        coins[countAssets].Token=balance_asset;
        coins[countAssets].coin_free=balance_free;
        coins[countAssets].locked=balance_locked;
        coins[countAssets].CoinCty=balance_free+balance_locked;
        coins[countAssets].KursTimestamp=Servertime;
         
        for (int i = 0; i < MAX_VALUES; i++) {
          if (strcmp(coins[countAssets].Token.c_str(),  "BTC") == 0 && strstr(symbols[i].c_str(),  "BTCEUR") != NULL) {coins[countAssets].WechselpaarKursEUR =     prices[i]; coins[countAssets].WechselpaarEUR =symbols[i];}
          if (strcmp(coins[countAssets].Token.c_str(),  "BNB") == 0 && strstr(symbols[i].c_str(),  "BNBEUR") != NULL) {coins[countAssets].WechselpaarKursEUR =     prices[i]; coins[countAssets].WechselpaarEUR =symbols[i];}
          if (strcmp(coins[countAssets].Token.c_str(), "USDT") == 0 && strstr(symbols[i].c_str(), "EURUSDT") != NULL) {coins[countAssets].WechselpaarKursEUR = 1 / prices[i]; coins[countAssets].WechselpaarEUR =symbols[i];}
          if (strcmp(coins[countAssets].Token.c_str(),  "XRP") == 0 && strstr(symbols[i].c_str(),  "XRPEUR") != NULL) {coins[countAssets].WechselpaarKursEUR =     prices[i]; coins[countAssets].WechselpaarEUR =symbols[i];}
          if (strcmp(coins[countAssets].Token.c_str(),"WAVES") == 0 && strstr(symbols[i].c_str(),"WAVESEUR") != NULL) {coins[countAssets].WechselpaarKursEUR =     prices[i]; coins[countAssets].WechselpaarEUR =symbols[i];}
          if (strcmp(coins[countAssets].Token.c_str(), "LUNA") == 0 && strstr(symbols[i].c_str(),"LUNAUSDT") != NULL) {coins[countAssets].WechselpaarKursUSDT =    prices[i]; coins[countAssets].WechselpaarUSDT=symbols[i];}
          if (strcmp(coins[countAssets].Token.c_str(), "LUNC") == 0 && strstr(symbols[i].c_str(),"LUNCUSDT") != NULL) {coins[countAssets].WechselpaarKursUSDT =    prices[i]; coins[countAssets].WechselpaarUSDT=symbols[i];}
          if (strcmp(coins[countAssets].Token.c_str(), "PEPE") == 0 && strstr(symbols[i].c_str(),"PEPEUSDT") != NULL) {coins[countAssets].WechselpaarKursUSDT =    prices[i]; coins[countAssets].WechselpaarUSDT=symbols[i];}
          if (strcmp(coins[countAssets].Token.c_str(),  "MLN") == 0 && strstr(symbols[i].c_str(),  "MLNBTC") != NULL) {coins[countAssets].WechselpaarKursBTC =     prices[i]; coins[countAssets].WechselpaarBTC =symbols[i];} // Serial.print("BTC Abfrage "); Serial.print(i); Serial.printf(": %.8f\n", coins[countAssets].WechselpaarKursEUR);  
        }
        
        if (coins[countAssets].WechselpaarEUR =="EURUSDT") {Kurs_EURUSDT = coins[countAssets].WechselpaarKursEUR;}
        if (coins[countAssets].WechselpaarEUR =="BTCEUR")  {Kurs_BTCEUR  = coins[countAssets].WechselpaarKursEUR;}

        if (!coins[countAssets].WechselpaarEUR.isEmpty()) {coins[countAssets].WertEUR = coins[countAssets].CoinCty *0.996 * coins[countAssets].WechselpaarKursEUR; Wallet_EUR +=  coins[countAssets].WertEUR;}
        if (!coins[countAssets].WechselpaarBTC.isEmpty()) {coins[countAssets].WertBTC = coins[countAssets].CoinCty *0.996 * coins[countAssets].WechselpaarKursBTC; Wallet_BTC +=  coins[countAssets].WertBTC; coins[countAssets].WertEUR = (coins[countAssets].WertBTC*Kurs_BTCEUR*0.996);}
        if (!coins[countAssets].WechselpaarUSDT.isEmpty()){coins[countAssets].WertUSDT = coins[countAssets].CoinCty*0.996 * coins[countAssets].WechselpaarKursUSDT;Wallet_USDT += coins[countAssets].WertUSDT;coins[countAssets].WertEUR = (coins[countAssets].WertUSDT*Kurs_EURUSDT*0.999);}

        firstlinetrue = false;
        countAssets ++;
      }
    }
  } else {
    Serial.printf(" HTTP request failed with error code %d\n", httpCode);
    Serial.println(http.getString());
    }
  http.end();

  // Sort by coins[countAssets].WertEUR
  qsort(coins, countAssets, sizeof(Coin), compareCoinsByWertEUR);
  
  // Zeige die sortierten Coins an oder führe andere Aktionen aus...
  Serial.printf("%-10s %-6s %-8s %-12s %-10s %-10s\n", "Timestamp", "Coin", "Menge", "Handelspaar", "Preis", "Wert[EUR]"); //Überschrift
  for (int i = 0; i < countAssets; i++) {
    if (!coins[i].WechselpaarEUR.isEmpty())  {Serial.printf ("%-10lu %-6s %-8.3f %-12s %-10.3f %-10.2f\n", coins[i].KursTimestamp, coins[i].Token, coins[i].CoinCty, coins[i].WechselpaarEUR,  coins[i].WechselpaarKursEUR ,  coins[i].WertEUR);}
    if (!coins[i].WechselpaarBTC.isEmpty())  {Serial.printf ("%-10lu %-6s %-8.3f %-12s %-10.6f %-10.2f\n", coins[i].KursTimestamp, coins[i].Token, coins[i].CoinCty, coins[i].WechselpaarBTC,  coins[i].WechselpaarKursBTC ,  coins[i].WertEUR);}
    if (!coins[i].WechselpaarUSDT.isEmpty()) {Serial.printf ("%-10lu %-6s %-8.3f %-12s %-10.3f %-10.2f\n", coins[i].KursTimestamp, coins[i].Token, coins[i].CoinCty, coins[i].WechselpaarUSDT, coins[i].WechselpaarKursUSDT , coins[i].WertEUR);}
  }
  
  Serial.print("Revante Coins mit Geldmenge: "); Serial.println(countAssets);
  Serial.print("Wert der Wallet in ***** ");
  Wallet_Summe_EUR = Wallet_EUR + Wallet_BTC * Kurs_BTCEUR + Wallet_USDT * Kurs_EURUSDT;
  Serial.printf("EUR = %.2f", Wallet_Summe_EUR); Serial.println(" *****");
  Serial.println("-------------------------------------------------Wallet End->");

}

// Vergleichsfunktion für qsort
int compareCoinsByWertEUR(const void* a, const void* b) {
  Coin* coinA = (Coin*)b;     // absteigend
  Coin* coinB = (Coin*)a;
  //Coin* coinA = (Coin*)a;    aufsteigend
  //Coin* coinB = (Coin*)b;

  // Hier können Sie je nach Sortierfeld die Vergleiche anpassen
  switch (currentSortField) {
    case KursTimestamp: return coinA->KursTimestamp.compareTo(coinB->KursTimestamp);
    case Token: return coinA->Token.compareTo(coinB->Token);
    case coin_free: return coinA->coin_free - coinB->coin_free;
    case locked: return coinA->locked - coinB->locked;
    case CoinCty: return coinA->CoinCty - coinB->CoinCty;
    case WechselpaarEUR: return coinA->WechselpaarEUR.compareTo(coinB->WechselpaarEUR);
    case WechselpaarBTC: return coinA->WechselpaarBTC.compareTo(coinB->WechselpaarBTC);
    case WechselpaarUSDT: return coinA->WechselpaarUSDT.compareTo(coinB->WechselpaarUSDT);
    case Trend1m: return coinA->Trend1m - coinB->Trend1m;
    case Trend3m: return coinA->Trend3m - coinB->Trend3m;
    case Trend5m: return coinA->Trend5m - coinB->Trend5m;
    case Trend15m: return coinA->Trend15m - coinB->Trend15m;
    case Trend30m: return coinA->Trend30m - coinB->Trend30m;
    case Trend1h: return coinA->Trend1h - coinB->Trend1h;
    case Trend2h: return coinA->Trend2h - coinB->Trend2h;
    case Trend4h: return coinA->Trend4h - coinB->Trend4h;
    case Trend6h: return coinA->Trend6h - coinB->Trend6h;
    case Trend8h: return coinA->Trend8h - coinB->Trend8h;
    case Trend12h: return coinA->Trend12h - coinB->Trend12h;
    case Trend1D: return coinA->Trend1D - coinB->Trend1D;
    case Trend3D: return coinA->Trend3D - coinB->Trend3D;
    case Trend1w: return coinA->Trend1w - coinB->Trend1w;
    case Trend1M: return coinA->Trend1M - coinB->Trend1M;
    case WechselpaarKursEUR: return coinA->WechselpaarKursEUR - coinB->WechselpaarKursEUR;
    case WechselpaarKursBTC: return coinA->WechselpaarKursBTC - coinB->WechselpaarKursBTC;
    case WechselpaarKursUSDT: return coinA->WechselpaarKursUSDT - coinB->WechselpaarKursUSDT;
    case WertEUR: return coinA->WertEUR - coinB->WertEUR;
    case WertBTC: return coinA->WertBTC - coinB->WertBTC;
    case WertUSDT: return coinA->WertUSDT - coinB->WertUSDT;                          
    // Fügen Sie hier weitere Fälle hinzu für die restlichen Felder
    // ...
    default: return 0; // Wenn kein Sortierfeld gefunden wird, werden die Elemente als gleich betrachtet
  }
}


void List_all_Coins () {
  Serial.print("<-Event Save all Coints to EEPROM:----------------------------");
  // for (int i = 0; i < countAssets; i++) {
  //   Serial.print((String(coins[i].KursTimestamp)) + ": ");
  //   Serial.print((String(i) + " : "));
  //   Serial.print((String(coins[i].Token))+ " Menge=");
  //   Serial.print((String(coins[i].CoinCty))+ " Wert=");
  //   Serial.println((String(coins[i].WertEUR))+ " EUR");
  // }
  Serial.println("-------------------------------------------------End->");
}


void calc_battery_symbol () {
  Serial.print("<- Calc Battery symbol: --------- ");
  Ladevorgang_Akkuvoll = 2853;
  Ladevorgang_Akkuleer = 2656;
  Batteriebetrieb_Max = 2475;  //Analogwert wenn Akku ganz voll
  Batteriebetrieb_Min = 1480;  //Analogwert, wenn Display zu zittern beginnt


  Vbattprozent = mapFloat(Vbatt, Batteriebetrieb_Min, Batteriebetrieb_Max, 0, 100);
  Ladeprozente = mapFloat(Vbatt, Ladevorgang_Akkuleer, Ladevorgang_Akkuvoll, 0, 100);
  Serial.print("Vbatt%: "+ String(Vbatt,0) + " " + String(Vbattprozent,0));
  
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
  Serial.print("----------------End Calc Battery symbol->"); Serial.println() ;
}


void setup() {
  
  Serial.begin(500000);
  delay(10);
  Serial.println("rebooted...");
  
  wifiMulti.addAP("SSID", "passwort"); // wifi1
  wifiMulti.addAP("SSID", "passwort"); // wifi2
  wifiMulti.addAP("SSID", "passwort"); // wifi3
  wifiMulti.addAP("SSID", "passwort"); // wifi4
  wifiMulti.addAP("SSID", "passwort"); // wifi5

  
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH); // Akku an
  pinMode(PIN_TOUCH_RES, OUTPUT);
  digitalWrite(PIN_TOUCH_RES, LOW);
  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, 250);
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


  
  timer1.start();  // (getPricesKlines, 30000); // alle 30s Klines laden
  timer2.start();  // (getBinancePrice; 1000); // alle 30s Klines laden
  timer3.start();  // (Wallet_Info, 5000); // alle 5s Wallet lesen und ausgeben
  timer4.start();  // (List_all_Coins, 10000); // alle 10s die Liste aller coins ausdrucken (und in EEPROM abspeichern)
  timer5.start();  //  4 calc_battery_symbol ();
  timer6.start();  //  5 readout_battery voltage
  timer7.start();  //  6 Display_Brightness
  

  readout_battery ();
  calc_battery_symbol ();
  getBinancePrice ();
  Wallet_Info ();
}


void loop() {

  
  timer1.update();  // (getPricesKlines, 30000); // alle 30s Klines laden
  timer2.update();  // (getBinancePrice; 1000); // alle 30s Klines laden
  timer3.update();  // (Wallet_Info, 5000); // alle 5s Wallet lesen und ausgeben
  timer4.update();  // (List_all_Coins, 10000); // alle 10s die Liste aller coins ausdrucken (und in EEPROM abspeichern)
  timer5.update();  //  4 calc_battery_symbol ();
  timer6.update();  //  5 readout_battery ();
  timer7.update();  //  6 Display_Brightness
  
  
  Pixeloffset=10;

// Display the 3 biggest coins in Wallet
  int Zeile=0;
  for (int i = 0; i < 3; i++) {
    if (i==0) {Zeile=1;} if (i==1) {Zeile=3;} if (i==2) {Zeile=4;}    
    if (!coins[i].WechselpaarEUR.isEmpty()) {sprite.drawString(coins[i].WechselpaarEUR + "  " , 5, (Zeilenabstand * (Zeile) + Pixeloffset), 4);sprite.drawString(String(coins[i].WechselpaarKursEUR,4) + " " + coins[i].WechselpaarEUR.substring(coins[i].WechselpaarEUR.length()   - 3), 160, (Zeilenabstand * (Zeile) + Pixeloffset), 4);}
    if (!coins[i].WechselpaarBTC.isEmpty()) {sprite.drawString(coins[i].WechselpaarBTC + "  " , 5, (Zeilenabstand * (Zeile) + Pixeloffset), 4);sprite.drawString(String(coins[i].WechselpaarKursBTC,6) + " " + coins[i].WechselpaarBTC.substring(coins[i].WechselpaarBTC.length()   - 3), 160, (Zeilenabstand * (Zeile) + Pixeloffset), 4);}
    if (!coins[i].WechselpaarUSDT.isEmpty()){sprite.drawString(coins[i].WechselpaarUSDT + "  ", 5, (Zeilenabstand * (Zeile) + Pixeloffset), 4);sprite.drawString(String(coins[i].WechselpaarKursUSDT,4)+ " " + coins[i].WechselpaarUSDT.substring(coins[i].WechselpaarUSDT.length() - 4), 160, (Zeilenabstand * (Zeile) + Pixeloffset), 4);}
  }

  sprite.drawString(String("1m MA(21):")  ,5 , (Zeilenabstand *2 +Pixeloffset),4); sprite.drawString((String(median, 2) + " "),160, (Zeilenabstand*2+Pixeloffset),4);
  sprite.drawString("Wallet total: "      ,5 , (Zeilenabstand *5 +Pixeloffset),4); sprite.drawString((String(Wallet_Summe_EUR, 3) + " EUR"),                                           160,(Zeilenabstand*5+Pixeloffset),4);
  
  
  if (digitalRead(PIN_BUTTON_1) == LOW || digitalRead(PIN_BUTTON_2) == LOW) {
     digitalWrite(PIN_POWER_ON, LOW); // Akku off
     esp_deep_sleep_start();
  }
   
  
  sprite.pushSprite(0,0);   
}
