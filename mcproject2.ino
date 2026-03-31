#include <SPI.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Keypad.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "rm_qrcode.h" 
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <time.h> 

// ---------- WIFI & GOOGLE SHEETS ----------
const char* ssid = "ESP32TEST";
const char* password = "12345678";
String scriptURL = "https://script.google.com/macros/s/AKfycbzWCcNN7kTd_g-y0jFRtkqgxxed5XF2pavyk01S2rVefyVj5DcTbhyb7D1yg1D08u3q/exec";

// ---------- ADAFRUIT IO MQTT ----------
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   
#define AIO_USERNAME    "mortalzenith"       
#define AIO_KEY         "aio_lhot38l5ivwVZY5ThxkjkehWTert"  

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe paymentFeed = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/payment-status");

// ---------- HARDWARE PINS ----------
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'} 
};
byte rowPins[ROWS] = {32, 33, 25, 26};
byte colPins[COLS] = {27, 14, 12, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ---------- SYSTEM VARIABLES ----------
// Added SHOW_RECEIPT state
enum State { ENTER_ITEM, ENTER_QTY, FETCHING, PROMPT_NEXT, WAIT_PAYMENT, SHOW_RECEIPT };
State currentState = ENTER_ITEM;

String inputBuffer = "";
String currentItem = "";
int currentQty = 0;
int totalAmount = 0;
String itemsDatabase = ""; // Fast RAM Cache

// Added variables for the E-Bill
int currentToken = 1; 
String allItems = ""; // Stores all items added in current transaction

// Function Declarations
void updateScreen();
void showQR();
void displayReceiptQR(String orderID, String items, String totalAmount);
void resetSystem();
void fetchPriceLocal();
void logTransactionLocal(String txnId); // Updated to accept ID
void syncTransactionsToCloud();
void downloadItemsDB();
void MQTT_connect();

// ----------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. Initialize Screen
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);

  // 2. Initialize Internal Memory (LittleFS)
  tft.setCursor(10, 20);
  tft.print("Mounting Memory...");
  if (!LittleFS.begin(true)) { 
    tft.setTextColor(ILI9341_RED);
    tft.println(" FAILED!");
    while(1);
  }
  tft.setTextColor(ILI9341_GREEN);
  tft.println(" SUCCESS!");
  delay(500);

  // 3. Connect WiFi
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(10, 50);
  tft.print("Connecting WiFi...");
  WiFi.begin(ssid, password);
  
  int wifi_attempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_attempts < 20) { 
    delay(500); 
    Serial.print("."); 
    wifi_attempts++;
  }
  
  // 4. Set Time
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");

  // 5. Download DB & Cache to RAM
  if(WiFi.status() == WL_CONNECTED) {
    tft.fillScreen(ILI9341_BLACK);
    tft.setCursor(10, 40);
    tft.setTextColor(ILI9341_CYAN);
    tft.println("Syncing latest prices...");
    downloadItemsDB();
  } else {
    // If WiFi fails, load the last known prices from memory
    File file = LittleFS.open("/items.csv", FILE_READ);
    if(file) {
      itemsDatabase = file.readString();
      file.close();
    }
  }

  // 6. Connect MQTT
  mqtt.subscribe(&paymentFeed);
  updateScreen();
}

void loop() {
  MQTT_connect();

  // Listen for Payment Confirmation
  // Listen for Payment Confirmation
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(10))) { 
    if (subscription == &paymentFeed) {
      String msg = (char *)paymentFeed.lastread;
      
      // THE FIX: Clean the incoming message of any invisible spaces or newlines
      msg.trim(); 
      
      // Add a debug print so you can see exactly what the ESP32 hears
      Serial.println("MQTT Received: [" + msg + "]"); 

      if (currentState == WAIT_PAYMENT && msg == "PAID") {
        
        // Generate a single Transaction ID for both logging and the receipt
        String txnId = "TXN" + String(millis());
        
        logTransactionLocal(txnId); // Securely save receipt to internal flash

        tft.fillScreen(ILI9341_GREEN);
        tft.setTextColor(ILI9341_BLACK);
        tft.setTextSize(3);
        tft.setCursor(50, 100);
        tft.print("PAYMENT");
        tft.setCursor(50, 140);
        tft.print("SUCCESSFUL!");
        
        delay(2500); // Show success for 2.5 seconds
        
        // Show the E-Bill QR Code
        displayReceiptQR(txnId, allItems, String(totalAmount));
        currentState = SHOW_RECEIPT; 
      }
    }
  }

  // Keypad Input
  char key = keypad.getKey();
  if (key) {
    if (key == 'C') { resetSystem(); return; }

    // THE NEW SYNC BUTTON TRIGGER (Only works when idle)
    if (key == 'D' && currentState == ENTER_ITEM) {
      syncTransactionsToCloud();
      return;
    }

    switch (currentState) {
      case ENTER_ITEM:
        if (key >= '0' && key <= '9') { inputBuffer += key; updateScreen(); } 
        else if (key == 'A' && inputBuffer.length() > 0) {
          currentItem = inputBuffer; inputBuffer = ""; currentState = ENTER_QTY; updateScreen();
        }
        break;

      case ENTER_QTY:
        if (key >= '0' && key <= '9') { inputBuffer += key; updateScreen(); } 
        else if (key == 'A' && inputBuffer.length() > 0) {
          currentQty = inputBuffer.toInt(); inputBuffer = ""; currentState = FETCHING;
          updateScreen();
          fetchPriceLocal(); // Instantly checks the RAM cache
        }
        break;

      case PROMPT_NEXT:
        if (key == 'A') { currentState = ENTER_ITEM; updateScreen(); } 
        else if (key == 'B') {
          if (totalAmount > 0) { currentState = WAIT_PAYMENT; showQR(); }
        }
        break;
        
      case SHOW_RECEIPT:
        // Any key pressed while the receipt is showing will clear the screen for the next customer
        resetSystem();
        break;

      default: break;
    }
  }
}

// ----------------------------------------
// E-BILL QR GENERATION FUNCTION
// ----------------------------------------
void displayReceiptQR(String orderID, String items, String totalAmt) {
    // 1. Build the Data-Rich URL
    items.replace(" ", "_"); 
    
    // UPDATE THIS LINK TO YOUR ACTUAL RENDER DOMAIN
    String baseURL = "https://canteen-pay.onrender.com/receipt.html?";
    
    // Combine everything into one long link
    String finalUrl = baseURL + "order=" + orderID + "&items=" + items + "&total=" + totalAmt + "&token=" + String(currentToken);

    // 2. Initialize the QR Code engine using the existing rm_qrcode logic
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(6)]; 
    qrcode_initText(&qrcode, qrcodeData, 6, 0, finalUrl.c_str());

    // 3. Draw to TFT Screen (Adapted to ILI9341 library standards)
    tft.fillScreen(ILI9341_WHITE); 
    
    int scale = 3; 
    int offsetX = (tft.width() - (qrcode.size * scale)) / 2;
    int offsetY = (tft.height() - (qrcode.size * scale)) / 2 - 10; // Shifted slightly up to fit text

    // Draw the black squares
    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                tft.fillRect(offsetX + (x * scale), offsetY + (y * scale), scale, scale, ILI9341_BLACK);
            }
        }
    }

    // 4. Add helpful text below the QR code
    tft.setTextColor(ILI9341_BLACK, ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(45, tft.height() - 30); 
    tft.println("Scan for E-Bill");
    
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.print("Token: "); tft.print(currentToken);

    // Increment the token for the next customer!
    currentToken++; 
}


// ----------------------------------------
// INTERNAL FLASH (LITTLEFS) & CLOUD LOGIC
// ----------------------------------------

void downloadItemsDB() {
  WiFiClientSecure clientSecure;
  clientSecure.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
  http.setTimeout(15000); // Give Google 15 seconds to respond

  http.begin(clientSecure, scriptURL + "?action=downloadItems");
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    
    Serial.println("\n--- WHAT GOOGLE SENT TO ESP32 ---");
    Serial.println(payload); 
    Serial.println("---------------------------------\n");

    itemsDatabase = payload; 
    
    File file = LittleFS.open("/items.csv", FILE_WRITE); 
    if (file) {
      file.print(payload);
      file.close();
      Serial.println("SUCCESS: Items DB written to Internal Memory.");
    }
  } else {
    Serial.println("Download Failed. HTTP Code: " + String(httpCode));
  }
  http.end();
}

void logTransactionLocal(String txnId) {
  File file = LittleFS.open("/transactions.csv", FILE_APPEND); 
  if (file) {
    struct tm timeinfo;
    char timeStr[30];
    if(!getLocalTime(&timeinfo)){
      strcpy(timeStr, "Unknown Time");
    } else {
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    }
    
    file.printf("%s,%s,%s,%d,%d\n", txnId.c_str(), timeStr, currentItem.c_str(), currentQty, totalAmount);
    file.close();
    Serial.println("Saved transaction to Internal Memory.");
  }
}

// ==========================================
// MANUAL SYNC FUNCTION (Optimized for Speed)
// ==========================================
void syncTransactionsToCloud() {
  if (!LittleFS.exists("/transactions.csv")) {
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(10, 100);
    tft.println("No offline records");
    tft.setCursor(10, 130);
    tft.println("to sync.");
    delay(2000);
    updateScreen();
    return; 
  }

  tft.fillScreen(ILI9341_BLUE);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 100);
  tft.println("Syncing Data...");
  tft.setCursor(10, 140);
  tft.println("Please Wait");

  File file = LittleFS.open("/transactions.csv", FILE_READ);
  if (!file) return;
  String bulkData = file.readString(); 
  file.close();

  WiFiClientSecure clientSecure;
  clientSecure.setInsecure();
  HTTPClient http;
  
  // THE FIX: Do not waste time following the redirect! 
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS); 
  http.setTimeout(15000); 

  http.begin(clientSecure, scriptURL + "?action=uploadTransactions");
  http.addHeader("Content-Type", "text/plain");
  
  int httpCode = http.POST(bulkData);

  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(10, 100);

  // If Google returns 200 (OK), 302 (Redirect), or 303, the data was saved!
  if (httpCode == 200 || httpCode == 302 || httpCode == 303) {
    LittleFS.remove("/transactions.csv"); 
    tft.setTextColor(ILI9341_GREEN);
    tft.println("Sync Complete!");
    tft.setCursor(10, 140);
    tft.println("Memory Cleared.");
    Serial.println(">>> SYNC SUCCESSFUL! (Google returned " + String(httpCode) + ")");
  } 
  else if (httpCode > 0) {
    tft.setTextColor(ILI9341_RED);
    tft.println("Sync Failed!");
    tft.setCursor(10, 140);
    tft.print("Server Err: "); tft.println(httpCode);
    Serial.println(">>> SYNC FAILED. Server rejected payload. Code: " + String(httpCode));
  } 
  else {
    tft.setTextColor(ILI9341_RED);
    tft.println("Sync Failed!");
    tft.setCursor(10, 140);
    tft.print("Net Err: "); tft.println(httpCode);
    Serial.println(">>> NETWORK ERROR: " + http.errorToString(httpCode));
  }
  
  http.end();
  delay(3000);
  updateScreen(); 
}

void fetchPriceLocal() {
  bool found = false;
  Serial.println("\n--- SEARCHING RAM CACHE FOR ITEM: [" + currentItem + "] ---");

  int startIndex = 0;
  while (startIndex < itemsDatabase.length()) {
    int endIndex = itemsDatabase.indexOf('\n', startIndex);
    if (endIndex == -1) endIndex = itemsDatabase.length();
    
    String line = itemsDatabase.substring(startIndex, endIndex);
    startIndex = endIndex + 1;
    
    line.trim();
    
    if (line.length() == 0 || line.indexOf("ITEM") >= 0) continue;

    int firstComma = line.indexOf(',');
    if (firstComma == -1) continue; 
    
    int secondComma = line.indexOf(',', firstComma + 1);
    if (secondComma == -1) continue; 

    String csvItemNo = line.substring(0, firstComma);
    String cleanCSV_ID = "";
    for(int i=0; i < csvItemNo.length(); i++) {
      if(isDigit(csvItemNo[i])) cleanCSV_ID += csvItemNo[i];
    }

    if (cleanCSV_ID == currentItem) {
      
      String priceStr = line.substring(secondComma + 1);
      String cleanPrice = "";
      for(int i = 0; i < priceStr.length(); i++) {
        if(isDigit(priceStr[i])) cleanPrice += priceStr[i];
        else if (priceStr[i] == '.') break; 
      }
      
      int price = cleanPrice.toInt();
      totalAmount += (price * currentQty);
      
      // Keep track of all items ordered for the E-Bill
      if(allItems.length() > 0) allItems += ",";
      allItems += currentItem + "-x" + String(currentQty);

      found = true;
      currentState = PROMPT_NEXT;
      
      Serial.println(">>> MATCH FOUND! Price: " + String(price));
      break;
    }
  }

  if (!found) {
    tft.fillRect(0, 150, 320, 50, ILI9341_BLACK);
    tft.setTextColor(ILI9341_RED);
    tft.setCursor(10, 160);
    tft.print("Error: Item " + currentItem + " Not Found");
    delay(2000);
    currentState = ENTER_ITEM; 
  }
  updateScreen();
}

// ----------------------------------------
// MQTT, UI, & QR FUNCTIONS 
// ----------------------------------------
void MQTT_connect() {
  if (mqtt.connected()) return;
  int8_t ret;
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { 
       mqtt.disconnect(); delay(5000); retries--;
       if (retries == 0) return; 
  }
}

void resetSystem() {
  inputBuffer = ""; currentItem = ""; currentQty = 0; totalAmount = 0;
  allItems = ""; // Reset the accumulated items string
  currentState = ENTER_ITEM; updateScreen();
}

void updateScreen() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(180, 10);
  tft.setTextColor(ILI9341_GREEN);
  tft.print("Total:"); tft.print(totalAmount);
  tft.setTextColor(ILI9341_WHITE); tft.setCursor(10, 50);

  if (currentState == ENTER_ITEM) {
    tft.println("Enter Item No:"); tft.setTextSize(3); tft.setCursor(10, 80);
    tft.print(inputBuffer); tft.setTextSize(2); tft.setCursor(10, 180);
    tft.setTextColor(ILI9341_YELLOW); tft.print("A:Next  D:Sync  C:Clr"); 
  } 
  else if (currentState == ENTER_QTY) {
    tft.print("Item: "); tft.println(currentItem); tft.setCursor(10, 80);
    tft.println("Enter Qty:"); tft.setTextSize(3); tft.setCursor(10, 110);
    tft.print(inputBuffer); tft.setTextSize(2); tft.setCursor(10, 200);
    tft.setTextColor(ILI9341_YELLOW); tft.print("Press A to Calculate");
  }
  else if (currentState == PROMPT_NEXT) {
    tft.setTextSize(3); tft.setCursor(10, 80); tft.setTextColor(ILI9341_GREEN);
    tft.print("Rs. "); tft.print(totalAmount); tft.setTextSize(2);
    tft.setTextColor(ILI9341_YELLOW); tft.setCursor(10, 150); tft.println("A: Add another item");
    tft.setCursor(10, 180); tft.println("B: Checkout & Pay"); tft.setCursor(10, 210);
    tft.setTextColor(ILI9341_RED); tft.println("C: Cancel Order");
  }
}

void showQR() {
  tft.fillScreen(ILI9341_WHITE);
  String amtStr = String(totalAmount) + ".00";
  String upi = "upi://pay?cu=INR&mc=7372&mode=19&pa=josephmathewjoy816930.rzp@rxairtel&tn=PaymentToJOSEPHMATHEWJOY&tr=SLZ8ZlmkHmLyhMqrv2&am=" + amtStr;
  QRCode qrcode;
  const uint8_t qrVersion = 8; 
  uint8_t qrcodeData[qrcode_getBufferSize(qrVersion)];
  qrcode_initText(&qrcode, qrcodeData, qrVersion, 0, upi.c_str());
  int size = qrcode.size; int scale = 4; 
  int offsetX = (320 - (size * scale)) / 2; int offsetY = 15; 
  for (uint8_t y = 0; y < size; y++) {
    for (uint8_t x = 0; x < size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) tft.fillRect(offsetX + x * scale, offsetY + y * scale, scale, scale, ILI9341_BLACK);
    }
  }
  tft.setTextColor(ILI9341_BLACK); tft.setTextSize(2);
  tft.setCursor(65, offsetY + (size * scale) + 15);
  tft.print("Scan to Pay Rs."); tft.print(totalAmount);
}