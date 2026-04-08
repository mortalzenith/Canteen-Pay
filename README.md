# Canteen-Pay 🛒💳

A **cashless payment system for canteens** using embedded systems and IoT. Canteen-Pay enables seamless, contactless transactions with digital bill generation via QR codes instead of printed receipts.

---

## ✨ Unique Selling Points (USPs)

- **Digital Bill Generation**: Payment confirmation generates a unique QR code for the bill (PDF) instead of printing physical receipts
- **Offline Menu Management**: Menu data from Google Sheets is automatically synced to ESP32 internal flash memory—works even without internet during transactions
- **Real-Time Transaction Tracking**: Transaction data is uploaded to Google Sheets for centralized record-keeping and analytics
- **Audio Confirmation**: DFmini MP3 module provides audio feedback for successful payments
- **Simple User Interface**: Item codes + quantity input via matrix keypad; intuitive navigation with A/B/C/D options

---

## 📋 Overview

**Canteen-Pay** is a beta-stage embedded IoT project designed for **canteen operators and staff**. It eliminates the need for cash transactions and physical receipts by:

1. Displaying menu items with assigned codes on an SPI TFT display
2. Accepting item selection via a matrix keypad
3. Generating a payment QR code upon order confirmation
4. Capturing payment confirmation from **Razorpay** (via Pipedream webhook → Adafruit IO)
5. Generating a digital bill QR code after successful payment
6. Logging all transactions to Google Sheets for record-keeping

---

## 🔄 Payment Flow

```
┌─────────────────────────────────────────────────────────┐
│ 1. User enters item code + quantity via keypad          │
├─────────────────────────────────────────────────────────┤
│ 2. Choose: Add Item (A) / Clear (C) / Confirm (B)       │
├─────────────────────────────────────────────────────────┤
│ 3. Confirm Order → Generate Payment QR (from Razorpay)  │
├─────────────────────────────────────────────────────────┤
│ 4. User scans QR with UPI app on phone                  │
├─────────────────────────────────────────────────────────┤
│ 5. Razorpay → Pipedream → Adafruit IO webhook           │
├─────────────────────────────────────────────────────────┤
│ 6. ESP32 receives payment confirmation (audio beep)     │
├─────────────────────────────────────────────────────────┤
│ 7. Digital Bill QR generated & displayed                │
├─────────────────────────────────────────────────────────┤
│ 8. Press D on keypad → Upload transaction to GSheets    │
└─────────────────────────────────────────────────────────┘
```

---

## 🛠️ Tech Stack

| Component | Technology |
|-----------|-----------|
| **Microcontroller** | ESP32 DevKit |
| **Firmware** | Arduino/C++ |
| **Backend** | Python |
| **Payment Gateway** | Razorpay |
| **Webhook Integration** | Pipedream → Adafruit IO |
| **Data Sync** | Google Sheets API (via Google Apps Script) |
| **Storage** | Google Sheets + ESP32 Flash Memory |
| **Display** | SPI TFT Display |

---

## 🔧 Hardware Components

| Component | Model/Details | Purpose |
|-----------|--------------|---------|
| **Microcontroller** | ESP32 DevKit | Core processing & WiFi connectivity |
| **Display** | SPI TFT Display | Menu & transaction UI |
| **Audio Module** | DFmini MP3 Player + 0.25W Speaker | Payment confirmation sound |
| **Input Device** | Matrix Keypad | Item code & quantity entry |
| **WiFi** | Built-in ESP32 | Google Sheets sync & webhook communication |

### Block Diagram
```
Matrix Keypad → ESP32 ← WiFi (Menu sync, Webhooks)
    ↓                 ↓
  Order          SPI TFT Display
  Entry          (Menu, Bill QR)
                     ↓
              DFmini MP3 Module
              (Audio Feedback)
                     ↓
                  Speaker
                     
[External Services]
Google Sheets ← ESP32 (transaction upload on D press)
Razorpay → Pipedream → Adafruit IO → ESP32 (payment confirmation)
```

---

## 📥 Installation & Setup

### Prerequisites
- **Arduino IDE** with ESP32 board support installed
- **Python 3.8+** with required packages
- **Google Account** with Google Sheets & Google Apps Script access
- **Razorpay Account** with webhook setup
- **Pipedream Account** for webhook routing
- **Adafruit IO Account** for webhook delivery to ESP32

### 1. Hardware Wiring

Connect the following to your ESP32:

**SPI TFT Display** (adjust pins as needed in code):
```
TFT_CS   → GPIO 5
TFT_DC   → GPIO 4
TFT_SDA  → GPIO 23 (MOSI)
TFT_SCL  → GPIO 18 (SCK)
TFT_RST  → GPIO 2 (or 3.3V if auto-reset)
```

**Matrix Keypad**:
```
Row Pins    → GPIO 13, 12, 14, 27
Column Pins → GPIO 26, 25, 33
```

**DFmini MP3 Module** (via software/hardware serial):
```
RX → GPIO 16 (or hardware RX2)
TX → GPIO 17 (or hardware TX2)
Speaker → Audio output pins
```

### 2. ESP32 Firmware Setup

1. Clone or download this repository
2. Open `mcproject2.ino` in Arduino IDE
3. Install required libraries:
   - TFT_eSPI (SPI display)
   - DFRobot_DFPlayer (MP3 module)
   - ArduinoJson (JSON parsing)
4. Configure WiFi credentials in the code:
   ```cpp
   const char* ssid = "YOUR_SSID";
   const char* password = "YOUR_PASSWORD";
   ```
5. Configure Adafruit IO credentials:
   ```cpp
   #define IO_USERNAME "your_adafruit_io_username"
   #define IO_KEY "your_adafruit_io_key"
   ```
6. Update the menu feed name in code (where transactions are sent)
7. Upload to ESP32 DevKit

### 3. Google Sheets & Apps Script Setup

1. Create a Google Sheet with two tabs:
   - **Menu Tab**: Columns: `ItemCode`, `ItemName`, `Price`
   - **Transactions Tab**: Columns: `Timestamp`, `ItemCode`, `Quantity`, `Amount`, `BillQR`

2. Open **Extensions → Apps Script** in your Google Sheet

3. Create two functions (or use `apps_script.js` from this repo):
   ```javascript
   // Function to fetch menu data (called by ESP32)
   function doGet(e) {
     var sheet = SpreadsheetApp.getActiveSheet();
     var data = sheet.getDataRange().getValues();
     return ContentService.createTextOutput(JSON.stringify(data))
       .setMimeType(ContentService.MimeType.JSON);
   }
   
   // Function to log transactions
   function logTransaction(itemCode, quantity, amount, billQR) {
     var sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName("Transactions");
     sheet.appendRow([new Date(), itemCode, quantity, amount, billQR]);
   }
   ```

4. Deploy as a web app:
   - Click **Deploy → New Deployment → Web App**
   - Execute as: Your account
   - Who has access: Anyone

5. Copy the deployment URL and update it in the ESP32 code

### 4. Razorpay & Pipedream Integration

1. **Razorpay Setup**:
   - Create a Razorpay account & get API keys
   - Set up webhook in Razorpay dashboard pointing to Pipedream

2. **Pipedream Setup**:
   - Create a Pipedream workflow that receives Razorpay webhooks
   - Add an HTTP action to forward payment confirmations to Adafruit IO

3. **Adafruit IO Setup**:
   - Create a feed named `payment-status`
   - Update the feed name in your ESP32 code
   - The webhook URL from Pipedream should post to: 
     ```
     https://io.adafruit.com/api/v2/{username}/feeds/payment-status/data
     ```

### 5. Python Backend (Optional Enhanced Features)

If you're using Python for additional processing:
```bash
pip install -r requirements.txt
python backend.py
```

---

## 📱 User Guide

### Placing an Order

1. **Power on** the Canteen-Pay terminal
2. **View menu** on the TFT display (items with codes and prices)
3. **Enter item code** using the matrix keypad (e.g., "01")
4. **Enter quantity** (e.g., "2")
5. **Choose action**:
   - **A**: Add another item
   - **C**: Clear current order
   - **B**: Confirm & generate payment QR
6. **Scan QR** with your phone's UPI app
7. **Complete payment** on your phone
8. **Receive audio confirmation** beep on the terminal
9. **View digital bill QR** on the display
10. **Press D** to upload transaction to Google Sheets (optional, or automatic)

### Admin Operations

- **Menu Updates**: Modify Google Sheet → Syncs to ESP32 on next WiFi connection
- **Transaction Records**: Check Google Sheets "Transactions" tab for all payments
- **Transaction Upload**: Press D on matrix keypad to manually sync to Sheets

---

## 📁 File Structure

```
canteen-pay/
├── README.md                  # This file
├── mcproject2.ino            # Main ESP32 Arduino sketch
├── rm_qrcode.c               # QR code generation library (C implementation)
├── rm_qrcode.h               # QR code header file
├── apps_script.js            # Google Apps Script for Sheets integration
├── esp32_code/               # Additional ESP32 utilities
│   └── (other ESP32 modules)
├── canteenpay.ino            # Configuration file for TFT pins & credentials
├── requirements.txt          # Python dependencies (if using backend)
└── (other configuration files)
```

### File Descriptions

| File | Purpose |
|------|---------|
| `mcproject2.ino` | Main firmware: handles keypad input, display, payment QR generation, Adafruit IO integration |
| `rm_qrcode.c/h` | QR code generation library for bill QR creation |
| `apps_script.js` | Google Sheets API functions for menu sync & transaction logging |
| `esp32_code/` | Additional modular code for specific components (display, keypad, etc.) |
| `canteenpay.ino` | Configuration: WiFi SSID/password, API keys, pin assignments |

---

## 🌐 API & Integration Details

### Google Sheets API (Apps Script)
- **Menu Fetch**: `GET` request to Apps Script URL with `?action=getMenu`
- **Transaction Log**: `POST` to Apps Script with transaction details

### Razorpay → Pipedream → Adafruit IO
- Razorpay webhook triggers Pipedream
- Pipedream forwards payment confirmation to Adafruit IO feed
- ESP32 subscribes to Adafruit IO feed for real-time payment status

### Adafruit IO
- Feed: `payment-status`
- Data format: `{"status": "success", "transactionId": "...", "billQR": "..."}`

---

## 🐛 Known Limitations & Future Work

### Current Limitations (Beta)
- Requires WiFi for menu sync and webhook delivery
- Manual transaction upload via D keypad press (can be automated)
- QR code timeout: user must scan within X seconds
- Single terminal per instance (no multi-terminal sync yet)

### Planned Features 🚀
- [ ] Offline payment queue (sync when WiFi returns)
- [ ] Multi-terminal support with central dashboard
- [ ] Admin mobile app for menu management
- [ ] SMS/email bill receipts
- [ ] Loyalty points & discount system
- [ ] Real-time analytics dashboard
- [ ] Support for other payment gateways (PhonePe, Google Pay)
- [ ] Biometric authentication for staff

---

## 👥 Team

- **Josh Mammen Jacob**
- **Aaron George Roy**
- **Anandu Ajai**
- **Joseph Mathew Joy**

---

## 📜 License

This project is currently in **beta**. License details to be determined. For inquiries, please contact the development team.

---

## 🤝 Contributing

We welcome contributions! Please feel free to:
- Report bugs via GitHub Issues
- Suggest features or improvements
- Submit pull requests with enhancements

---

## 📧 Support & Contact

For questions, issues, or feature requests, please open a GitHub issue or contact the team directly.

---

## 🙏 Acknowledgments

- **Razorpay** for payment processing
- **Google Sheets & Apps Script** for data management
- **Adafruit IO** for IoT integration
- **Pipedream** for webhook routing
- **Arduino & ESP32 communities** for excellent documentation

---

## 📚 Additional Resources

- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)
- [DFPlayer Mini](https://www.dfrobot.com/product-1121.html)
- [Razorpay Documentation](https://razorpay.com/docs/)
- [Adafruit IO Guide](https://learn.adafruit.com/adafruit-io-basics)
- [Google Apps Script](https://script.google.com/)

---

**Last Updated**: April 2026  
**Status**: Beta (v0.1)
