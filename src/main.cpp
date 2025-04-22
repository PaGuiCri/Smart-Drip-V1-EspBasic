#include <Arduino.h>
#include <SimpleDHT.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP_Mail_Client.h>
#include <ESP32Time.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h> // Biblioteca para obtener detalles de la memoria
#include "storageManager.h"
#include "configManager.h"

/* WiFi */
const int MAX_CONNECT = 10;
unsigned long lastConnectionTry = 0;
const unsigned long tryInterval = 3600000;  // 1 hora en milisegundos
wl_status_t state;
void initWiFi();
void handleWiFiReconnection();
/* Función para calcular CRC32 */
uint32_t crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFF;
  while (length--) {
    uint8_t byte = *data++;
    crc = crc ^ byte;
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
  }
  return ~crc;
}
/* Email Config */
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465 
#define AUTHOR_EMAIL "falder24@gmail.com"
#define AUTHOR_PASSWORD "kcjbfgngmfgkcxtw"
SMTPSession smtp;
void smtpCallback(SMTP_Status status);
void setupMail(SMTP_Message& msg, const char* subject);
void mailSmartDripOn();                     // mail proceso de riego iniciado
void mailSmartDripOff();                    // mail proceso de riego finalizado
void mailStartSystem();                     // mail inicio sistema
void mailErrorValve();                      // mail error en electroválvula
void mailErrorDHT11();                      // mail error en sensor DHT11
void mailErrorSensorHigro();                // mail error en sensor higrometro
void mailActiveSchedule();                  // mail horario de riego activo
void mailNoActiveSchedule();                // mail horario de riego no activo
void mailAnnualReport();
ESP_Mail_Session session;
SMTP_Message mailStartSDS;
SMTP_Message mailDripOn;
SMTP_Message mailDripOff;
SMTP_Message mailErrValve;
SMTP_Message mailErrorFlowSensor;
SMTP_Message mailErrorDHT;
SMTP_Message mailErrorHigro;
SMTP_Message mailActivSchedule; 
SMTP_Message mailNoActivSchedule;
SMTP_Message mailAnualReport;
bool debugSmtp = false;
bool mailDripOnSended = false;
bool mailDripOffSended = false;
bool mailErrorValveSended = false;
bool mailErrorDHTSended = false;
bool mailErrorHigroSended = false;
bool mailAnnualReportSended = false;
bool mailActiveScheduleCheck = false;
bool mailNoActiveScheduleCheck = false;
bool mailStartSystemActive = true;
bool mailActiveScheduleActive = true;
bool mailNoActiveScheduleActive = true;
bool mailSmartDripOnActive = true;
bool mailSmartDripOffActive = true;
bool mailAnnualReportActive = true;
String showErrorMail, showErrorMailConnect, showErrorWiFi, showErrorSummary, fechaSMTP, fechaEnvio, fechaWiFi, finalMessage = "";
char errorMailConnect[256], errorMail[256], errorBuffer[512], textMsg[4800];
int smtpCount, envioCount, wifiCount = 0;
/* Open/Close Solenoid Valve  */
void openDripValve();
void closeDripValve();
void stopPulse();
void closeValveError();
/* Define NTP Client to get time */
ESP32Time rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
void NTPsincro();
void saveLastSyncTime(time_t timestamp);
time_t getLastSyncTime();
/* Variables to save date and time */
String nowTime = "";
String date = "";
String startTime = "08:00";
String endTime = "10:30";
String startHourStr, startMinuteStr, endHourStr, endMinuteStr, dataMonthlyMessage;
int startHour, startMinute, endHour, endMinute;
int currentHour, currentMinute, currentDay, currentMonth, currentYear, lastDay, lastDrip, lastDayDrip, counterDripDays, lastCheckedDay;
int emailSendHour = 9;        // Hora del día en que se enviará el correo (formato 24 horas)
bool emailSentToday = false;   // Variable para asegurarnos de que solo se envíe una vez al día
bool pendingStore = false;
bool autoCleanAnnualData = false;
int lastYearCleaned = 0;
bool debugPrintJson = false;   // 🔁 Actívalo cuando quieras ver en el Serial los datos del mes
void extractTimeValues();
void showMemoryStatus();
void loadErrorLogFromJson();  
void clearOldDataIfNewYear();
void createAndVerifyID();
void dumpDataJsonToSerial();
/* NTP server config */
const char* ntpServer = "hora.roa.es"; // Servidor NTP para sincronizar la hora
/* Terminal configuration for hygrometer and DHT11 */
void getDHTValues();               // Método para obtener los valores del sensor DHT11
void getHigroValues();             // Método para obtener los valores del sensor higrómetro
void handleDrip();                 // Método para el manejo de los procesos de riego
void handleScheduleDrip();         // Método para el manedo del riego dentro del horario activo
void handleOutOfScheduleDrip();    // Método para el manejo del riego fuera de horario activo
void manageScheduleStatus();       // Método para el manejo del estado del horario de riego
void finalizeDrip();               // Método para el manejo de la finalización del proceso de riego
String getMonthName(int month);    // Método para obtener el nombre del mes en español
#define PinHigro 34  // Nueva configuración de pines antes 34. volvemos al pin 34 desde el 13
#define PinDHT 4     // El pin del sensor DHT tiene q ser el 4 si se trabaja con la biblioteca SimpleDHT
float temp, humidity = 0;   // Variables para almacenar los datos recibidos del sensor DHT11
SimpleDHT11 DHT(PinDHT);
unsigned long TiempoDHT = 0;
#define SampleDHT 1200
int higroValue, dryValue, wetValue = 0;
int substrateHumidity = 0;
int counter = 0;
bool outputEstatus = false;
const int dry = 460;
const int wet = 0;                   // Si se incrementa, el máximo (100%) sera mayor y viceversa
//int dry, wet = 0;            // Variables para almacenar los valores límites del sensor higrómetro
/* Variables for flow calculation */
volatile int pulses = 0;
float caudal = 0.0;
float waterVolume = 0.0; // *** redefinir variables de caudal de agua
float totalLitros = 0.0;
unsigned long oldTime = 0;
bool flowMeterEstatus = false;
bool flowSensorEnabled = false;  // Habilita o deshabilita el sensor de flujo de caudal
void flowMeter();
void pulseCounter(){
  pulses++;
}
/* Checking Active Schedule */
bool withinSchedule = false;
bool isWithinSchedule(int currentHour, int currentMinute);
unsigned long currentMillis, previousMillis = 0;
const unsigned long intervalDay = 86400000; // 1 día en milisegundos (24 horas)
size_t freeHeap = 0;
size_t totalHeap = 0;
size_t usedHeap = 0;
/* Pin Config */
#define dripValveVin1 27             // Nueva configuración de pines antes 32. Salida Electroválvula 1
#define dripValveGND1 26             // Nueva configuración de pines antes 25. Salida Electroválvula 1
#define dripValveVin2 25             // Segunda válvula opcional
#define dripValveGND2 33             // Segunda válvula opcional
#define flowSensor  13               // Nueva configuración de pines antes 20 pendiente test pin 13
/* Drip Control Variables */
int dripHumidity = 0;                // Indica el límite de humedad del sustrato dentro del proceso de riego
int dripTimeLimit = 5;               // Duración del riego en minutos
int dripHumidityLimit = 45;          // Indica el límite de humedad para activar el riego
int remainingMinutes = 0;            // Variable para almacenar los minutos restantes de riego
int remainingSeconds = 0;            // Variable para almacenar los segundos restantes de riego
unsigned long startDripTime = 0;     // Marca el tiempo de inicio del riego en milisegundos
unsigned long dripTime = 0;          // Indica el tiempo de riego en milisegundos dentro del proceso de riego activo
unsigned long elapsedTime = 0;       // Tiempo transcurrido desde el inicio del riego en milisegundos
unsigned long remainingTime = 0;     // Tiempo restante para finalizar el riego en milisegundos
bool dripValve= false;               // Indica si la electroválvula está abierta o cerrada
bool activePulse = false;            // Indica si el pulso de apertura o cierre de la válvula está activo
bool dhtOk, dhtOkCheck = false;      // Indica si el sensor DHT11 está funcionando correctamente
bool dripActived = false;            // Indica si el riego fue activado para almacenar la información diaria
bool checkTimer = false;             // Indica si hay un proceso de riego en marcha
/* Pulse Variables */
const unsigned long pulseTime = 100; // Duración del pulso en milisegundos = 50ms
unsigned long startTimePulse = 0;
int closeValveCounter = 10;
bool mountLittleFS(bool allowFormat = false) {
  Serial.println("🔁 Intentando montar LittleFS...");
  if (!LittleFS.begin(allowFormat)) {
    Serial.println("❌ No se pudo montar LittleFS.");
    if (!allowFormat) {
      Serial.println("➡️ Puedes forzar el formateo llamando a mountLittleFS(true).");
    }
    return false;
  }
  Serial.println("✔ LittleFS montado correctamente.");
  Serial.printf("📦 Tamaño total: %lu bytes\n", LittleFS.totalBytes());
  Serial.printf("📂 Espacio usado: %lu bytes\n", LittleFS.usedBytes());
  return true;
}
void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("🔁 [SETUP] Iniciado tras reinicio o arranque...");
  // Montar LittleFS
  if (!mountLittleFS(false)) {
    Serial.println("⚠️ Intentando montar con formateo (recuperación)...");
    if (mountLittleFS(true)) {
      Serial.println("⚠️ LittleFS fue formateado por recuperación.");
    } else {
      Serial.println("❌ Falló incluso tras formatear. Problema grave.");
      return;
    }
  }
  // Inicializaciones esenciales
  checkStorageFile();           // Asegura que data.json exista
  loadConfigFromJson();         // Cargar configuración desde config.json
  createAndVerifyID();          // Crear y guardar ID único si no existe
  loadErrorLogFromJson();       // Cargar errores anteriores desde JSON
  initWiFi();                   // Conexión WiFi
  // 🛠 Solo si necesitas el backup (descomentalo si hace falta)
  //if (Serial) {
  //   delay(2000);
  //   dumpDataJsonToSerial();
  //}
  // Mostrar datos cargados
  Serial.println("🔧 Configuración inicial finalizada:");
  Serial.print("🆔 ID SmartDrip: ");
  Serial.println(idSDHex);
  Serial.print("👤 Usuario: ");
  Serial.println(idUser);
  Serial.print("📍 Dispositivo: ");
  Serial.println(idSmartDrip);
  Serial.print("🕒 Hora actual: ");
  Serial.println(nowTime);
  Serial.print("📅 Fecha actual: ");
  Serial.println(date);
  // Configurar resolución ADC
  analogReadResolution(9);
  // Configurar pines de riego
  pinMode(dripValveVin1, OUTPUT);  digitalWrite(dripValveVin1, LOW);
  pinMode(dripValveGND1, OUTPUT);  digitalWrite(dripValveGND1, LOW);
  pinMode(flowSensor, INPUT);
  attachInterrupt(digitalPinToInterrupt(flowSensor), pulseCounter, FALLING); // Sensor flujo
  // Configuración de SMTP
  if (debugSmtp) {
    smtp.debug(1);
    smtp.callback(smtpCallback);
  }
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";
  // Crear mails
  setupMail(mailStartSDS,           "Estado ESP32 Smart Drip");
  setupMail(mailDripOn,             "Inicio Riego Smart Drip");
  setupMail(mailDripOff,            "Fin Riego Smart Drip");
  setupMail(mailErrValve,           "Estado válvula de Smart Drip");
  setupMail(mailErrorFlowSensor,    "Estado sensor de flujo");
  setupMail(mailErrorDHT,           "Estado sensor medio ambiente");
  setupMail(mailErrorHigro,         "Estado sensor higro");
  setupMail(mailActivSchedule,      "Horario de riego activo");
  setupMail(mailNoActivSchedule,    "Horario de riego NO activo");
  setupMail(mailAnualReport,        "Estado informe anual");
  stopPulse();                       // Asegurar que no haya pulso activo
  getHigroValues();                  // Obtener primera lectura
  if (mailStartSystemActive) {
    showMemoryStatus();
    mailStartSystem();               // Enviar correo de inicio de sistema
  }
}
void loop() {
  handleWiFiReconnection();                                       // Verifica WiFi
  extractTimeValues();                                            // Extrae la hora y fecha actuales
  manageScheduleStatus();                                         // Gestionar estado según horario
  dripActived = checkTimer;                                       // Verificar si hay riego en curso
  if (currentDay != lastCheckedDay) {
    clearOldDataIfNewYear();
    mailAnnualReportSended = false;
    lastCheckedDay = currentDay;
  }
  // Guardado tras fin de horario activo, sin riego en curso      
  if (!withinSchedule && !checkTimer) {                           
    String dateKey = getCurrentDateKey();                         
    // Corregimos si endTime == 00:00                             
    if (endHour == 0 && endMinute == 0) {                         
      time_t now = time(nullptr) - 86400;                         
      struct tm* timeinfo = localtime(&now);                      
      char buffer[11];                                            
      snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", 1900 + timeinfo->tm_year, 1 + timeinfo->tm_mon, timeinfo->tm_mday);
      dateKey = String(buffer);                                   
    }
    if (!isDataStoredForDate(dateKey)) {                          
      getHigroValues();                                          
      getDHTValues();                                             
      storeOrUpdateDailyDataJson(currentDay, currentMonth, currentYear, substrateHumidity, humidity, temp, dripActived, true, dateKey);
      showMemoryStatus();
    }                                                             
  }
  // Si hay riego activo al final del horario, marcar para guardar después
  if (!withinSchedule && checkTimer) {                            
    if (!pendingStore) {
      Serial.println("📌 Riego activo al final del horario. Marcamos para guardar más tarde.");
      pendingStore = true;
    }
  }
  if (flowSensorEnabled) {
    flowMeter();                                                   // Monitorización continua del flujo de agua
  }
  finalizeDrip();                                                  // Cierre de proceso de riego si ha terminado
  Serial.print("📡 Error SMTP: ");
  Serial.println(showErrorMailConnect);
  Serial.print("📬 Error envío mail: ");
  Serial.println(showErrorMail);
}
/* Get Time */
void extractTimeValues() {
  currentHour = rtc.getHour(true);            // Obtenemos la hora actual, sólo el dato de la hora (0-23). True para formato 24h  
  currentMinute = rtc.getMinute();            // Obtenemos los minutos actuales
  currentDay = rtc.getDay();                  // Obtenemos el número de día del mes (1-31)
  currentMonth = rtc.getMonth() + 1;          // Obtenemos el número de mes (1-12)
  currentYear = rtc.getYear();                // Obtenemos el año actual
  startHourStr = startTime.substring(0, 2);   
  startMinuteStr = startTime.substring(3, 5);
  endHourStr = endTime.substring(0, 2);
  endMinuteStr = endTime.substring(3, 5);
  startHour = startHourStr.toInt();
  startMinute = startMinuteStr.toInt();
  endHour = endHourStr.toInt();
  endMinute = endMinuteStr.toInt();
  Serial.print("Hora actual: ");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.print(currentMinute);
  Serial.println();
}
/* Checking active schedule */
bool isWithinSchedule(int currentHour, int currentMinute) {
    int currentTotalMinutes = (currentHour * 60) + currentMinute;
    int startTotalMinutes = (startHour * 60) + startMinute;
    int endTotalMinutes = (endHour * 60) + endMinute;
    if (startTotalMinutes < endTotalMinutes) {
        // Caso normal: el rango no cruza la medianoche
        return (currentTotalMinutes >= startTotalMinutes && currentTotalMinutes <= endTotalMinutes);
    } else {
        // Caso especial: el rango cruza la medianoche
        return (currentTotalMinutes >= startTotalMinutes || currentTotalMinutes <= endTotalMinutes);
    }
}
void manageScheduleStatus() {
  // Actualizar si estamos o no dentro del horario activo
  withinSchedule = isWithinSchedule(currentHour, currentMinute);

  if (withinSchedule) {
    handleScheduleDrip();  // Dentro del horario activo
  } else {
    handleOutOfScheduleDrip();  // Fuera del horario activo
  }
}
void handleScheduleDrip(){
  getHigroValues();
  mailNoActiveScheduleCheck = false;
  Serial.println("Active irrigation schedule");
  if (!mailActiveScheduleCheck && mailActiveScheduleActive) {  
    mailActiveSchedule();                 // Envío mail horario de riego activo
  }
  if (!checkTimer) {                                        // Si el temporizador no está habilitado, reiniciar los valores predeterminados de riego
    dripTime = dripTimeLimit * 60000;                       // Indica el tiempo de riego en milisegundos según el tiempo límite marcado por el usuario
    dripHumidity = dripHumidityLimit;
    Serial.println("Timer disabled");
  } else {                                                  // Si el temporizador está habilitado, indicar que el proceso de riego está en curso
    Serial.println("Timer enabled");
    Serial.println("Drip process underway");
  }
  if (substrateHumidity > dripHumidity) {    
      if (!checkTimer) {
        Serial.println("Wet substrate, no need to water");
      }else {
        Serial.println("Drip process already in progress");
      }
  } else {
      Serial.println("Dry substrate, needs watering");
      handleDrip();
  }
}
/* Handle Drip Process */
void handleDrip() {
  if (!checkTimer) {
    startDripTime = millis();
    checkTimer = true;
    mailDripOffSended = false;
    openDripValve();
    Serial.println("🚿 Riego iniciado");
  } else {
    if (!mailDripOnSended && mailSmartDripOnActive) {
      mailSmartDripOn();
    }
    Serial.printf("🕒 Tiempo restante: %d min, %d seg\n", remainingMinutes, remainingSeconds);
    Serial.printf("💧 Caudal: %.2f L/min - Total: %.2f L\n", caudal, totalLitros);
  }
  elapsedTime = millis() - startDripTime;
  remainingTime = dripTime - elapsedTime;
  remainingMinutes = remainingTime / 60000;
  remainingSeconds = (remainingTime % 60000) / 1000;
}
/* Handle Out of Schedule Irrigation */
void handleOutOfScheduleDrip() {
  Serial.println("Fuera de horario de riego");
  Serial.print("Caudal de riego fuera de horario: ");  
  Serial.println(caudal);
  mailActiveScheduleCheck = false;
  if (!mailNoActiveScheduleCheck && mailNoActiveScheduleActive) {
    mailNoActiveSchedule();
  }
  if (!dripValve && caudal != 0) {
    if (closeValveCounter != 0) {
      closeValveError();
      Serial.println("Emergency valve closure");
    }
    if (flowSensorEnabled && flowMeterEstatus && !mailErrorValveSended && closeValveCounter == 0) {
      mailErrorValve();
      Serial.println("Email de Error en válvula enviado");
      closeValveCounter = 10;
    }
  }
}
/* Finalize Irrigation */
void finalizeDrip() {
  if (checkTimer) {
    elapsedTime = millis() - startDripTime;
    remainingTime = dripTime - elapsedTime;
    remainingMinutes = remainingTime / 60000;
    remainingSeconds = (remainingTime % 60000) / 1000;
    Serial.print("⏳ Riego en curso. Tiempo restante: ");
    Serial.print(remainingMinutes);
    Serial.print(" minutos, ");
    Serial.print(remainingSeconds);
    Serial.println(" segundos.");
    if (elapsedTime >= dripTime) {
      Serial.println("✅ Proceso de riego finalizado.");
      if (dripValve == true) {
        closeDripValve();
        checkTimer = false;
        mailDripOnSended = false;
        if (!mailDripOffSended && mailSmartDripOffActive) {
          mailSmartDripOff();
        }
        getHigroValues();  // Actualizar valores del sustrato tras el riego
        if (pendingStore) {
          Serial.println("📥 Guardado pendiente detectado. Procediendo al almacenamiento...");
          // Corregimos fecha si el horario terminó a las 00:00
          String dateKey = getCurrentDateKey();
          if (startHour == 0 && startMinute == 0) {
            time_t now = time(nullptr) - 86400; // Restamos un día en segundos
            struct tm* timeinfo = localtime(&now);
            char buffer[11];
            snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", 1900 + timeinfo->tm_year, 1 + timeinfo->tm_mon, timeinfo->tm_mday);
            dateKey = String(buffer);
          }
          storeOrUpdateDailyDataJson(currentDay, currentMonth, currentYear,
                                     substrateHumidity, humidity, temp,
                                     true, true, dateKey);
          showMemoryStatus();
          pendingStore = false;  // Limpiar flag
        }
      }
    }
  }
}

/* Getting Higro Measurements */
void getHigroValues(){
  higroValue = analogRead(PinHigro);
  substrateHumidity = map(higroValue, wet, dry, 100, 0);
  Serial.print("Valor leido en el sensor de humedad: ");
  Serial.println(higroValue);
  Serial.print("Valor humedad máxima: ");
  Serial.println(wet);
  Serial.print("Valor mínimo de humedad: ");
  Serial.println(dry);
  Serial.print("Substrate humidity: "); 
  Serial.print(substrateHumidity);
  Serial.println("% "); 
}
/* Getting DHT Measurements */
void getDHTValues(){
  if(DHT.read2(&temp, &humidity, NULL) == SimpleDHTErrSuccess) {
    Serial.println("DHT11 OK");
    dhtOk = true;
    if(dhtOk != dhtOkCheck){  // Añadida comprobación de estado del sensor
      Serial.println("Estado DHT11 ON actualizado");
    }
    TiempoDHT = millis();
    mailErrorDHTSended = false;
  }else{
    dhtOk = false;
    if(dhtOk != dhtOkCheck){
      Serial.println("Estado DHT11 OFF actualizado");
    }
    if(!mailErrorDHTSended){
      Serial.println("Se envía email DHT ERROR");
      //mailErrorDHT11();
    }
    Serial.println("Error DHT11");
    dhtOkCheck = dhtOk;
  }
}
/* Solenoid Valve Opening */
void openDripValve(){
  Serial.println("APERTURA DE VALVULA DE RIEGO");
  digitalWrite(dripValveVin1, HIGH);
  digitalWrite(dripValveGND1, LOW);
  activePulse = true;
  startTimePulse = millis();
  outputEstatus = digitalRead(dripValveVin1);
  Serial.println("VálvulaRiegoVin: " + outputEstatus);
  outputEstatus = digitalRead(dripValveGND1);
  Serial.println("VálvulaRiegoGND: " + outputEstatus);
  dripValve = true;
  Serial.println("Pulso de apertura Activo");
  delay(50);
  if (activePulse && (millis() - startTimePulse >= pulseTime)) {  
    stopPulse();
  } 
}
/* Solenoid Valve Closing */
void closeDripValve(){
  Serial.println("CIERRE DE VALVULA DE RIEGO");
  digitalWrite(dripValveVin1, LOW);
  digitalWrite(dripValveGND1, HIGH);
  activePulse = true;
  startTimePulse = millis();
  outputEstatus = digitalRead(dripValveVin1);
  Serial.println("VálvulaRiegoVin: " + outputEstatus);
  outputEstatus = digitalRead(dripValveGND1);
  Serial.println("VálvulaRiegoGND: " + outputEstatus);
  dripValve = false;
  Serial.println("Pulso de cierre Activo");
  delay(50);
  if (activePulse && (millis() - startTimePulse >= pulseTime)) {
    stopPulse();
  } 
}
/* Emergency Solenoid Valve Closure */
void closeValveError(){
  Serial.println("Cierre de válvula de riego de emergencia");
  digitalWrite(dripValveVin1, LOW);
  digitalWrite(dripValveGND1, HIGH);
  outputEstatus = digitalRead(dripValveVin1);
  Serial.print("VálvulaRiegoVin: " + outputEstatus);
  outputEstatus = digitalRead(dripValveGND1);
  Serial.print("VálvulaRiegoGND: " + outputEstatus);
  dripValve = false;
  startTimePulse = millis();
  activePulse = true;
  Serial.println("Pulso Activo");
  delay(50);
  if (activePulse && (millis() - startTimePulse >= pulseTime)) {  
    stopPulse();
  } 
  closeValveCounter--;
  Serial.print("Intentos de cierre de válvula de riego: ");
  Serial.println(closeValveCounter);
  delay(1000);
}
/* Disable Active Pulse */
void stopPulse(){
  digitalWrite(dripValveVin1, LOW);
  digitalWrite(dripValveGND1, LOW);
  activePulse = false;
  Serial.println("Corta corriente salidas electroválvula");
  outputEstatus = digitalRead(dripValveVin1);
  Serial.print("VálvulaRiegoVin: " + outputEstatus);
  Serial.println(outputEstatus);
  outputEstatus = digitalRead(dripValveGND1);
  Serial.print("VálvulaRiegoGND: " + outputEstatus);
  Serial.println("Pulso electroválvula no activo");
  delay(500);
}
/* Flow meter */
void flowMeter() {
  if ((millis() - oldTime) > 1000) {  // Actualización cada segundo
    detachInterrupt(digitalPinToInterrupt(flowSensor));
    caudal = pulses / 5.5;           // factor de conversión, siendo K=7.5 para el sensor de ½”, K=5.5 para el sensor de ¾” y 3.5 para el sensor de 1”
    pulses = 0;                      // Reinicia el contador de pulsos
    waterVolume = caudal / 60.0;     // Litros por segundo
    totalLitros += waterVolume;      // Incrementa el volumen total acumulado
    oldTime = millis();
    attachInterrupt(digitalPinToInterrupt(flowSensor), pulseCounter, FALLING);
    flowMeterEstatus = (caudal > 0);
    Serial.printf("💧 Caudal: %.2f L/min - Acumulado: %.2f L\n", caudal, totalLitros);
    Serial.println(flowMeterEstatus ? "✅ Sensor conectado" : "❌ Sensor desconectado");
  }
}
/* Create and Encrypt ID */
void createAndVerifyID() {
  Serial.println("🔎 [createAndVerifyID] Iniciando verificación de ID...");
  String macAddress = WiFi.macAddress();
  uint8_t macBytes[6];
  sscanf(macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
         &macBytes[0], &macBytes[1], &macBytes[2],
         &macBytes[3], &macBytes[4], &macBytes[5]);
  idNumber = crc32(macBytes, 6);
  String generatedID = String(idNumber, HEX);
  idSDHex = generatedID;
  File file = LittleFS.open("/config/config.json", "r");
  DynamicJsonDocument doc(1024);
  if (!file) {
    Serial.println("❌ No se pudo abrir config.json para validar ID");
    return;
  }
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.print("❌ Error leyendo config.json: ");
    Serial.println(error.c_str());
    Serial.println("🚫 No se continuará para evitar sobrescribir datos.");
    return;
  }
  if (!doc.containsKey("config") || !doc["config"].is<JsonObject>()) {
    Serial.println("⚠️ El campo 'config' no existe o es inválido. Abortando escritura.");
    return;
  }
  String storedID = doc["config"]["idSDHex"] | "";
  if (storedID == "") {
    Serial.println("📌 ID no encontrado en JSON. Guardando nuevo ID...");
  } else if (storedID != generatedID) {
    Serial.println("⚠️ ID en JSON no coincide con el generado.");
    Serial.printf("➡️ Corrigiendo: %s -> %s\n", storedID.c_str(), generatedID.c_str());
  } else {
    Serial.println("✅ ID en JSON verificado correctamente");
    return;
  }
  // ✅ Si llegamos aquí, es seguro modificar
  doc["config"]["idSDHex"] = generatedID;
  Serial.println("📤 [createAndVerifyID] JSON antes de guardar:");
  serializeJsonPretty(doc, Serial);
  file = LittleFS.open("/config/config.json", "w");
  if (!file) {
    Serial.println("❌ No se pudo abrir config.json para escritura");
    return;
  }
  serializeJsonPretty(doc, file);
  file.close();
  Serial.println("✅ ID actualizado en config.json correctamente.");
}
void dumpDataJsonToSerial() {
  File file = LittleFS.open("/data.json", "r");
  if (!file) {
    Serial.println("❌ No se pudo abrir data.json");
    return;
  }
  Serial.println("\n📤 [dumpDataJsonToSerial] Inicio del volcado de data.json:\n");
  while (file.available()) {
    Serial.write(file.read());  // Imprime el JSON crudo
  }
  file.close();
  Serial.println("\n📤 [dumpDataJsonToSerial] Fin del volcado.");
}

/* New Start WiFi */
void initWiFi() {
  if (ssid.isEmpty() || pass.isEmpty()) {
    Serial.println("⚠️ SSID o contraseña no definidos. No se intentará conectar a WiFi.");
    updateErrorLog("", "", "Credenciales vacías");
    return;
  }
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid.c_str(), pass.c_str());
  int tries = 0;
  state = WiFi.status();
  unsigned long initTime = millis();
  const unsigned long interval = 5000;
  const unsigned long waitTime = 15000;
  while (state != WL_CONNECTED && tries < MAX_CONNECT) {
    currentMillis = millis();
    if (currentMillis - initTime >= interval) {
      Serial.printf("...Intento %d a la red WiFi %s\n", tries + 1, ssid.c_str());
      if (state != WL_CONNECTED && (currentMillis - initTime >= waitTime)) {
        WiFi.reconnect();
        initTime = millis();  // Reiniciar contador
      } else {
        initTime = millis();
      }
      state = WiFi.status();
      tries++;
    }
  }
  if (state == WL_CONNECTED) {
    Serial.println("\n✅ Conexión WiFi exitosa!!!");
    Serial.print("📡 IP: ");
    Serial.println(WiFi.localIP());
    updateErrorLog("", "", "");  // Limpiamos errores WiFi anteriores
    NTPsincro();
  } else {
    Serial.println("\n❌ No se pudo conectar a la red WiFi.");
    String wifiError;
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:   wifiError = "SSID no disponible"; break;
      case WL_CONNECT_FAILED:  wifiError = "Fallo de autenticación"; break;
      case WL_DISCONNECTED:    wifiError = "Desconectado"; break;
      case WL_IDLE_STATUS:     wifiError = "Estado inactivo"; break;
      default:                 wifiError = "Error WiFi desconocido"; break;
    }
    updateErrorLog("", "", wifiError);
  }
}
/* Check WiFi Reconnection */
void handleWiFiReconnection() {
  if (millis() - lastConnectionTry >= tryInterval) {  // Comprobación de la conexión de la red WiFi cada hora
    lastConnectionTry = millis();                     // Actualizar el tiempo del último chequeo
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("Conexión WiFi perdida. Intentando reconectar...");
      initWiFi();
      updateErrorLog("wifi", "WiFi reconnection failed", getCurrentDateKey());
    }
    Serial.println("Conexión WiFi estable. ");
  }
}
/* Function to save the last synchronized time in NVS memory */
void saveLastSyncTime(time_t timestamp) {
  File file = LittleFS.open("/data.json", "r");
  DynamicJsonDocument doc(4096);
  if (file) {
    deserializeJson(doc, file);
    file.close();
  }
  doc["ultima_sincronizacion"] = (uint64_t)timestamp;
  file = LittleFS.open("/data.json", "w");
  serializeJsonPretty(doc, file);
  file.close();
  Serial.printf("🕒 Hora de sincronización guardada: %llu\n", (uint64_t)timestamp);
}
/* Function to retrieve the last synchronized time from NVS memory */
time_t getLastSyncTime() {
  File file = LittleFS.open("/data.json", "r");
  if (!file) return 0;
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) return 0;
  return doc["ultima_sincronizacion"] | 0;
}
/* NTP Sincronization with RTC */
void NTPsincro() {
  struct tm timeinfo;                                             // Estructura para almacenar la hora obtenida de NTP
  Serial.println("Intentando sincronizar con NTP...");
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "hora.roa.es", "ntp.ign.es", "es.pool.ntp.org");          // Configurar zona horaria de España (M3.5.0 = último domingo de marzo, M10.5.0/3 = último domingo de octubre)
  int attempts = 0;                                               // Contador de intentos
  const int maxAttempts = 5;                                      // Máximo número de intentos para sincronizar
  while (attempts < maxAttempts) {                                // Intentar obtener la hora desde el servidor NTP
      if (getLocalTime(&timeinfo)) {                              // Si la hora se obtiene correctamente...
          Serial.println("✔ Hora sincronizada con NTP:");
          Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
          rtc.setTimeStruct(timeinfo);                            // Configurar el RTC interno con la nueva hora
          time_t nowTime = rtc.getEpoch();                        // Obtener la hora actual en formato epoch
          saveLastSyncTime(nowTime);                              // Guardar la hora sincronizada en NVS
          return;
      }
      Serial.println("❌ Error al sincronizar con NTP. Reintentando..."); 
      attempts++;
      delay(2000);
  }
  Serial.println("⚠ No se pudo sincronizar con NTP. Usando la última hora guardada...");
  time_t lastSync = getLastSyncTime();                            // Recuperar la última hora sincronizada desde la memoria NVS
  if (lastSync > 0) {                                             // Si hay una hora almacenada en NVS...
      rtc.setTime(lastSync);                                      // Configurar el RTC con esa hora
      Serial.println("✔ Última hora recuperada de Preferences:");
      Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));
  } else {                                                        // Si nunca se ha sincronizado antes, no hay datos en NVS
      Serial.println("⚠ No hay hora previa almacenada. La hora será incorrecta hasta la próxima sincronización.");
  }
}
/* Show Memory Status */
void showMemoryStatus() {
  freeHeap = ESP.getFreeHeap();
  totalHeap = ESP.getHeapSize();
  usedHeap = totalHeap - freeHeap;
  Serial.println("----- Estado de la memoria -----");
  Serial.print("Memoria total: ");
  Serial.print(totalHeap);
  Serial.println(" bytes");
  Serial.print("Memoria usada: ");
  Serial.print(usedHeap);
  Serial.println(" bytes");
  Serial.print("Memoria libre: ");
  Serial.print(freeHeap);
  Serial.println(" bytes");
  Serial.println("--------------------------------");
}
void loadErrorLogFromJson() {
  File file = LittleFS.open("/data.json", "r");
  if (!file) {
    Serial.println("❌ No se pudo abrir data.json para leer errores");
    return;
  }
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.println("❌ Error al leer errores desde data.json");
    return;
  }
  JsonObject errores = doc["errores"];
  showErrorMail        = errores["envio"]       | " No mail errors ";
  showErrorMailConnect = errores["smtp"]        | " No SMTP connect error ";
  showErrorWiFi        = errores["wifi"]        | " No WiFi error ";
  fechaSMTP            = errores["fechaSMTP"]   | "-";
  fechaEnvio           = errores["fechaEnvio"]  | "-";
  fechaWiFi            = errores["fechaWiFi"]   | "-";
  smtpCount            = errores["smtpCount"]   | 0;
  envioCount           = errores["envioCount"]  | 0;
  wifiCount            = errores["wifiCount"]   | 0;
  // Mostrar en el monitor serial
  Serial.println("📩 Error almacenado - Envío mail:");
  Serial.printf("• %s (x%d) | Última vez: %s\n", showErrorMail.c_str(), envioCount, fechaEnvio.c_str());
  Serial.println("📡 Error almacenado - Conexión SMTP:");
  Serial.printf("• %s (x%d) | Última vez: %s\n", showErrorMailConnect.c_str(), smtpCount, fechaSMTP.c_str());
  Serial.println("📶 Error almacenado - WiFi:");
  Serial.printf("• %s (x%d) | Última vez: %s\n", showErrorWiFi.c_str(), wifiCount, fechaWiFi.c_str());
  // Resumen para insertar en los mails
  snprintf(errorBuffer, sizeof(errorBuffer),
         "• Conexión SMTP: %s (x%d, %s)\n"
         "• Envío de correo: %s (x%d, %s)\n"
         "• Error WiFi: %s (x%d, %s)",
         showErrorMailConnect.c_str(), smtpCount, fechaSMTP.c_str(),
         showErrorMail.c_str(), envioCount, fechaEnvio.c_str(),
         showErrorWiFi.c_str(), wifiCount, fechaWiFi.c_str());
  showErrorSummary = String(errorBuffer);
}
void clearOldDataIfNewYear() {
  if (!autoCleanAnnualData) return;
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  int currentYearToKeep = 1900 + timeinfo->tm_year;
  if (lastYearCleaned == currentYearToKeep) return;  // Ya se hizo limpieza este año
  File file = LittleFS.open("/data.json", "r");
  if (!file) {
    Serial.println("❌ No se pudo abrir data.json para limpieza");
    return;
  }
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.println("❌ Error al deserializar data.json");
    return;
  }
  JsonObject data = doc["data"];
  if (!data) return;
  JsonObject filtered = doc.createNestedObject("data_filtrada");
  for (JsonPair kv : data) {
    String key = kv.key().c_str();  // Ej: "2023-12-31"
    int year = key.substring(0, 4).toInt();
    if (year >= currentYearToKeep) {
      filtered[key] = kv.value();
    }
  }
  doc["data"] = filtered;
  doc.remove("data_filtrada");
  file = LittleFS.open("/data.json", "w");
  if (serializeJsonPretty(doc, file) == 0) {
    Serial.println("❌ Error al escribir datos tras limpieza anual");
  } else {
    Serial.println("🧹 Datos antiguos eliminados correctamente");
    lastYearCleaned = currentYearToKeep;
  }
  file.close();
  if (mailAnnualReportActive && !mailAnnualReportSended) {
    mailAnnualReport();              // Enviar el informe anual
    mailAnnualReportSended = true;   // Evitar reenvío
  }
}
/* Mail Setup */
void setupMail(SMTP_Message& msg, const char* subject) {
  msg.sender.name = "Smart Drip System";
  msg.sender.email = AUTHOR_EMAIL;
  msg.subject = subject;
  msg.addRecipient("Pablo", "falder24@gmail.com");
}
/* Mail Start System */
void mailStartSystem() {
  snprintf(textMsg, sizeof(textMsg),
        "📡 *SmartDrip en línea*\n"
        "🔹 Dispositivo: SmartDrip%s\n"
        "🔹 Usuario: %s (%s)\n\n"
        "🟢 El sistema se ha conectado correctamente a la red WiFi.\n"
        "⚙️ Configuración actual:\n"
        "• Tiempo de riego: %d min\n"
        "• Límite de humedad: %d%%\n"
        "• Horario de riego: %s - %s\n"
        "• Humedad actual del sustrato: %d%%\n\n"
        "✅ El sistema está listo y en funcionamiento."
        "⚠️ *Errores recientes en el sistema:*\n%s\n",
        idSmartDrip.c_str(), idUser.c_str(), idSDHex.c_str(),
        dripTimeLimit, dripHumidityLimit,
        startTime.c_str(), endTime.c_str(),
        substrateHumidity, showErrorSummary.c_str());
  finalMessage = String(textMsg);
  mailStartSDS.text.content = finalMessage.c_str();
  mailStartSDS.text.charSet = "us-ascii";
  mailStartSDS.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailStartSDS.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailStartSDS)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  }else {
    Serial.println("📩 Mail de inicio de sistema enviado.");
  }
  ESP_MAIL_PRINTF("🧠 Memoria tras envío: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
}
/* Mail Active Schedule */
void mailActiveSchedule() {
  nowTime = rtc.getTime();
  date = rtc.getDate();
  currentMonth = rtc.getMonth() + 1;
  String dateKey = getCurrentDateKey(); // YYYY-MM-DD
  bool datosGuardados = isDataStoredForDate(dateKey);
  String message = printMonthlyDataJson(currentMonth, currentYear);
  snprintf(textMsg, sizeof(textMsg),
        "🌿 *SmartDrip: Horario activo iniciado*\n"
        "🔹 Dispositivo: SmartDrip%s\n"
        "🔹 Usuario: %s (%s)\n\n"
        "🕒 Hora actual (RTC): %s | %s\n\n"
        "⚙️ *Configuración activa:*\n"
        "• Tiempo de riego: %d min\n"
        "• Límite de humedad: %d%%\n"
        "• Horario: %s - %s\n"
        "• Humedad del sustrato: %d%%\n\n"
        "📅 *Datos del día %d:*\n"
        "• Datos guardados: %s\n"
        "• ¿Se regó?: %s\n\n"
        "🗓 *Resumen del mes %d:*\n"
        "%s\n"
        "💾 *Estado de memoria LittleFS:*\n"
        "• Total: %d bytes\n"
        "• Usada: %d bytes\n"
        "• Libre: %d bytes\n\n"
        "⚠️ *Errores recientes en el sistema:*\n%s\n",
        idSmartDrip.c_str(), idUser.c_str(), idSDHex.c_str(),
        date.c_str(), nowTime.c_str(),
        dripTimeLimit, dripHumidityLimit,
        startTime.c_str(), endTime.c_str(),
        substrateHumidity,
        currentDay,
        datosGuardados ? "Sí" : "No",
        dripActived ? "Sí" : "No",
        currentMonth, message.c_str(),
        totalHeap, usedHeap, freeHeap,
        showErrorSummary.c_str());
  finalMessage = String(textMsg);
  mailActivSchedule.text.content = finalMessage.c_str();
  mailActivSchedule.text.charSet = "us-ascii";
  mailActivSchedule.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailActivSchedule.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailActivSchedule)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("✅ Correo enviado con éxito");
  }
  ESP_MAIL_PRINTF("💾 Memoria libre tras envío: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
  mailActiveScheduleCheck = true;
}
/* Mail No ACtive Schedule */
void mailNoActiveSchedule() {
  nowTime = rtc.getTime();
  date = rtc.getDate();
  currentMonth = rtc.getMonth() + 1;
  String message = printMonthlyDataJson(currentMonth, currentYear);  // Obtenemos el mensaje
  snprintf(textMsg, sizeof(textMsg),
        "🌙 *SmartDrip: Fin de horario activo*\n"
        "🔹 Dispositivo: SmartDrip%s\n"
        "🔹 Usuario: %s (%s)\n\n"
        "🕒 Hora actual (RTC): %s | %s\n\n"
        "⚙️ *Configuración activa:*\n"
        "• Tiempo de riego: %d min\n"
        "• Límite de humedad: %d%%\n"
        "• Horario: %s - %s\n"
        "• Humedad del sustrato: %d%%\n\n"
        "📅 *Datos del día %d:*\n"
        "• Sensores activos: %s\n"
        "• ¿Se regó?: %s\n\n"
        "🗓 *Resumen del mes %d:*\n"
        "%s\n"
        "💾 *Estado de memoria LittleFS:*\n"
        "• Total: %d bytes\n"
        "• Usada: %d bytes\n"
        "• Libre: %d bytes\n\n"
        "⚠️ *Errores recientes en el sistema:*\n%s\n",
        idSmartDrip.c_str(), idUser.c_str(), idSDHex.c_str(),
        date.c_str(), nowTime.c_str(),
        dripTimeLimit, dripHumidityLimit,
        startTime.c_str(), endTime.c_str(),
        substrateHumidity,
        currentDay,
        (substrateHumidity > -1) ? "Sí" : "No",
        dripActived ? "Sí" : "No",
        currentMonth, message.c_str(),
        totalHeap, usedHeap, freeHeap,
        showErrorSummary.c_str());
  finalMessage = String(textMsg);                         
  mailNoActivSchedule.text.content = finalMessage.c_str();
  mailNoActivSchedule.text.charSet = "us-ascii";
  mailNoActivSchedule.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailNoActivSchedule.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailNoActivSchedule)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📧 Correo enviado con éxito");
  }
  ESP_MAIL_PRINTF("🧠 Liberar memoria: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
  mailNoActiveScheduleCheck = true;
}
/* Mail Drip On */
void mailSmartDripOn() {
  nowTime = rtc.getTime();
  date = rtc.getDate();
  snprintf(textMsg, sizeof(textMsg),
        "💧 *Riego iniciado correctamente*\n"
        "🔹 Dispositivo: SmartDrip%s\n"
        "🔹 Usuario: %s (%s)\n\n"
        "🗓 Fecha: %s\n"
        "🕒 Hora de activación: %s\n\n"
        "⚙️ *Parámetros del riego:*\n"
        "• Duración: %d min\n"
        "• Límite de humedad: %d%%\n"
        "• Humedad actual del sustrato: %d%%\n",
        idSmartDrip.c_str(), idUser.c_str(), idSDHex.c_str(),
        date.c_str(), nowTime.c_str(),
        dripTimeLimit,
        dripHumidity,
        substrateHumidity);
  finalMessage = String(textMsg);
  mailDripOn.text.content = finalMessage.c_str();
  mailDripOn.text.charSet = "us-ascii";
  mailDripOn.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailDripOn.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailDripOn)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📩 Mail de inicio de riego enviado.");
  }
  ESP_MAIL_PRINTF("🧠 Memoria tras envío: %d\n", MailClient.getFreeHeap());
  mailDripOnSended = true;
  smtp.closeSession();
}
/* Mail Drip Off */
void mailSmartDripOff() {
  nowTime = rtc.getTime();
  date = rtc.getDate();
  snprintf(textMsg, sizeof(textMsg),
        "💧 *Riego finalizado correctamente*\n"
        "🔹 Dispositivo: SmartDrip%s\n"
        "🔹 Usuario: %s (%s)\n\n"
        "🗓 Fecha: %s\n"
        "🕒 Hora de finalización: %s\n\n"
        "⚙️ *Parámetros del riego:*\n"
        "• Duración programada: %d min\n"
        "• Límite de humedad: %d%%\n\n"
        "🌱 *Lecturas tras el riego:*\n"
        "• Humedad sustrato final: %d%%\n"
        "• Humedad ambiental: %d%%\n"
        "• Temperatura ambiente: %d°C\n",
        idSmartDrip.c_str(), idUser.c_str(), idSDHex.c_str(),
        date.c_str(), nowTime.c_str(),
        dripTimeLimit,
        dripHumidity,
        substrateHumidity,
        humidity,
        temp);
  finalMessage = String(textMsg);
  mailDripOff.text.content = finalMessage.c_str();
  mailDripOff.text.charSet = "us-ascii";
  mailDripOff.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailDripOff.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailDripOff)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📩 Mail de fin de riego enviado.");
  }
  ESP_MAIL_PRINTF("🧠 Memoria tras envío: %d\n", MailClient.getFreeHeap());
  mailDripOffSended = true;
  smtp.closeSession();
}
/* Mail Annual Report */
void mailAnnualReport() {
  if (!mailAnnualReportActive || mailAnnualReportSended) return;
  int currentYear = rtc.getYear();
  String fullReport = "";
  for (int m = 1; m <= 12; m++) {
    String monthName = getMonthName(m);
    String data = printMonthlyDataJson(m, currentYear, false);
    if (data != "") {
      fullReport += "📅 ";
      fullReport += monthName;
      fullReport += ":\n";
      fullReport += data;
      fullReport += "\n";
    }
  }
  snprintf(textMsg, sizeof(textMsg),
           "📩 *Informe anual de Smart Drip*\n\n"
           "📌 **Dispositivo:** %s\n"
           "👤 **Usuario:** %s\n"
           "🔢 **ID Smart Drip:** %s\n"
           "📅 **Año:** %d\n\n"
           "%s",
           idSDHex.c_str(),
           idUser.c_str(),
           idSmartDrip.c_str(),
           currentYear,
           fullReport.c_str());
  finalMessage = String(textMsg);
  mailAnualReport.sender.name = "Smart Drip System";
  mailAnualReport.sender.email = AUTHOR_EMAIL;
  mailAnualReport.subject = "📊 Informe Anual Smart Drip";
  mailAnualReport.addRecipient("Pablo", "falder24@gmail.com");
  mailAnualReport.text.content = finalMessage.c_str();
  mailAnualReport.text.charSet = "us-ascii";
  mailAnualReport.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailAnualReport.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailAnualReport)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📧 Informe anual enviado correctamente");
    mailAnnualReportSended = true;
  }
  smtp.closeSession();
}
/* Mail Solenoid Valve Error */
void mailErrorValve(){
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "Error en la electroválvula de riego del Smart Drip%s\n"
           "Se detiene el proceso de riego automático. \n"
           "Los sensores indican que el agua continúa fluyendo. \n"
           "Por favor revise la instalación lo antes posible.",
           idSDHex.c_str(),                // ID del SD en formato string
           idUser.c_str(),                 // ID del usuario
           idSmartDrip.c_str());           // ID del dispositivo SmartDrip
  finalMessage = String(textMsg);
  mailErrValve.text.content = finalMessage.c_str();
  mailErrValve.text.charSet = "us-ascii";
  mailErrValve.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrValve.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailErrValve)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  mailErrorValveSended = true;
  smtp.closeSession();
} 
/* Mail DHT Sensor Error */
void mailErrorDHT11(){
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "El sensor de datos ambientales del Smart Drip %s está desconectado o dañado \n"
           "Proceda a su inspección o llame al servicio técnico \n",
           idSDHex.c_str(),                // ID del SD en formato string
           idUser.c_str(),                 // ID del usuario
           idSmartDrip.c_str());           // ID del dispositivo SmartDrip
  finalMessage = String(textMsg);
  mailErrorDHT.text.content = finalMessage.c_str();
  mailErrorDHT.text.charSet = "us-ascii";
  mailErrorDHT.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrorDHT.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailErrorDHT)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap());
  mailErrorDHTSended = true;
  smtp.closeSession();
}
/* Mail Hygro Error */
void mailErrorSensorHigro(){
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "El sensor de humedad del sustrato del Smart Drip%s está fuera de rango o dañado. \n"
           "Se recomienda recalibración \n"
           "Proceda a su inspección o llame al servicio técnico \n",
           idSDHex.c_str(),                // ID del SD en formato string
           idUser.c_str(),                 // ID del usuario
           idSmartDrip.c_str());           // ID del dispositivo SmartDrip
  finalMessage = String(textMsg);
  mailErrorHigro.text.content = finalMessage.c_str();
  mailErrorHigro.text.charSet = "us-ascii";
  mailErrorHigro.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrorHigro.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailErrorHigro)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  mailErrorHigroSended = true;
  smtp.closeSession();
}
/* Check Mail Callback */
void smtpCallback(SMTP_Status status){
  Serial.println(status.info());
  if(status.success()){
    Serial.println("......................");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    Serial.println("......................");
    struct tm dt;
    for(size_t i = 0; i < smtp.sendingResult.size(); i++){
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);
      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipent: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject);
    }
    Serial.println(".....................\n");
  }
}
/* Get Month Name */
String getMonthName(int month) {
  const char* months[] = {"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
                          "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};
  return (month >= 1 && month <= 12) ? String(months[month - 1]) : "Desconocido";
}
