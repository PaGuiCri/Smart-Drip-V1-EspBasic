#include "configManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// Variables globales definidas en main.cpp
extern String startTime, endTime;
extern int dripTimeLimit, dripHumidityLimit;
unsigned long idNumber = 0;
String idUser = "", idSmartDrip = "", idSDHex = "";
String ssid = "", pass = "";
String smtpServer = "smtp.gmail.com";
int smtpPort = 465;
String smtpEmail = "";
String smtpPass = "";

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
  File file = LittleFS.open("/config/config.json", "r");
  if (!file) {
    Serial.println("❌ [loadConfigFromJson] No se pudo abrir config.json");
    return;
  }
  Serial.println("📄 [loadConfigFromJson] Contenido bruto del archivo:");
  while (file.available()) Serial.write(file.read());
  file.seek(0);
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.print("❌ [loadConfigFromJson] Error al parsear config.json: ");
    Serial.println(error.c_str());
    return;
  }
  // Extraer campos generales
  idUser = doc["config"]["idUser"] | "";
  idSmartDrip = doc["config"]["idSmartDrip"] | "";
  idSDHex = doc["config"]["idSDHex"] | "";
  startTime = doc["watering"]["start_time"] | "08:00";
  endTime = doc["watering"]["end_time"] | "10:00";
  dripTimeLimit = doc["watering"]["duration_minutes"] | 5;
  dripHumidityLimit = doc["watering"]["humidity_threshold"] | 40;
  // === WiFi ===
  String ssidHex = doc["wifi"]["ssid"] | "";
  String passHex = doc["wifi"]["pass"] | "";
  String ssidDecoded = hexToString(ssidHex);
  String passDecoded = hexToString(passHex);
  ssid = xorEncryptDecrypt(ssidDecoded, idUser);
  pass = xorEncryptDecrypt(passDecoded, idUser);
  // === SMTP ===
  smtpServer = doc["smtp"]["server"] | "smtp.gmail.com";
  smtpPort   = doc["smtp"]["port"]   | 465;
  String smtpEmailHex = doc["smtp"]["email"] | "";
  String smtpPassHex  = doc["smtp"]["pass"]  | "";
  String smtpEmailDecoded = hexToString(smtpEmailHex);
  String smtpPassDecoded  = hexToString(smtpPassHex);
  smtpEmail = xorEncryptDecrypt(smtpEmailDecoded, idUser);
  smtpPass  = xorEncryptDecrypt(smtpPassDecoded, idUser);
  // === Debug info ===
  Serial.println("\n📊 [loadConfigFromJson] DECODIFICANDO CREDENCIALES");
  Serial.printf("🔑 Clave XOR (idUser): %s\n", idUser.c_str());
  Serial.printf("📶 SSID final:         %s\n", ssid.c_str());
  Serial.printf("🔒 PASS final:         %s\n", pass.c_str());
  Serial.printf("📧 Email SMTP:         %s\n", smtpEmail.c_str());
  Serial.printf("🔑 Clave SMTP:         %s\n", smtpPass.c_str());
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
  Serial.println("\n💾 [saveConfigToJson] Iniciando guardado de configuración WiFi y SMTP...");
  File file = LittleFS.open("/config/config.json", "r");
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
  // Actualizar campos WiFi
  doc["wifi"]["ssid"] = encryptAndEncode(ssid, idUser);
  doc["wifi"]["pass"] = encryptAndEncode(pass, idUser);
  // Actualizar campos SMTP
  doc["smtp"]["server"] = smtpServer;
  doc["smtp"]["port"] = smtpPort;
  doc["smtp"]["email"] = encryptAndEncode(smtpEmail, idUser);
  doc["smtp"]["pass"]  = encryptAndEncode(smtpPass, idUser);
  Serial.println("📄 [saveConfigToJson] Configuración que se va a guardar:");
  serializeJsonPretty(doc, Serial);
  file = LittleFS.open("/config/config.json", "w");
  if (!file) {
    Serial.println("❌ [saveConfigToJson] No se pudo abrir config.json para escritura");
    return;
  }
  serializeJsonPretty(doc, file);
  file.close();
  Serial.println("✅ [saveConfigToJson] Configuración guardada correctamente.\n");
}

