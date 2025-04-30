#include "storageManager.h"
#include "mailManager.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

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
  File file = LittleFS.open(path, "r");
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
Serial.printf("📥 [storeOrUpdateDailyDataJson] Intentando guardar datos para: %s\n", dateKey.c_str());
File file = LittleFS.open("/data.json", "r");
DynamicJsonDocument doc(8192);  // Tamaño mayor para evitar problemas con muchos días
if (file) {
DeserializationError error = deserializeJson(doc, file);
file.close();
if (error) {
Serial.print("⚠️ Error al parsear data.json: ");
Serial.println(error.c_str());
doc.clear();  // Limpiar por si quedó corrupto
}
} else {
Serial.println("⚠️ data.json no existe. Se creará nuevo documento JSON.");
}
// Asegurarse que existe el objeto 'data'
if (!doc.containsKey("data")) {
Serial.println("⚠️ Sección 'data' no encontrada. Se crea nueva.");
doc.createNestedObject("data");
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
file = LittleFS.open("/data.json", "w");
if (!file) {
Serial.println("❌ No se pudo abrir data.json para escritura");
return;
}
if (serializeJsonPretty(doc, file) == 0) {
Serial.println("❌ Error al guardar JSON");
} else {
Serial.println("✅ Datos guardados/actualizados correctamente:");
serializeJsonPretty(doc, Serial);  // Mostramos el JSON completo por consola
}
file.close();
}
void checkStorageFile() {
  Serial.println("📁 [checkStorageFile] Verificando existencia de data.json...");
  if (!LittleFS.exists("/data.json")) {
    Serial.println("📂 data.json no existe. Creando archivo vacío...");
    File file = LittleFS.open("/data.json", "w");
    if (file) {
      file.print("{}");
      file.close();
      Serial.println("✔ data.json creado correctamente.");
    } else {
      Serial.println("❌ No se pudo crear data.json");
    }
    return;
  }
  Serial.println("✔ Archivo data.json encontrado.");
  File file = LittleFS.open("/data.json", "r");
  if (!file) {
    Serial.println("❌ No se pudo abrir data.json");
    return;
  }
  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, file)) {
    file.close();
    Serial.println("❌ Error al leer el archivo JSON");
    return;
  }
  file.close();
  JsonObject data = doc["data"];
  if (!data) {
    Serial.println("⚠️ No hay sección 'data' en el JSON");
    return;
  }
  // 🔎 Buscar el mes más reciente en los datos
  int latestYear = 0, latestMonth = 0;
  for (JsonPair kv : data) {
    String fecha = kv.key().c_str();
    if (fecha.length() != 10) continue;
    int year = fecha.substring(0, 4).toInt();
    int month = fecha.substring(5, 7).toInt();
    if (year > latestYear || (year == latestYear && month > latestMonth)) {
      latestYear = year;
      latestMonth = month;
    }
  }
  if (latestYear == 0) {
    Serial.println("⚠️ No se encontraron fechas válidas en los datos.");
    return;
  }
  Serial.printf("🔎 Mostrando datos del mes más reciente: %04d-%02d\n", latestYear, latestMonth);
  int count = 0;
  for (JsonPair kv : data) {
    String fecha = kv.key().c_str();
    int year = fecha.substring(0, 4).toInt();
    int month = fecha.substring(5, 7).toInt();
    if (year == latestYear && month == latestMonth) {
      JsonObject dia = kv.value().as<JsonObject>();
      Serial.printf("📆 %s:\n", fecha.c_str());
      Serial.printf("   🌱 Humedad sustrato: %d%%\n", dia["substrate"].as<int>());
      Serial.printf("   💧 Humedad ambiente: %d%%\n", dia["humidity"].as<int>());
      Serial.printf("   🌡  Temperatura: %d°C\n", dia["temp"].as<int>());
      Serial.printf("   🚿 Riego activado: %s\n", dia["drip"].as<bool>() ? "Sí" : "No");
      count++;
    }
  }
  Serial.printf("📊 Se mostraron %d días con datos del mes más reciente.\n", count);
}
void updateErrorLog(const String& tipoError, const String& mensajeError, const String& fecha) {
  const char* path = "/errors/errors.json";
  DynamicJsonDocument doc(4096);
  File file = LittleFS.open(path, "r");
  if (file) {
    DeserializationError err = deserializeJson(doc, file);
    if (err) {
      Serial.println("⚠️ Error al leer errors.json, se creará uno nuevo.");
      doc.clear();
    }
    file.close();
  }
  JsonObject errores = doc["errores"];
  char claveFecha[32];
  char claveCount[32];
  snprintf(claveFecha, sizeof(claveFecha), "fecha%s", tipoError.c_str());
  snprintf(claveCount, sizeof(claveCount), "%sCount", tipoError.c_str());
  if (!errores.containsKey(tipoError)) {
    errores[tipoError] = mensajeError;
    errores[claveFecha] = fecha;
    errores[claveCount] = 1;
  } else {
    errores[tipoError] = mensajeError;
    errores[claveFecha] = fecha;
    errores[claveCount] = errores[claveCount].as<int>() + 1;
  }
  file = LittleFS.open(path, "w");
  if (!file) {
    Serial.println("❌ No se pudo abrir errors.json para escritura");
    return;
  }
  if (serializeJsonPretty(doc, file) == 0) {
    Serial.println("❌ Error al guardar errores en errors.json");
  } else {
    Serial.println("📌 Errores actualizados en errors.json");
  }
  file.close();
  generateErrorSummaryFromDoc(errores);
}
void generateErrorSummaryFromDoc(JsonObject errores) {
  showErrorMail        = errores["envio"]       | " No mail errors ";
  showErrorMailConnect = errores["smtp"]        | " No SMTP connect error ";
  showErrorWiFi        = errores["wifi"]        | " No WiFi error ";
  fechaSMTP            = errores["fechaSMTP"]   | "-";
  fechaEnvio           = errores["fechaEnvio"]  | "-";
  fechaWiFi            = errores["fechaWiFi"]   | "-";
  smtpCount            = errores["smtpCount"]   | 0;
  envioCount           = errores["envioCount"]  | 0;
  wifiCount            = errores["wifiCount"]   | 0;
  snprintf(errorBuffer, sizeof(errorBuffer),
         "• Conexión SMTP: %s (x%d, %s)\n"
         "• Envío de correo: %s (x%d, %s)\n"
         "• Error WiFi: %s (x%d, %s)",
         showErrorMailConnect.c_str(), smtpCount, fechaSMTP.c_str(),
         showErrorMail.c_str(), envioCount, fechaEnvio.c_str(),
         showErrorWiFi.c_str(), wifiCount, fechaWiFi.c_str());
  showErrorSummary = String(errorBuffer);
}
void printDailyData() {
  File file = LittleFS.open("/data.json", "r");
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
  File file = LittleFS.open("/data.json", "r");
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
  if (data.isNull()) {
    if (debug) Serial.println("⚠️ Sección 'data' no encontrada.");
    return "";
  }
  String result = "";
  char lineBuffer[128];
  for (int day = 1; day <= 31; day++) {
    char dateKey[11];
    snprintf(dateKey, sizeof(dateKey), "%04d-%02d-%02d", year, month, day);
    if (data[dateKey].isNull()) continue;
    JsonObject dayData = data[dateKey];
    // Solo usamos los datos del día si existen
    bool hasSubstrate = !dayData["substrate"].isNull();
    bool hasHumidity = !dayData["humidity"].isNull();
    bool hasTemp     = !dayData["temp"].isNull();
    bool hasDrip     = !dayData["drip"].isNull();
    if (!hasSubstrate && !hasHumidity && !hasTemp && !hasDrip) continue;
    int humidity = hasHumidity ? dayData["humidity"].as<int>() : -1;
    int temp     = hasTemp     ? dayData["temp"].as<int>()     : -1;
    int substrate= hasSubstrate? dayData["substrate"].as<int>(): -1;
    bool drip    = hasDrip     ? dayData["drip"].as<bool>()    : false;
    snprintf(lineBuffer, sizeof(lineBuffer),
             "🗓 Día %d | 🚿: %s | 🌱: %s | 💧: %s | 🌡: %s\n",
             day,
             drip ? "Sí" : "No",
             hasSubstrate ? String(substrate).c_str() : "-",
             hasHumidity  ? String(humidity).c_str()  : "-",
             hasTemp      ? String(temp).c_str()      : "-");

    if (debug) Serial.print(lineBuffer);
    result += lineBuffer;
  }
  return result;
}
