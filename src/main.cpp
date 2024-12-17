#include <Arduino.h>
#include <SimpleDHT.h>
#include <NTPClient.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <ESP_Mail_Client.h>
#include <ESP32Time.h>
#include <HTTPClient.h>
/* WiFi */
//#define SSID "MOVISTAR_D327_EXT" // Cambio Wifi a red casa Salva antes MiFibra-21E0_EXT...DIGIFIBRA-HNch...MOVISTAR_D327_EXT
//#define PASS "iMF5HSG35242K9G4GRUr" //Cambio Wifi a red casa Salva antes 2SSxDxcYNh.....iMF5HSG35242K9G4GRUr
#define SSID "MiFibra-21E0_EXT"
#define PASS "2SSxDxcYNh"
uint32_t idNumber = 0;    // id Smart Drip crc32
String idSDHex = "";      //id Smart Drip Hexadecimal
String idSmartDrip = " Pablo Terraza ";   //id Smart Drip Usuario
String idUser = " PabloG ";   //id usuario
const int MAX_CONNECT = 10;
unsigned long lastConnectionTry = 0;
const unsigned long tryInterval = 3600000;  // 1 hora en milisegundos
wl_status_t state;
void InitWiFi();
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
void mailSmartDripOn();                // mail proceso de riego iniciado
void mailStartSystem();                // mail inicio sistema
void mailErrorValve();                 // mail error en electroválvula
void mailErrorDHT11();                 // mail error en sensor DHT11
void mailErrorSensorHigro();           // mail error en sensor higrometro
void mailActiveSchedule();             // mail horario de riego activo
void mailNoActiveSchedule();           // mail horario de riego no activo
void mailMonthData(String message);    // mail datos de riego mensual
void mailCalibrateSensor();
ESP_Mail_Session session;
SMTP_Message mailStartSDS;
SMTP_Message mailDripOn;
SMTP_Message mailErrValve;
SMTP_Message mailErrorFlowSensor;
SMTP_Message mailErrorDHT;
SMTP_Message mailErrorHigro;
SMTP_Message mailActivSchedule; 
SMTP_Message mailNoActivSchedule;
SMTP_Message mailMonthlyData;
SMTP_Message mailCalibratSensor;
bool mailDripOnSended = false;
bool mailErrorValveSended = false;
bool mailErrorDHTSended = false;
bool mailErrorHigroSended = false;
bool mailActiveScheduleCheck = false;
bool mailNoActiveScheduleCheck = false;
bool mailStartSystemActive = true;
bool mailActiveScheduleActive = true;
bool mailNoActiveScheduleActive = true;
bool mailSmartDripOnActive = true;
bool mailCalibrateSensorSended = false;
String showErrorMail, showErrorMailConnect, finalMessage = "";
char errorMailConnect[256], errorMail[256], textMsg[4800];
/* Timers */
volatile bool toggle = true;
void IRAM_ATTR onTimer1();
hw_timer_t *timer1 = NULL;
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
/* Variables to save date and time */
String nowTime = "";
String date = "";
String startTime = "08:00";
String endTime = "10:30";
String startHourStr, startMinuteStr, endHourStr, endMinuteStr, dataMonthlyMessage;
int startHour, startMinute, endHour, endMinute;
int currentHour, currentMinute, currentDay, lastDay, lastDrip, lastDayDrip, counterDripDays;
int emailSendDay = 15;  // Día del mes en que se enviará el correo
int emailSendHour = 10;        // Hora del día en que se enviará el correo (formato 24 horas)
bool emailSentToday = false;   // Variable para asegurarnos de que solo se envíe una vez al día
void extractTimeValues();
void checkAndSendEmail();
void storeDailyData(int currentDay,int currentHour, int currentMinute);
void storeDripData(int currentDay, int currentHour, int currentMinute, bool dripActive);
String monthlyMessage();
void cleanData();   
void createID();
/* NTP server config */
const char* ntpServer = "pool.ntp.org";
const long gmtOFFset_sec = 3600;
const int daylightOffset_sec = 3600;
/* Terminal configuration for hygrometer and DHT11 */
void getDHTValues();     // Método para obtener los valores del sensor DHT11
void getHigroValues();   // Método para obtener los valores del sensor higrómetro
void getCalibrateHigroData();    // Método para recuperar los valores almacenados de la calibración del sensor higrómetro
int calibrateHigro(const char* fase, int minRange, int maxRange);   // Método para calibrar el sensor higrómetro según fase requerida
void startCalibration();   // Método para iniciar la calibración del sensor higrómetro
bool calibrationOk();     // Método para comprobar que la calibración ha sido correcta
void handleDrip();     // Método para el manejo de los procesos de riego
void handleOutOfScheduleDrip();    // Método para el manejo del riego fuera de horario activo
void finalizeDrip();   // Método para el manejo de la finalización del proceso de riego
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
//const int dry = 445;
//const int wet = 2;                   // Si se incrementa, el máximo (100%) sera mayor y viceversa
int dry, wet = 0;            // Variables para almacenar los valores límites del sensor higrómetro
int dripTime, dripTimeCheck = 0;
int dripHumidity, dripHumidityCheck = 0;
int dripTimeLimit = 5;
int dripHumidityLimit = 45;  
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
/* Instance to store in flash memory */
Preferences preferences;
char key[20], dayKeyHigro[20], dayKeyHum[20], dayKeyTemp[20], dayKeyRiego[20], emailBuffer[4100], lineBuffer[128]; 
unsigned long currentMillis, previousMillis = 0;
const unsigned long intervalDay = 86400000; // 1 día en milisegundos (24 horas)
/* Pin Config */
#define dripValveVin1 27  // Nueva configuración de pines antes 32. Salida Electroválvula 1
#define dripValveGND1 26  // Nueva configuración de pines antes 25. Salida Electroválvula 1
#define dripValveVin2 25  // Segunda válvula opcional
#define dripValveGND2 33  // Segunda válvula opcional
#define flowSensor  13    // Nueva configuración de pines antes 20 pendiente test pin 13
bool dripValve, activePulse;
bool dhtOk, dhtOkCheck, checkTimer, dripActived = false;
/* Pulse Variables */
const unsigned long pulseTime = 50; // Duración del pulso en milisegundos = 50ms
unsigned long startTimePulse = 0;
int closeValveCounter = 10;
void setup() {
  Serial.begin(9600);
   if (!SPIFFS.begin(true)) {
        Serial.println("No se pudo montar SPIFFS, se requiere formateo.");
    } else {
        Serial.println("SPIFFS montado correctamente.");
        Serial.printf("Tamaño total: %u bytes\n", SPIFFS.totalBytes());
        Serial.printf("Espacio usado: %u bytes\n", SPIFFS.usedBytes());
    }
    /* Start preferences */
  preferences.begin("sensor_data", false);
  idNumber = preferences.getUInt("device_id", 0); // Obtener el id único del dispositivo almacenado
  /* Creación de ID único */
  createID();
  Serial.print("ID único CRC32: ");
  Serial.println(idNumber, HEX);  // Muestra el id único del dispositivo en formato hexadecimal
  idSDHex += String(idNumber, HEX);
  showErrorMail = preferences.getString("lastMailError", " No mail errors " );
  showErrorMailConnect = preferences.getString("errorSMTPServer", " No SMTP connect error ");
  Serial.print("Error enviando mails almacenado: ");
  Serial.println(showErrorMail);
  Serial.print("Error conectando con el servidor SMTP almacenado: ");
  Serial.println(showErrorMailConnect);
  /* Inicio conexión WiFi */
  InitWiFi();
  Serial.print("Time: ");
  Serial.println(nowTime);
  Serial.print("Date: ");
  Serial.println(date);
  analogReadResolution(9);
  pinMode(dripValveVin1, OUTPUT);
  digitalWrite(dripValveVin1, LOW);
  pinMode(dripValveGND1, OUTPUT);
  digitalWrite(dripValveGND1, LOW);
  pinMode(flowSensor, INPUT);
  /* Configuración de la interrupción para detectar los pulsos del sensor de flujo */
  attachInterrupt(digitalPinToInterrupt(flowSensor), pulseCounter, FALLING);
  /* Temporizador */
  timer1 = timerBegin(0, 80, true);
  timerAttachInterrupt(timer1, &onTimer1, true);
  timerAlarmWrite(timer1, 1000000, true);
  timerAlarmEnable(timer1);
  timerAlarmDisable(timer1);
  /* Configuración de emails */
  //smtp.debug(1);
  //smtp.callback(smtpCallback);
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";
  /* Mail de inicio de Smart Drip System */
  mailStartSDS.sender.name = "Smart Drip System";
  mailStartSDS.sender.email = AUTHOR_EMAIL;
  mailStartSDS.subject = "Estado ESP32 Smart Drip";
  mailStartSDS.addRecipient("Pablo", "falder24@gmail.com");
  /* Mail de Estado de riego de Smart Drip System */
  mailDripOn.sender.name = "Smart Drip System";
  mailDripOn.sender.email = AUTHOR_EMAIL;
  mailDripOn.subject = "Estado Riego Smart Drip";
  mailDripOn.addRecipient("Pablo", "falder24@gmail.com");
  /* Mail de error en electrválvula de riego */
  mailErrValve.sender.name = "Smart Drip System";
  mailErrValve.sender.email = AUTHOR_EMAIL;
  mailErrValve.subject = "Estado válvula de Smart Drip";
  mailErrValve.addRecipient("Pablo", "falder24@gmail.com");
  /* Mail de error en sensor de flujo */
  mailErrorFlowSensor.sender.name = "Smart Drip System";
  mailErrorFlowSensor.sender.email = AUTHOR_EMAIL;
  mailErrorFlowSensor.subject = "Estado sensor de flujo";
  mailErrorFlowSensor.addRecipient("Pablo", "falder24@gmail.com");
  /* Mail de error en sensor DHT11 */
  mailErrorDHT.sender.name = "Smart Drip System";
  mailErrorDHT.sender.email = AUTHOR_EMAIL;
  mailErrorDHT.subject = "Estado sensor medio ambiente";
  mailErrorDHT.addRecipient("Pablo", "falder24@gmail.com"); 
  /* Mail de error en sensor Higro */
  mailErrorHigro.sender.name = "Smart Drip System";
  mailErrorHigro.sender.email = AUTHOR_EMAIL;
  mailErrorHigro.subject = "Estado sensor higro";
  mailErrorHigro.addRecipient("Pablo", "falder24@gmail.com"); 
  /* Mail de horario de riego activo */
  mailActivSchedule.sender.name = "Smart Drip System";
  mailActivSchedule.sender.email = AUTHOR_EMAIL;
  mailActivSchedule.subject = "Horario de riego activo";
  mailActivSchedule.addRecipient("Pablo", "falder24@gmail.com"); 
  /* Mail de horario de riego NO activo */
  mailNoActivSchedule.sender.name = "Smart Drip System";
  mailNoActivSchedule.sender.email = AUTHOR_EMAIL;
  mailNoActivSchedule.subject = "Horario de riego NO activo";
  mailNoActivSchedule.addRecipient("Pablo", "falder24@gmail.com"); 
  /* Mail semanal de comprobación de humedades */
  mailMonthlyData.sender.name = "Smart Drip System";
  mailMonthlyData.sender.email = AUTHOR_EMAIL;
  mailMonthlyData.subject = "Mail mensual de humedades";
  mailMonthlyData.addRecipient("Pablo", "falder24@gmail.com"); 
  /* Mail Proceso de Calibracion sensor higrómetro iniciado */
  mailCalibratSensor.sender.name = " Smart Drip System";
  mailCalibratSensor.sender.email = AUTHOR_EMAIL;
  mailCalibratSensor.subject = " Proceso de calibración iniciado";
  mailCalibratSensor.addRecipient("Pablo", "falder24@gmail.com");
  stopPulse();
  getCalibrateHigroData();
  getHigroValues();
  if(mailStartSystemActive){
    mailStartSystem();
  }
}
void loop() {
  /* Verificar cada hora la conexión WiFi y reconecta si se ha perdido */
  handleWiFiReconnection();
  /* Extraer valores de tiempo actual y selección de horario */
  extractTimeValues();
  /* Almacenar datos en NVS */
  storeDailyData(currentDay, currentHour, currentMinute);
  storeDripData(currentDay, currentHour, currentMinute, dripActived);
  /* Comprobacion y envío de mail mensual con los datos almacenados */
  checkAndSendEmail();
  /* Comprobación de horario activo */
  withinSchedule = isWithinSchedule(currentHour, currentMinute);
  /* Comprobar si el temporizador de riego está habilitado */
  checkTimer = timerAlarmEnabled(timer1);
  dripActived = checkTimer;  // Actualizar el estado de la activación del riego
  Serial.print("Timer ON: ");
  Serial.println(checkTimer);
  if (!checkTimer) {    // Si el temporizador no está habilitado, reiniciar los valores predeterminados de riego
    dripTime = dripTimeLimit;
    dripTimeCheck = dripTimeLimit;
    dripHumidity = dripHumidityLimit;
    Serial.println("Timer disabled");
  } else {       // Si el temporizador está habilitado, indicar que el proceso de riego está en curso
    Serial.println("Timer enabled");
    Serial.println("Drip process underway");
  }
    // Mostrar los valores actuales de tiempo y humedad de riego
  Serial.print("Drip time: ");
  Serial.println(dripTime);
  Serial.print("Drip humidity: ");
  Serial.println(dripHumidity);
  Serial.print("Error conectando con el servidor smtp almacenado: ");
  Serial.println(showErrorMailConnect);
  Serial.print("Error enviando mails almacenado: ");
  Serial.println(showErrorMail);
  if (withinSchedule) {   // Si estamos dentro del horario de riego
    /* Manejar el proceso de riego cuando estamos dentro del horario programado */
    handleDrip();    
  } else {
    /* Manejar situaciones de riego fuera del horario programado */
    handleOutOfScheduleDrip();
  }
  /* Finalizar el proceso de riego si el tiempo de riego ha terminado */
  finalizeDrip();
}
/* Timer 1min */
void IRAM_ATTR onTimer1(){
  toggle ^= true;
  if(toggle == true){
   counter++;
    if(counter == 59){
      counter = 0;
      dripTime--;
    }
  }
}
/* Get Time */
void extractTimeValues() {
  currentHour = rtc.getHour(true); // Obtenemos la hora actual, sólo el dato de la hora (0-23). True para formato 24h  
  currentMinute = rtc.getMinute(); // Obtenemos los minutos actuales
  currentDay = rtc.getDay();       // Obtenemos el número de día del mes (1-31)
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
/* Handle Irrigation */
void handleDrip() {
  getHigroValues();
  mailNoActiveScheduleCheck = false;
  Serial.println("Active irrigation schedule");
  if (!mailActiveScheduleCheck && mailActiveScheduleActive) {  
    mailActiveSchedule();  // Envío mail horario de riego activo - desactivado
  }
  if (substrateHumidity > dripHumidity) {
    if (!checkTimer) {
      Serial.println("Wet substrate, no need to water");
    }
  } else {
    Serial.println("Dry substrate, needs watering");
    timerAlarmEnable(timer1);
    if (!dripValve) {
      openDripValve();
      if(flowSensorEnabled){
      flowMeter();   // Solo se llama si el sensor de flujo está habilitado
      }
      Serial.println("Irrigation process underway");  
    } else {
      if(flowSensorEnabled){
      flowMeter();   // Solo se llama si el sensor de flujo está habilitado
      Serial.print("Caudal: ");
      Serial.print(caudal);
      Serial.print(" L/min - Volumen acumulado: ");
      Serial.print(totalLitros);
      Serial.println(" L.");
      }
      if (!mailDripOnSended && mailSmartDripOnActive) {  
        mailSmartDripOn();
      }
    }
    dripValve = true; // *** revisar si conviene activar esta variable aquí o dentro del método de apertura
    Serial.print("Salida ValvulaRiego: ");
    Serial.println(dripValve);
    if(flowSensorEnabled){
    Serial.print("Estado sensor flujo: ");
    Serial.println(flowMeterEstatus);    
    }
    Serial.print("Contador conectado: ");
    Serial.println(counter);
    Serial.print("Tiempo de riego: ");
    Serial.println(dripTime + " min.");
    delay(500);
  }
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
  if (dripTime <= 0) {
    Serial.println("Tiempo de Riego terminado");
    timerAlarmDisable(timer1);
    if (dripValve == true) {
      closeDripValve();
      dripValve = false;
      mailDripOnSended = false;
    }
  }
}
/* Get Higro Sensor Data */
void getCalibrateHigroData(){
  if (preferences.isKey("dryValue") && preferences.isKey("wetValue")) {
    dry = dryValue = preferences.getInt("dryValue");
    wet = wetValue = preferences.getInt("wetValue");
    Serial.println("Valores de calibración cargados desde memoria:");
    Serial.print("Valor Seco (0%): ");
    Serial.println(dry);
    Serial.print("Valor Húmedo (100%): ");
    Serial.println(wet);
    if (calibrationOk()) {
      Serial.println("Calibración válida.");
       mailCalibrateSensorSended = false;
    } else {
      Serial.println("ERROR: Calibración inválida. Iniciando recalibración...");
      startCalibration();
    }
  } else {
    Serial.println("No se encontraron valores de calibración. Iniciando calibración...");
    startCalibration();
  }
}
/* Calibrate Higro Process */
int calibrateHigro(const char* fase, int minRange, int maxRange) {
  Serial.println("Proceso de calibración del sensor de humedad iniciado. Siga las siguientes indicaciones: \n");
  if (strcmp(fase, "aire") == 0) {
    Serial.print("Por favor, asegúrese de que el sensor no está en contacto con ninguna sustancia húmeda, \n");
    Serial.println("deje el sensor en el aire y espere 10 segundos.");
  } else {
    Serial.println("introduzca el sensor en agua y espere 10 segundos.");
  }
  Serial.println("La calibración empezará en: ");
  for(int i = 10; i > 0; i--){ 
    Serial.print("Tiempo: ");  // Espera 10 segundos para que el usuario coloque el sensor en el aire
    Serial.print(i);
    Serial.println("s.");
    delay(1000);
  }
  int valor = (strcmp(fase, "aire") == 0) ? 4095 : 0;  // Inicialización según la fase
    Serial.println("Iniciando lecturas...");
  for (int i = 0; i < 5; i++) {
    int lectura = analogRead(PinHigro);
    if (strcmp(fase, "aire") == 0) {
      if (lectura < valor) {
        valor = lectura;  // Busca el mínimo en aire
      }
    } else {
      if (lectura > valor) {
        valor = lectura;  // Busca el máximo en agua
      }
    }
    Serial.print("Lectura ");
    Serial.print(String(i + 1));
    Serial.print(": ");
    Serial.println(lectura);
    delay(2000);  // Espera de 2 segundos entre lecturas
  }
  Serial.print("Calibración en ");
  Serial.print(fase);
  Serial.println(" completada. Valor: " + valor);
  if (valor < minRange || valor > maxRange) {
    Serial.print("ERROR: Valor de ");
    Serial.print(fase);
    Serial.println(" fuera del rango esperado.");
    return -1;  // Indica que la calibración falló
  }
  return valor;  // Retorna el valor calibrado si es válido
}
/* Check Calibration */
bool calibrationOk() {
  if (dryValue < 400 || dryValue > 500) {
    return false;
  }
  if (wetValue < 0 || wetValue > 100) {
    return false;
  }
  if (dryValue <= wetValue) {
    return false;
  }
  return true;
}
/* Start Calibration Phases */
void startCalibration() {
  if(!mailCalibrateSensorSended){
    mailCalibrateSensor();
  }
  dryValue = calibrateHigro("aire", 400, 500);
  if (dryValue == -1) {
    Serial.println("ERROR: Calibración en aire fallida. Repita el proceso.");
    startCalibration();
    return;
  }
  wetValue = calibrateHigro("agua", 1, 100);
  if (wetValue == -1 || dryValue <= wetValue) {
    Serial.println("ERROR: Calibración en agua fallida o inconsistente. Repita el proceso.");
    startCalibration();
    return;
  }
  preferences.putInt("dryValue", dryValue);
  preferences.putInt("wetValue", wetValue);
  Serial.println("Calibración completada y válida.");
  Serial.println("Valores almacenados en memoria");
  // Imprimir los valores almacenados para verificación
  int storedDryValue = preferences.getInt("dryValue");
  int storedWetValue = preferences.getInt("wetValue");
  Serial.println("Verificación de valores almacenados:");
  Serial.print("Valor en aire (0% humedad): ");
  Serial.println(storedDryValue);
  Serial.print("Valor en agua (100% humedad): ");
  Serial.println(storedWetValue);
  mailErrorHigroSended = false;      // Resetear el flag de error de envío de email
}
/* Getting Higro Measurements */
void getHigroValues(){
  higroValue = analogRead(PinHigro);
  substrateHumidity = map(higroValue, wet, dry, 100, 0);
  if(higroValue < wet || higroValue > dry){
    Serial.println("ADVERTENCIA: El sensor está fuera del rango calibrado. Recalibración recomendada.");
    if(!mailErrorHigroSended){
      Serial.println(" Mail error en sensor de humedad enviado ");
      mailErrorSensorHigro();
    }
  }
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
void flowMeter(){
  if ((millis() - oldTime) > 1000){                // Cálculo del caudal cada segundo
    detachInterrupt(digitalPinToInterrupt(flowSensor));    // Desactiva las interrupciones mientras se realiza el cálculo
    Serial.print("Pulsos: ");
    Serial.println(pulses);
    /* Calculates the flow rate in liters per minute */
    caudal = pulses / 5.5;                         // factor de conversión, siendo K=7.5 para el sensor de ½”, K=5.5 para el sensor de ¾” y 3.5 para el sensor de 1”
    pulses = 0;                                    // Reinicia el contador de pulsos
    waterVolume = (caudal / 60) * 1000/1000;       // Calcula el volumen de agua en mililitros
    totalLitros += waterVolume;                    // Incrementa el volumen total acumulado
    attachInterrupt(digitalPinToInterrupt(flowSensor), pulseCounter, FALLING);
    oldTime = millis();
    float caudalRiego = caudal;
    float caudalTotal = totalLitros; 
    if(caudal != 0){
      flowMeterEstatus = true;
      Serial.println(" Sensor de riego conectado");
    }else{
      flowMeterEstatus = false;
      Serial.println(" Sensor de riego desconectado");
    } 
  }
}
/* Create and Encrypt ID */
void createID(){
if (idNumber == 0) {  // Si no está almacenado, se genera y se almacena
    String macAddress = WiFi.macAddress();  // Inicializa la dirección MAC como un ID único y convierte la dirección MAC a un array de bytes
    Serial.print("Dirección MAC: ");
    Serial.println(macAddress);
    uint8_t macBytes[6];
    sscanf(macAddress.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", 
           &macBytes[0], &macBytes[1], &macBytes[2], 
           &macBytes[3], &macBytes[4], &macBytes[5]);
    idNumber = crc32(macBytes, 6);
    // Muestra el hash en el monitor serial
    Serial.print("ID único (CRC32): ");
    Serial.println(idNumber, HEX);
    preferences.putUInt("device_id", idNumber);
  }
}
/* New Start WiFi */
void InitWiFi() {
  WiFi.begin(SSID, PASS);  // Inicializamos el WiFi con nuestras credenciales.
  Serial.print("Conectando a ");
  Serial.print(SSID);
  Serial.println("...");
  int tries = 0;
  state = WiFi.status();
  unsigned long initTime = millis();
  const unsigned long interval = 5000;   // 5 segundos
  const unsigned long waitTime = 15000;  // 15 segundos para dar tiempo al WiFi
  while (state != WL_CONNECTED && tries < MAX_CONNECT) {      // Continuar mientras no esté conectado y no se hayan agotado los intentos
    currentMillis = millis();
    // Verificar si han pasado 5 segundos
    if (currentMillis - initTime >= interval) {
      Serial.print("...Intento de conectar a la red WiFi ");
      Serial.print(SSID);
      Serial.print(": ");
      Serial.println(tries + 1);
      if (state != WL_CONNECTED && (currentMillis - initTime >= waitTime)) {   // Verificar si el tiempo de espera total ha pasado para intentar reconectar
        WiFi.reconnect();
        initTime = millis(); // Reiniciar el temporizador solo después de reconectar
      }else{
        initTime = millis();
      }
      state = WiFi.status();
      tries++;
    }
    // Aquí puedes ejecutar otras tareas mientras esperas
  }
  if (state == WL_CONNECTED) {      // Verificar si la conexión fue exitosa
    Serial.println("\n\nConexión exitosa!!!");
    Serial.print("Tu IP es: ");
    Serial.println(WiFi.localIP());
    NTPsincro();
  } else {
    Serial.println("\n\nError: No se pudo conectar a la red WiFi.");
  }
}
/* Check WiFi Reconnection */
void handleWiFiReconnection() {
  if (millis() - lastConnectionTry >= tryInterval) {  // Comprobación de la conexión de la red WiFi cada hora
    lastConnectionTry = millis();                     // Actualizar el tiempo del último chequeo
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("Conexión WiFi perdida. Intentando reconectar...");
      InitWiFi();
    }
    Serial.println("Conexión WiFi estable. ");
  }
}
/* NTP Sincronization with RTC */
void NTPsincro(){
  configTime(gmtOFFset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  int attempts = 0;
  const int maxAttempts = 5;                              // Número máximo de reintentos
  while (attempts < maxAttempts) {
    if (getLocalTime(&timeinfo)) {
        Serial.println("Hora sincronizada con NTP:");     // Mostrar la fecha y hora formateada
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");   
        rtc.setTimeStruct(timeinfo);                      // Establecer el RTC con la estructura de tiempo obtenida
        nowTime = rtc.getTime();
        date = rtc.getDate();
        return;
    }
    Serial.println("Error al sincronizar con NTP. Reintentando...");
    attempts++;
    delay(2000);
  }
  Serial.println("Error: No se pudo sincronizar con NTP tras varios intentos."); 
  Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));  // Mostrar la hora en formato legible
}
/* Storage Data Sensors */
void storeDailyData(int currentDay, int currentHour, int currentMinute){
  if (currentDay != lastDay && currentHour == endHour && currentMinute == endMinute) {  // Comprueba si el último día de guardado es diferente al día actual y si estamos finalizando el horario activo de riego
    getHigroValues();
    getDHTValues();
    Serial.println("Nuevo día detectado, almacenando datos...");      
    // Guarda los datos en la memoria no volátil
    sprintf(key, "Higro_day%d", currentDay);
    preferences.putInt(key, substrateHumidity);
    if(dhtOk){
      sprintf(key, "Humedad_day%d", currentDay);
      preferences.putInt(key, humidity);
      sprintf(key, "Temp_day%d", currentDay);
      preferences.putInt(key, temp);
    }
    lastDay = currentDay;
    Serial.print("Datos guardados para el día ");
    Serial.println(date);
  }else{
    Serial.print("Los datos para el día ");
    Serial.print(date);
    Serial.println(" ya han sido guardados.");
    dripActived = false;
  }
}
/* Storage Drip Data */
void storeDripData(int currentDay, int currentHour, int currentMinute, bool dripActive){
  if(currentDay != lastDayDrip && currentHour == endHour && currentMinute == endMinute){
    sprintf(key, "riego_day%d", currentDay);
    preferences.putBool(key, dripActived);
    if(!dripActive){
      counterDripDays ++;
    }else{
      counterDripDays = 0;
    }
    dripActived = false;
    lastDayDrip = currentDay;
  }
  if(counterDripDays == 25){

  }
}
/* Sender Monthly Mail */
void checkAndSendEmail(){
  // Comprobar si es el día y la hora configurados para enviar el correo
  if (currentDay == emailSendDay && currentHour >= emailSendHour && !emailSentToday) {
    //Envía correo mensual con los datos almacenados
    dataMonthlyMessage = monthlyMessage();
    mailMonthData(dataMonthlyMessage);
    Serial.println("Informe mensual enviado");
    // Marcar que el correo ya fue enviado hoy
    emailSentToday = true;
    // Borrar datos guardados
    cleanData();
  }
  if (currentDay != emailSendDay) {       // Si es otro día, restablecer la bandera para permitir envío el próximo més
    emailSentToday = false;
  }
}
/* Monthly Data Message Maker */
String monthlyMessage() {
  emailBuffer[0] = '\0';  // Inicializar el buffer vacío
  for (int day = 1; day <= 31; day++) {       // Iterar sobre los días del mes (1-31)
    // Crear claves únicas para cada día
    snprintf(dayKeyHigro, sizeof(dayKeyHigro), "Higro_day%d", day);
    snprintf(dayKeyHum, sizeof(dayKeyHum), "Humedad_day%d", day);
    snprintf(dayKeyTemp, sizeof(dayKeyTemp), "Temp_day%d", day);
    snprintf(dayKeyRiego, sizeof(dayKeyRiego), "Riego_day%d", day);
    // Verificar si existen los datos para ese día en `Preferences`
    if (preferences.isKey(dayKeyRiego) && preferences.isKey(dayKeyHigro)) {
      int higro = preferences.getInt(dayKeyHigro);
      int hum = preferences.getInt(dayKeyHum);
      int tempe = preferences.getInt(dayKeyTemp);
      bool dripWasOn = preferences.getBool(dayKeyRiego);
      snprintf(lineBuffer, sizeof(lineBuffer),                // Añadir los datos de este día al cuerpo del correo. Formatear una línea de datos para este día
               "Día %d:\n Humedad del sustrato = %d%%\n"
               "Humedad ambiental = %d%%\n"
               "Temperatura ambiental = %d °C\n"
               "Riego activado: %s\n",
               day, higro, hum, tempe, dripWasOn ? "Sí" : "No");
    } else {
      snprintf(lineBuffer, sizeof(lineBuffer), "Día %d: Sin datos.\n", day);             // Si no hay datos, formatear un mensaje vacío para este día
    }
    strncat(emailBuffer, lineBuffer, sizeof(emailBuffer) - strlen(emailBuffer) - 1);     // Añadir la línea al buffer principal
  }
  return String(emailBuffer);  // Convertir a String para devolver
}
/* Mail Start System */
void mailStartSystem(){
  snprintf(textMsg, sizeof(textMsg),
         "%s \n%s \n"
         "SmartDrip%s conectado a la red y en funcionamiento. \n"
         "Datos de configuración guardados: \n"
         "Tiempo de riego: %d min. \n"
         "Límite de humedad de riego: %d%% \n"
         "Horario de activación de riego: \n"
         "Hora de inicio: %s\n"
         "Hora de fin: %s\n"
         "Humedad sustrato: %d%% \n",
         idSDHex.c_str(), idUser.c_str(), idSmartDrip.c_str(),  // Convertir `String` a `const char*`
         dripTimeLimit,
         dripHumidityLimit,
         startTime.c_str(), endTime.c_str(),                   // Convertir `String` a `const char*`
         substrateHumidity);
  finalMessage = String(textMsg);       // Si necesitas devolverlo como String
  mailStartSDS.text.content = finalMessage.c_str();
  mailStartSDS.text.charSet = "us-ascii";
  mailStartSDS.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailStartSDS.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    snprintf(errorMailConnect, sizeof(errorMailConnect),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("errorSMTPServer", errorMailConnect);
    Serial.print("Error conectando al servidor SMTP: ");
    Serial.println(errorMailConnect);
    return;
  if(!MailClient.sendMail(&smtp, &mailStartSDS)){
    snprintf(errorMail, sizeof(errorMail),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("lastMailError", errorMail);
    Serial.println("Error envío Email: ");
    Serial.println(errorMail);
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
}
/* Mail Active Schedule */
void mailActiveSchedule(){
  nowTime = rtc.getTime();
  date = rtc.getDate();
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "SmartDrip%s: inicia horario activo de riego. \n"
           "RTC: con fecha: %s\n"
           "      hora: %s\n"
           "Datos de configuración guardados: \n"
           "Tiempo de riego: %d min. \n"
           "Límite de humedad de riego: %d%% \n"
           "Horario de activación de riego: \n"
           "Hora de inicio: %s\n"
           "Hora de fin: %s\n"
           "Humedad sustrato: %d%% \n",
           idSDHex.c_str(),                // ID del SD en formato string
           idUser.c_str(),                 // ID del usuario
           idSmartDrip.c_str(),            // ID del dispositivo SmartDrip
           date.c_str(),                   // Fecha del RTC
           nowTime.c_str(),                // Hora del RTC
           dripTimeLimit,                  // Tiempo de riego en minutos
           dripHumidity,                   // Límite de humedad para riego
           startTime.c_str(),              // Hora de inicio de riego
           endTime.c_str(),                // Hora de fin de riego
           substrateHumidity);             // Humedad del sustrato
  finalMessage = String(textMsg);
  mailActivSchedule.text.content = finalMessage.c_str();
  mailActivSchedule.text.charSet = "us-ascii";
  mailActivSchedule.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailActivSchedule.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    snprintf(errorMailConnect, sizeof(errorMailConnect),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("errorSMTPServer", errorMailConnect);
    Serial.print("Error conectando al servidor SMTP: \n");
    Serial.println(errorMailConnect);
    return;
  if(!MailClient.sendMail(&smtp, &mailActivSchedule)){
    snprintf(errorMail, sizeof(errorMail),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("lastMailError", errorMail);
    Serial.println("Error envío Email: ");
    Serial.println(errorMail);
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
  mailActiveScheduleCheck = true;
}
/* Mail No ACtive Schedule */
void mailNoActiveSchedule(){
  nowTime = rtc.getTime();
  date = rtc.getDate();
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "SmartDrip %s: Fuera horario activo de riego. \n"
           "RTC: con fecha: %s\n"
           "      hora: %s\n"
           "Datos de configuración guardados: \n"
           "Tiempo de riego: %d min. \n"
           "Límite de humedad de riego: %d%% \n"
           "Horario de activación de riego: \n"
           "Hora de inicio: %s\n"
           "Hora de fin: %s\n"
           "Humedad sustrato: %d%% \n",
           idSDHex.c_str(),                // ID del SD en formato string
           idUser.c_str(),                 // ID del usuario
           idSmartDrip.c_str(),            // ID del dispositivo SmartDrip
           date.c_str(),                   // Fecha del RTC
           nowTime.c_str(),                // Hora del RTC
           dripTimeLimit,                  // Tiempo de riego en minutos
           dripHumidity,                   // Límite de humedad para riego
           startTime.c_str(),              // Hora de inicio de riego
           endTime.c_str(),                // Hora de fin de riego
           substrateHumidity);             // Humedad del sustrato
  finalMessage = String(textMsg);
  mailNoActivSchedule.text.content = finalMessage.c_str();
  mailNoActivSchedule.text.charSet = "us-ascii";
  mailNoActivSchedule.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailNoActivSchedule.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    snprintf(errorMailConnect, sizeof(errorMailConnect),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("errorSMTPServer", errorMailConnect);
    Serial.print("Error conectando al servidor SMTP: \n");
    Serial.println(errorMailConnect);
    return;
  if(!MailClient.sendMail(&smtp, &mailNoActivSchedule)){
    snprintf(errorMail, sizeof(errorMail),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("lastMailError", errorMail);
    Serial.println("Error envío Email: ");
    Serial.println(errorMail);
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
  mailNoActiveScheduleCheck = true;
}
/* Mail Drip On */
void mailSmartDripOn(){
  nowTime = rtc.getTime();  //Probar si no es necesario actualizar hora y fecha para el envío del mail  
  date = rtc.getDate(); 
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "Con fecha: %s\n"
           "Riego conectado correctamente en Smart Drip%s a las: %s\n"
           "Tiempo de riego: %d min. \n"
           "Límite de humedad de riego: %d%% \n"
           "Humedad sustrato: %d%% \n",
           idSDHex.c_str(),                // ID del SD en formato string
           idUser.c_str(),                 // ID del usuario
           date.c_str(),                   // Fecha
           idSmartDrip.c_str(),            // ID del SmartDrip
           nowTime.c_str(),                // Hora actual
           dripTimeLimit,                  // Tiempo de riego en minutos
           dripHumidity,                   // Límite de humedad para riego
           substrateHumidity);             // Humedad del sustrato
  finalMessage = String(textMsg);
  mailDripOn.text.content = finalMessage.c_str();
  mailDripOn.text.charSet = "us-ascii";
  mailDripOn.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailDripOn.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    snprintf(errorMailConnect, sizeof(errorMailConnect),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("errorSMTPServer", errorMailConnect);
    Serial.print("Error conectando al servidor SMTP: \n");
    Serial.println(errorMailConnect);
    return;
  if(!MailClient.sendMail(&smtp, &mailDripOn)){
    snprintf(errorMail, sizeof(errorMail),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("lastMailError", errorMail);
    Serial.println("Error envío Email: ");
    Serial.println(errorMail);
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  mailDripOnSended = true;
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
  if(!smtp.connect(&session))
    snprintf(errorMailConnect, sizeof(errorMailConnect),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("errorSMTPServer", errorMailConnect);
    Serial.print("Error conectando al servidor SMTP: \n");
    Serial.println(errorMailConnect);
    return;
  if(!MailClient.sendMail(&smtp, &mailErrValve)){
    snprintf(errorMail, sizeof(errorMail),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("lastMailError", errorMail);
    Serial.println("Error envío Email: ");
    Serial.println(errorMail);
  }else{
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
  if(!smtp.connect(&session))
    snprintf(errorMailConnect, sizeof(errorMailConnect),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("errorSMTPServer", errorMailConnect);
    Serial.print("Error conectando al servidor SMTP: \n");
    Serial.println(errorMailConnect);
    return;
  if(!MailClient.sendMail(&smtp, &mailErrorDHT)){
    snprintf(errorMail, sizeof(errorMail),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("lastMailError", errorMail);
    Serial.println("Error envío Email: ");
    Serial.println(errorMail);
  }else{
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
  if(!smtp.connect(&session))
    snprintf(errorMailConnect, sizeof(errorMailConnect),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("errorSMTPServer", errorMailConnect);
    Serial.print("Error conectando al servidor SMTP: \n");
    Serial.println(errorMailConnect);
    return;
  if(!MailClient.sendMail(&smtp, &mailErrorHigro)){
    snprintf(errorMail, sizeof(errorMail),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("lastMailError", errorMail);
    Serial.println("Error envío Email: ");
    Serial.println(errorMail);
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  mailErrorHigroSended = true;
  smtp.closeSession();
}
/* Mail Start Calibration */
void mailCalibrateSensor(){
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "El sensor de humedad del sustrato del Smart Drip %s necesita ser calibrado y se ha iniciado el proceso de calibración. \n"
           "Proceda a su inspección o llame al servicio técnico \n",
           idSDHex.c_str(),                // ID del SD en formato string
           idUser.c_str(),                 // ID del usuario
           idSmartDrip.c_str());           // ID del dispositivo SmartDrip
  finalMessage = String(textMsg);
  mailCalibratSensor.text.content = finalMessage.c_str();
  mailCalibratSensor.text.charSet = "us-ascii";
  mailCalibratSensor.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailCalibratSensor.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    snprintf(errorMailConnect, sizeof(errorMailConnect),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("errorSMTPServer", errorMailConnect);
    Serial.print("Error conectando al servidor SMTP: \n");
    Serial.println(errorMailConnect);
    return;
  if(!MailClient.sendMail(&smtp, &mailCalibratSensor)){
    snprintf(errorMail, sizeof(errorMail),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("lastMailError", errorMail);
    Serial.println("Error envío Email: ");
    Serial.println(errorMail);
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  mailCalibrateSensorSended = true;
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
/* Mail Monthly Data */
void mailMonthData(String message){
  date = rtc.getDate();
  nowTime = rtc.getHour();
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "Mensaje mensual de comprobación de humedades del sustrato del Smart Drip %s \n"
           "con fecha %s los datos son: \n"
           "%s\n",
           idSDHex.c_str(),        // ID del SD
           idUser.c_str(),         // ID del usuario
           idSmartDrip.c_str(),    // ID del Smart Drip
           date.c_str(),           // Fecha
           message.c_str());       // Mensaje mensual con los datos
  finalMessage = String(textMsg);
  mailMonthlyData.text.content = finalMessage.c_str();
  mailMonthlyData.text.charSet = "us-ascii";
  mailMonthlyData.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailMonthlyData.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    snprintf(errorMailConnect, sizeof(errorMailConnect),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("errorSMTPServer", errorMailConnect);
    Serial.print("Error conectando al servidor SMTP: \n");
    Serial.println(errorMailConnect);
    return;
  if(!MailClient.sendMail(&smtp, &mailMonthlyData)){
    snprintf(errorMail, sizeof(errorMail),
         "%s\n%s\n El sistema reporta el siguiente error: \n %s",
         date, nowTime, smtp.errorReason().c_str());
    preferences.putString("lastMailError", errorMail);
    Serial.println("Error envío Email: ");
    Serial.println(errorMail);
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  smtp.closeSession();
}
/* Clean Data Preferences */
void cleanData(){
  for(int i = 1; i <= 31; i++){
     snprintf(key, sizeof(key), "Higro_day%d", i);
    preferences.remove(key);
    snprintf(key, sizeof(key), "Humedad_day%d", i);
    preferences.remove(key);
    snprintf(key, sizeof(key), "Temp_day%d", i);
    preferences.remove(key);
    snprintf(key, sizeof(key), "Riego_day%d", i);
    preferences.remove(key);
  }
}