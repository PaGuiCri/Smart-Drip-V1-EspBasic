#include "configManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// Variables globales definidas en main.cpp
extern String startTime, endTime;
extern int dripTimeLimit, dripHumidityLimit;
unsigned long idNumber = 0;
String idUser = "", idSmartDrip = "", idSDHex = "";
String ssid = "", pass = "";

// 🔁 XOR simétrico
String xorEncryptDecrypt(const String &input, const String &key) {
  String output = "";
  for (size_t i = 0; i < input.length(); i++) {
    output += char(input[i] ^ key[i % key.length()]);
  }
  return output;
}

// 🔁 String → HEX
String stringToHex(const String &input) {
  String hexString = "";
  for (int i = 0; i < input.length(); i++) {
    char hex[3];
    sprintf(hex, "%02X", input[i]);
    hexString += hex;
  }
  return hexString;
}

// 🔁 HEX → String
String hexToString(const String &hexString) {
  String output = "";
  for (int i = 0; i < hexString.length(); i += 2) {
    String byteStr = hexString.substring(i, i + 2);
    char byte = strtol(byteStr.c_str(), NULL, 16);
    output += byte;
  }
  return output;
}

// 🔓 Decodifica desde HEX y aplica XOR
String decodeAndDecrypt(const String &hex, const String &key) {
  String xorInput = hexToString(hex);
  return xorEncryptDecrypt(xorInput, key);
}

// 🔐 Codifica con XOR y HEX
String encryptAndEncode(const String &plainText, const String &key) {
  String xorResult = xorEncryptDecrypt(plainText, key);
  return stringToHex(xorResult);
}

// 📥 Cargar configuración desde config.json
void loadConfigFromJson() {
  Serial.println("\n📥 [loadConfigFromJson] Iniciando lectura de config.json...");
  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    Serial.println("❌ [loadConfigFromJson] No se pudo abrir config.json");
    return;
  }
  Serial.println("📄 [loadConfigFromJson] Contenido bruto del archivo:");
  while (file.available()) Serial.write(file.read());
  file.seek(0);
  StaticJsonDocument<768> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.print("❌ [loadConfigFromJson] Error al parsear config.json: ");
    Serial.println(error.c_str());
    return;
  }
  // Extraer campos
  idUser = doc["config"]["idUser"].as<String>();
  idSmartDrip = doc["config"]["idSmartDrip"].as<String>();
  idSDHex = doc["config"]["idSDHex"].as<String>();
  startTime = doc["watering"]["start_time"].as<String>();
  endTime = doc["watering"]["end_time"].as<String>();
  dripTimeLimit = doc["watering"]["duration_minutes"];
  dripHumidityLimit = doc["watering"]["humidity_threshold"];
  String ssidHex = doc["wifi"]["ssid"] | "";
  String passHex = doc["wifi"]["pass"] | "";
  // Mostrar paso a paso la decodificación
  Serial.println("\n📊 [loadConfigFromJson] DECODIFICANDO CREDENCIALES");
  Serial.printf("🔑 Clave XOR (idUser): %s\n", idUser.c_str());
  Serial.printf("📦 SSID HEX:           %s\n", ssidHex.c_str());
  Serial.printf("📦 PASS HEX:           %s\n", passHex.c_str());
  String ssidDecoded = hexToString(ssidHex);
  String passDecoded = hexToString(passHex);
  Serial.printf("🔁 SSID XOR Input:     %s\n", ssidDecoded.c_str());
  Serial.printf("🔁 PASS XOR Input:     %s\n", passDecoded.c_str());
  ssid = xorEncryptDecrypt(ssidDecoded, idUser);
  pass = xorEncryptDecrypt(passDecoded, idUser);
  Serial.printf("📶 SSID final:         %s\n", ssid.c_str());
  Serial.printf("🔒 PASS final:         %s\n", pass.c_str());
  idNumber = strtoul(idSDHex.c_str(), nullptr, 16);
  Serial.println("✔ [loadConfigFromJson] Configuración cargada correctamente:");
  Serial.printf("👤 Usuario: %s | Dispositivo: %s | ID Hex: %s | ID Num: %lu\n",
                idUser.c_str(), idSmartDrip.c_str(), idSDHex.c_str(), idNumber);
  Serial.printf("🕒 Horario riego: %s - %s | %d min | Límite: %d%%\n",
                startTime.c_str(), endTime.c_str(), dripTimeLimit, dripHumidityLimit);
  Serial.println("✅ [loadConfigFromJson] Finalizado.\n");
}
// 💾 Guardar configuración WiFi (básica)
void saveConfigToJson() {
  Serial.println("\n💾 [saveConfigToJson] Iniciando guardado de configuración WiFi...");
  File file = LittleFS.open("/config.json", "r");
  DynamicJsonDocument doc(1024);
  bool canWrite = true;
  if (file) {
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
      Serial.print("⚠️ [saveConfigToJson] Error al leer config.json: ");
      Serial.println(error.c_str());
      canWrite = false;
    }
  } else {
    Serial.println("⚠️ [saveConfigToJson] config.json no encontrado. Se creará nuevo.");
    canWrite = false;
  }
  if (!canWrite) {
    Serial.println("❌ [saveConfigToJson] Abortado para evitar sobrescritura incorrecta.");
    return;
  }
  // Actualizar campos
  doc["wifi"]["ssid"] = encryptAndEncode(ssid, idUser);
  doc["wifi"]["pass"] = encryptAndEncode(pass, idUser);
  Serial.println("📄 [saveConfigToJson] Configuración que se va a guardar:");
  serializeJsonPretty(doc, Serial);
  file = LittleFS.open("/config.json", "w");
  if (!file) {
    Serial.println("❌ [saveConfigToJson] No se pudo abrir config.json para escritura");
    return;
  }
  serializeJsonPretty(doc, file);
  file.close();
  Serial.println("✅ [saveConfigToJson] Configuración guardada correctamente.\n");
}

