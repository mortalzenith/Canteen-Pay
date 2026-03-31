function doGet(e) {
  var action = e.parameter.action;
  var sheet = SpreadsheetApp.getActiveSpreadsheet();

  // ----- ESP32 BOOT SYNC: Download all items to SD Card -----
  if (action == 'downloadItems') {
    var itemsSheet = sheet.getSheetByName("ITEMS_DB");
    var data = itemsSheet.getDataRange().getDisplayValues();
    var csvData = "";
    
    // Loop through all items (starting at row 1 to skip headers)
    for (var i = 1; i < data.length; i++) {
      // Formats as: ItemNo,CostPrice,SellPrice
      // Array index 0=ItemNo, 1=ItemName, 3=SellPrice
      csvData += data[i][0] + "," + data[i][1] + "," + data[i][3] + "\n";
    }
    return ContentService.createTextOutput(csvData);
  }
}

function doPost(e) {
  try {
    var action = e.parameter.action;
    var sheet = SpreadsheetApp.getActiveSpreadsheet();

    // ----- HOURLY/MANUAL SYNC: Upload transactions from ESP32 to Sheets -----
    if (action == 'uploadTransactions') {
      var txSheet = sheet.getSheetByName("TRANSACTIONS_DB");
      
      // THE FIX: Use postData.contents to read the raw text/plain file from ESP32
      var csvData = e.postData.contents; 
      
      if (!csvData) {
        return ContentService.createTextOutput("ERROR: No data received from ESP32!");
      }

      var rows = csvData.split("\n");
      
      for (var i = 0; i < rows.length; i++) {
        if (rows[i].length > 2) { // Ignore empty blank lines
          var columns = rows[i].split(",");
          txSheet.appendRow(columns);
        }
      }
      return ContentService.createTextOutput("SYNC_SUCCESS");
    }
    
    return ContentService.createTextOutput("ERROR: Unknown Action");

  } catch (error) {
    // If it crashes, send the exact error back to the ESP32 Serial Monitor
    return ContentService.createTextOutput("APPS SCRIPT ERROR: " + error.toString());
  }
}