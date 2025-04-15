#include "storageManager.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

const char* path = "/smartdrip.json";

// Genera una clave tipo "2025-03-31"
String getCurrentDateKey() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char buffer[11];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", 1900 + timeinfo->tm_year, 1 + timeinfo->tm_mon, timeinfo->tm_mday);
  return String(buffer);
}
// Verifica si hay datos para una fecha dada
bool isDataStoredForDate(const String& dateKey) {
  File file = SPIFFS.open(path, "r");
  if (!file) return false;
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, file)) {
    file.close();
    return false;
  }
  file.close();
  JsonObject data = doc["data"];
  return !data[dateKey].isNull();
}
// Guarda o actualiza datos diarios
void storeOrUpdateDailyDataJson(int day, int month, int year,
                                int newSubstrate, int newHumidity, int newTemp,
                                bool dripActive, bool forceOverwrite,
                                String customDateKey) {
  String dateKey = customDateKey;
  if (dateKey == "") {
    char buffer[11];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
    dateKey = String(buffer);
  }
  File file = SPIFFS.open("/data.json", "r");
  DynamicJsonDocument doc(8192);  // Tamaño mayor para evitar problemas con muchos días
  if (file) {
    deserializeJson(doc, file);
    file.close();
  }
  JsonObject data = doc["data"];
  JsonObject dayData;
  if (!data[dateKey].isNull()) {
    dayData = data[dateKey];
    Serial.printf("🔄 Actualizando datos del día %s\n", dateKey.c_str());
  } else {
    Serial.printf("🆕 Guardando nuevos datos para el día %s\n", dateKey.c_str());
    dayData = data.createNestedObject(dateKey);
  }
  // Condicionales para no sobreescribir si ya existen
  if (forceOverwrite || dayData["substrate"].isNull()) dayData["substrate"] = newSubstrate;
  if (forceOverwrite || dayData["humidity"].isNull())  dayData["humidity"] = newHumidity;
  if (forceOverwrite || dayData["temp"].isNull())      dayData["temp"]     = newTemp;
  if (forceOverwrite || dayData["drip"].isNull())      dayData["drip"]     = dripActive;
  file = SPIFFS.open("/data.json", "w");
  if (serializeJsonPretty(doc, file) == 0) {
    Serial.println("❌ Error al guardar JSON");
  } else {
    Serial.println("✅ Datos guardados/actualizados correctamente");
  }
  file.close();
}
void checkStorageFile() {
  if (!SPIFFS.exists("/data.json")) {
    Serial.println("📂 data.json no existe. Creando archivo vacío...");
    File file = SPIFFS.open("/data.json", "w");
    if (!file) {
      Serial.println("❌ No se pudo crear data.json");
      return;
    }
    DynamicJsonDocument doc(512);
    doc["fecha_creacion"] = "0000-00-00";
    doc["data"] = JsonObject();  // Estructura vacía para datos
    serializeJson(doc, file);
    file.close();
    Serial.println("✔ data.json creado correctamente.");
  } else {
    Serial.println("✔ Archivo data.json encontrado.");
  }
}
void updateErrorLog(String smtpError, String mailError) {
  File file = SPIFFS.open("/data.json", "r");
  DynamicJsonDocument doc(4096);
  if (file) {
    deserializeJson(doc, file);
    file.close();
  }
  doc["errores"]["smtp"] = smtpError;
  doc["errores"]["envio"] = mailError;
  file = SPIFFS.open("/data.json", "w");
  if (!file) {
    Serial.println("❌ No se pudo abrir data.json para escritura");
    return;
  }
  if (serializeJsonPretty(doc, file) == 0) {
    Serial.println("❌ Error al guardar errores en data.json");
  } else {
    Serial.println("📌 Errores actualizados en data.json");
  }
  file.close();
}
void printDailyData() {
  File file = SPIFFS.open("/data.json", "r");
  if (!file) {
    Serial.println("❌ No se pudo abrir data.json");
    return;
  }
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, file)) {
    file.close();
    Serial.println("❌ Error al leer el archivo JSON");
    return;
  }
  file.close();
  JsonObject data = doc["data"];
  if (!data) {
    Serial.println("⚠️ No hay datos almacenados");
    return;
  }
  Serial.println("📅 Datos diarios registrados:");
  for (JsonPair kv : data) {
    String fecha = kv.key().c_str();
    JsonObject dia = kv.value().as<JsonObject>();
    Serial.printf("📆 %s:\n", fecha.c_str());
    Serial.printf("   🌱 Humedad sustrato: %d%%\n", dia["substrate"].as<int>());
    Serial.printf("   💧 Humedad ambiente: %d%%\n", dia["humidity"].as<int>());
    Serial.printf("   🌡  Temperatura: %d°C\n", dia["temp"].as<int>());
    Serial.printf("   🚿 Riego activado: %s\n", dia["drip"].as<bool>() ? "Sí" : "No");
  }
}
String printMonthlyDataJson(int month, int year, bool debug) {
  File file = SPIFFS.open("/data.json", "r");
  if (!file) {
    if (debug) Serial.println("❌ No se pudo abrir data.json");
    return "";
  }
  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, file)) {
    file.close();
    if (debug) Serial.println("❌ Error al leer JSON");
    return "";
  }
  file.close();
  JsonObject data = doc["data"];
  if (data.isNull()) return "";
  String result = "";
  char lineBuffer[128];
  int lastHumidity = -100, lastTemp = -100, lastSubstrate = -100;
  for (int day = 1; day <= 31; day++) {
    char dateKey[11];
    snprintf(dateKey, sizeof(dateKey), "%04d-%02d-%02d", year, month, day);
    if (data[dateKey].isNull()) continue;
    JsonObject dayData = data[dateKey];
    if (!dayData["humidity"].isNull())  lastHumidity = dayData["humidity"].as<int>();
    if (!dayData["temp"].isNull())      lastTemp = dayData["temp"].as<int>();
    if (!dayData["substrate"].isNull()) lastSubstrate = dayData["substrate"].as<int>();
    bool drip = !dayData["drip"].isNull() && dayData["drip"].as<bool>();
    bool hasData = lastHumidity != -100 || lastTemp != -100 || lastSubstrate != -100 || drip;
    if (!hasData) continue;
    snprintf(lineBuffer, sizeof(lineBuffer),
             "Día %d: Riego: %s | Humedad sustrato: %d%% | Humedad ambiental: %d%% | Temp: %d°C\n",
             day, drip ? "Sí" : "No", lastSubstrate, lastHumidity, lastTemp);
    if (debug) Serial.print(lineBuffer);
    result += lineBuffer;
  }
  return result;
}
