#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"
#include <HX711_ADC.h>

#define HX711_DT 16
#define HX711_SCK 17

#define RS485_RX 18
#define RS485_TX 19

HX711_ADC LoadCell(HX711_DT, HX711_SCK);

HardwareSerial RS485Serial(1);

// #define HYSTERESIS 5.0f
// #define HYSTERESIS_KG 0.01f
#define HYSTERESIS_STABLE_CHECK 1.0f
#define STABLE_READING_REQUIRED 32

unsigned long t = 0;
float weight = 0.00;
float lastWeight = 0.0f;
int stableReadingsCount = 0;
float stableWeight = 0.0f;
boolean doCheckStableState = false;
int CHECK_STATUS = 0;

DynamicJsonDocument doc(1024);
JsonArray parts;

// BEGIN: PART MANAGER
void getPartList() {
  const size_t capacity = JSON_ARRAY_SIZE(9) + JSON_OBJECT_SIZE(9) + 400;
  doc.clear(); // Clear the document before use

  // Read the existing file
  File file = SPIFFS.open("/partList.json", "r");
  if (!file) {
    // Serial.println("Failed to open file for reading");
    return;
  }

  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    // Serial.println("Failed to read file, returning empty JSON document");
    return;
  }

  // Check if partList key exists and is an array
  if (!doc.containsKey("partList") || !doc["partList"].is<JsonArray>()) {
    // Serial.println("partList key is missing or not an array");
    return;
  }

  DynamicJsonDocument responseDoc(2048);
  responseDoc["data"] = doc["partList"];
  responseDoc["message"] = "Part list retrieved successfully";
  responseDoc["status"] = 200;

  serializeJson(responseDoc, Serial);

  Serial.println();
}

void getPart(int id) {

}

void createPartList() {
  const char* jsonContent = R"rawliteral(
  {
    "partList": [
      { "id": 1,"name": "PART 01", "std": 100.00, "unit": "gr", "hysteresis": 5.0 },
      { "id": 2,"name": "PART 02", "std": 500.00, "unit": "gr", "hysteresis": 5.0 },
      { "id": 3,"name": "PART 03", "std": 1.25, "unit": "kg", "hysteresis": 0.01 }
    ]
  }
  )rawliteral";

  // Open file for writing
  File file = SPIFFS.open("/partList.json", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open partList.json for writing");
    return;
  }

  // Write the JSON content to the file
  if (file.print(jsonContent)) {
    Serial.println("partList.json created successfully");
  } else {
    Serial.println("Failed to write to partList.json");
  }

  // Close the file
  file.close();

  DynamicJsonDocument responseDoc(1024);
  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Parts created";
  responseDoc["status"] = 200;
  
  serializeJson(responseDoc, Serial);

  Serial.println();
}

void createPart(String name, float std, String unit, float hysteresis) {
  const size_t capacity = JSON_ARRAY_SIZE(9) + JSON_OBJECT_SIZE(9) + 400;
  DynamicJsonDocument temp(capacity);

  // Try to read the existing file
  File file = SPIFFS.open("/partList.json", "r");
  if (!file) {
    // Serial.println("Failed to open file for reading, creating new file");
    // Create a new file
    file = SPIFFS.open("/partList.json", "w");
    if (!file) {
      // Serial.println("Failed to create new file");
      return;
    }
    // Initialize the JSON structure
    JsonArray partListArray = temp.createNestedArray("partList");
    file.close();
  } else {
    DeserializationError error = deserializeJson(temp, file);
    file.close();
    if (error) {
      // Serial.println("Failed to read file, using empty JSON");
    }
  }

  JsonArray partListArray;
  if (!temp.containsKey("partList") || !temp["partList"].is<JsonArray>()) {
    partListArray = temp.createNestedArray("partList");
  } else {
    partListArray = temp["partList"].as<JsonArray>();
  }

  int id = partListArray[partListArray.size() - 1]["id"].as<int>() + 1;

  // Check if part with the same ID already exists
  for (JsonObject part : partListArray) {
    if (part["id"].as<int>() == id) {
      // Serial.println("Part with this ID already exists");
      return;
    }
  }

  JsonObject newPart = partListArray.createNestedObject();
  newPart["id"] = id;
  newPart["name"] = name;
  newPart["std"] = std;
  newPart["unit"] = unit;
  newPart["hysteresis"] = hysteresis;

  // Write the updated file
  file = SPIFFS.open("/partList.json", "w");
  if (!file) {
    // Serial.println("Failed to open file for writing");
    return;
  }
  serializeJson(temp, file);
  file.close();

  DynamicJsonDocument responseDoc(1024);
  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Part created";
  responseDoc["status"] = 200;
  
  serializeJson(responseDoc, Serial);

  Serial.println();
}

void updatePart(int id, String name, float std, String unit, float hysteresis) {
  DynamicJsonDocument temp(1024);

  // Read the existing file
  File file = SPIFFS.open("/partList.json", "r");
  if (!file) {
    // Serial.println("Failed to open file for reading");
    return;
  }
  DeserializationError error = deserializeJson(temp, file);
  file.close();
  if (error) {
    // Serial.println("Failed to read file, using empty JSON");
  }

  JsonArray partListArray = temp["partList"].as<JsonArray>();

  // Update part if it exists
  for (JsonObject part : partListArray) {
    if (part["id"].as<int>() == id) {
      part["name"] = name;
      part["std"] = std;
      part["unit"] = unit;
      part["hysteresis"] = hysteresis;
      // Serial.println("id: " + String(part["id"].as<int>()));
      // Serial.println("name: " + part["name"].as<String>());
      // Serial.println("std: " + String(part["std"].as<float>(), 2));
      // Serial.println("unit: " + part["unit"].as<String>());
      break;
    }
  }

  // Write the updated file
  file = SPIFFS.open("/partList.json", "w");
  if (!file) {
    // Serial.println("Failed to open file for writing");
    return;
  }
  serializeJson(temp, file);
  file.close();

  DynamicJsonDocument responseDoc(1024);
  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Part updated";
  responseDoc["status"] = 200;
  serializeJson(responseDoc, Serial);

  Serial.println();
}

void deletePart(int id) {
  DynamicJsonDocument temp(1024);

  // Read the existing file
  File file = SPIFFS.open("/partList.json", "r");
  if (!file) {
    // Serial.println("Failed to open file for reading");
    return;
  }
  DeserializationError error = deserializeJson(temp, file);
  file.close();
  if (error) {
    // Serial.println("Failed to read file, returning");
    return;
  }

  JsonArray partListArray = temp["partList"].as<JsonArray>();
  for (size_t i = 0; i < partListArray.size(); i++) {
    if (partListArray[i]["id"].as<int>() == id) {
      // Serial.println("id: " + String(partListArray[i]["id"].as<int>()));
      // Serial.println("name: " + partListArray[i]["name"].as<String>());
      // Serial.println("std: " + String(partListArray[i]["std"].as<float>(), 2));
      // Serial.println("unit: " + partListArray[i]["unit"].as<String>());
      partListArray.remove(i);
      break;
    }
  }

  // Write the updated file
  file = SPIFFS.open("/partList.json", "w");
  if (!file) {
    // Serial.println("Failed to open file for writing");
    return;
  }
  serializeJson(temp, file);
  file.close();

  DynamicJsonDocument responseDoc(1024);
  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Part deleted";
  responseDoc["status"] = 200;
  
  serializeJson(responseDoc, Serial);

  Serial.println();
}
// END: PART MANAGER

// BEGIN: SCALE MANAGER
void tare() {
  LoadCell.tareNoDelay();
  while (!LoadCell.getTareStatus()) {
    LoadCell.update();
    delay(100);
  }
  // LoadCell.tare();

  DynamicJsonDocument responseDoc(1024);
  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Tare initialized successfully";
  responseDoc["status"] = 200;
  
  serializeJson(responseDoc, Serial);

  Serial.println();
}

float getCalibrationFactor() {
  File file = SPIFFS.open("/config.json", "r");
  if (!file) {
    // Serial.println("Failed to open file for reading");
    return 1.0;  // Return the default calibration factor if file reading fails
  }

  DynamicJsonDocument temp(1024);
  DeserializationError error = deserializeJson(temp, file);
  file.close();
  if (error) {
    // Serial.println("Failed to read file, using default calibration factor");
    return 1.0;  // Return the default calibration factor if file reading fails
  }

  // Check if the calFactor key exists and is a valid float
  if (!temp.containsKey("calFactor") || !temp["calFactor"].is<float>()) {
    // Serial.println("calFactor key is missing or not a valid float, using default calibration factor");
    return 1.0;  // Return the default calibration factor if the key is missing or invalid
  }

  float calibrationFactor = temp["calFactor"].as<float>();
  // Serial.println("Calibration factor read from config: " + String(calibrationFactor));

  return calibrationFactor;
}

void initScale() {
  LoadCell.begin();
  
  // preciscion right after power-up can be improved by adding a few seconds of stabilizing time
  unsigned long stabilizingtime = 2000;

  //set this to false if you don't want tare to be performed in the next step
  boolean _tare = true;
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag() || LoadCell.getSignalTimeoutFlag()) {
    // Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1);
  } else {
    float calibrationFactor = getCalibrationFactor();
    // Serial.println(calibrationFactor);
    if (calibrationFactor > 0) {
      LoadCell.setCalFactor(calibrationFactor);
    } else {
      LoadCell.setCalFactor(1.0);
    }
    // Serial.println("Startup is complete");
  }
  while (!LoadCell.update());
}

void refreshDataSet() {
  LoadCell.refreshDataSet();

  DynamicJsonDocument responseDoc(1024);
  responseDoc["data"] = nullptr;
  responseDoc["message"] = "Data set refreshed successfully";
  responseDoc["status"] = 200;
  
  serializeJson(responseDoc, Serial);

  Serial.println();
}

bool checkStableState(float wt, String unit) {
  float hys = 0.0;
  if (doCheckStableState) {
    if (unit == "kg") {
      hys = HYSTERESIS_STABLE_CHECK / 1000.0;
    } else {
      hys = HYSTERESIS_STABLE_CHECK;
    }

    if (wt >= wt - hys && wt <= wt + hys && abs(wt - lastWeight) <= hys) {
    // if (abs(wt - lastWeight) < HYSTERESIS_STABLE_CHECK) {
      stableReadingsCount++;
    } else {
      stableReadingsCount = 0;
    }
    lastWeight = wt;
  }
  doCheckStableState = false;
    
  return (stableReadingsCount >= STABLE_READING_REQUIRED);
}

float getScale() {
  static boolean newDataReady = 0;

  if (LoadCell.update()) newDataReady = true;

  if (newDataReady) {
    if (millis() > t) {
      weight = LoadCell.getData();
      // Serial.print("Load_cell output val: ");
      // Serial.println(weight);
      newDataReady = 0;
      t = millis();
      doCheckStableState = true;
    }
  }

  return weight;
}

void getWeight(float partStd, String partUnit, float hysteresis) {
  float wt = getScale();
  if (partUnit == "kg") {
    wt = wt / 1000.0;
  }

  if (checkStableState(wt, partUnit)) {
    // float hys = 0.0;
    // if (partUnit == "kg") {
    //   hys = HYSTERESIS_KG;
    // } else {
    //   hys = HYSTERESIS;
    // }

    if (partUnit == "kg" && wt <= 0.01f) {
      CHECK_STATUS = 0;
      stableWeight = 0.0;
    } else if (partUnit == "gr" && wt <= 2.0f) {
      CHECK_STATUS = 0;
      stableWeight = 0.0;
    } else if (wt >= partStd - hysteresis && wt <= partStd + hysteresis && CHECK_STATUS == 0) {
      CHECK_STATUS = 1;
      stableWeight = wt;
    } else if (CHECK_STATUS == 0) {
      CHECK_STATUS = 2;
      stableWeight = wt;
    }
  } else if (stableWeight != 0.0 && (wt <= stableWeight * 0.9f || wt >= stableWeight * 1.1f)) {
    CHECK_STATUS = 0;
    stableWeight = 0.0;
  }

  DynamicJsonDocument doc(1024);
  doc["weight"] = wt;
  doc["check"] = CHECK_STATUS;

  DynamicJsonDocument responseDoc(1024);
  responseDoc["data"] = doc;
  responseDoc["message"] = "Weight retrieved successfully";
  responseDoc["status"] = 200;

  serializeJson(responseDoc, Serial);

  Serial.println();
}

void getStableWeight() {
  float data = 0.0;
  bool stable = false;
  LoadCell.refreshDataSet();
  while (!stable) {
    float newWeight = getScale();
    if (checkStableState(newWeight, "gr")) {
      if (newWeight > 2.0f) {
        data = newWeight;
        stable = true;
      }
    }
  }

  DynamicJsonDocument responseDoc(1024);
  responseDoc["data"] = data;
  responseDoc["message"] = "Stable Weight retrieved successfully";
  responseDoc["status"] = 200;

  serializeJson(responseDoc, Serial);

  Serial.println();
}

void initCalibration() {
  // Serial.println("***");
  // Serial.println("Start calibration:");
  // Serial.println("Place the load cell an a level stable surface.");
  // Serial.println("Remove any load applied to the load cell.");

  LoadCell.tareNoDelay();
  while (!LoadCell.getTareStatus()) {
    LoadCell.update();
    delay(100);
  }

  // Serial.println("Now, place your known mass on the loadcell.");
  
  DynamicJsonDocument responseDoc(1024);
  responseDoc["data"] = true;
  responseDoc["message"] = "Calibration initialized successfully";
  responseDoc["status"] = 200;

  serializeJson(responseDoc, Serial);

  Serial.println();
}

void createCalibrationFactor(float knownWeight) {
  // Refresh the dataset to be sure that the known mass is measured correctly
  LoadCell.refreshDataSet();

  // Get the new calibration value
  float newCalibrationFactor = LoadCell.getNewCalibration(knownWeight);

  // Check if the new calibration factor is valid
  if (isnan(newCalibrationFactor)) {
    // Serial.println("Failed to get new calibration factor, result is NaN");
    return;
  }

  // Serial.print("New calibration factor has been set to: ");
  // Serial.println(newCalibrationFactor);
  // Serial.println("Use this as calibration factor (calFactor) in your project sketch.");

  // Serial.println("Save this value to config.json");

  // Create a new JSON document to store the calibration factor
  StaticJsonDocument<200> temp;
  temp["calFactor"] = newCalibrationFactor;

  // Open the file for writing
  File file = SPIFFS.open("/config.json", "w");
  if (!file) {
    // Serial.println("Failed to open file for writing");
    return;  // Return the new calibration factor even if file writing fails
  }

  // Write the updated JSON to the file
  serializeJson(temp, file);
  file.close();

  // Serial.println("End calibration");
  // Serial.println("***");

  DynamicJsonDocument responseDoc(1024);
  responseDoc["data"] = newCalibrationFactor;
  responseDoc["message"] = "Cal Factor created";
  responseDoc["status"] = 200;
  
  serializeJson(responseDoc, Serial);

  Serial.println();

}
// END: SCALE MANAGER

void handleCommand(String inputString) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, inputString);

  if (error) {
    // Serial.println("Failed to parse JSON");
    return;
  }

  int cmd = doc["cmd"];
  JsonObject data = doc["data"];

  switch (cmd) {
    case 1:
      getPartList();
      break;
    case 2:
      getPart(data["id"]);
      break;
    case 3:
      createPartList();
      break;
    case 4:
      createPart(data["name"], data["std"], data["unit"], data["hysteresis"]);
      break;
    case 5:
      updatePart(data["id"], data["name"], data["std"], data["unit"], data["hysteresis"]);
      break;
    case 6:
      deletePart(data["id"]);
      break;
    case 7:
      tare();
      break;
    case 8:
      getWeight(data["std"], data["unit"], data["hysteresis"]);
      break;
    case 9:
      getStableWeight();
      break;
    case 10:
      initCalibration();
      break;
    case 11:
      createCalibrationFactor(data["knownWeight"]);
      break;
    case 12:
      refreshDataSet();
      break;
    default:
      // Serial.println("Unknown command");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  RS485Serial.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);

  if (!SPIFFS.begin(true)) {
    // Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  initScale();
}

void loop() {
  String inputString = "";
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      break;
    }
    inputString += inChar;
  }
  // Serial.println("Received: " + inputString);
  handleCommand(inputString);
}