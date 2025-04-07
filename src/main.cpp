#include <Arduino.h>
#include <SimpleDHT.h>
#include <NTPClient.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <ESP_Mail_Client.h>
#include <ESP32Time.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h> // Biblioteca para obtener detalles de la memoria
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
void mailSmartDripOn();                     // mail proceso de riego iniciado
void mailSmartDripOff();                    // mail proceso de riego finalizado
void mailStartSystem();                     // mail inicio sistema
void mailErrorValve();                      // mail error en electroválvula
void mailErrorDHT11();                      // mail error en sensor DHT11
void mailErrorSensorHigro();                // mail error en sensor higrometro
void mailActiveSchedule(String message);    // mail horario de riego activo
void mailNoActiveSchedule(String message);  // mail horario de riego no activo
bool mailMonthData(String message);         // mail datos de riego mensual
void mailCalibrateSensor();
void saveMailError(const char* key, String errorMsg);
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
SMTP_Message mailMonthlyData;
SMTP_Message mailCalibratSensor;
bool mailDripOnSended = false;
bool mailDripOffSended = false;
bool mailErrorValveSended = false;
bool mailErrorDHTSended = false;
bool mailErrorHigroSended = false;
bool mailActiveScheduleCheck = false;
bool mailNoActiveScheduleCheck = false;
bool mailStartSystemActive = true;
bool mailActiveScheduleActive = true;
bool mailNoActiveScheduleActive = true;
bool mailSmartDripOnActive = true;
bool mailSmartDripOffActive = true;
bool mailCalibrateSensorSended = false;
String showErrorMail, showErrorMailConnect, finalMessage = "";
char errorMailConnect[256], errorMail[256], textMsg[4800];
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
int currentHour, currentMinute, currentDay, currentMonth, currentYear, lastDay, lastDrip, lastDayDrip, counterDripDays;
int emailSendHour = 9;        // Hora del día en que se enviará el correo (formato 24 horas)
bool emailSentToday = false;   // Variable para asegurarnos de que solo se envíe una vez al día
void extractTimeValues();
int getLastDayOfMonth(int month, int year);
void checkAndSendEmail();
void storeDailyData(int currentDay, int currentMonth, int currentHour, int currentMinute, int newSubstrate, int newHumidity, int newTemp);
void storeDripData(int currentDay, int currentMonth, int currentHour, int currentMinute, bool dripActive);
bool verifyStoredData(int day, int month);
bool verifyDripStored(int day, int month);
void backupStoreIfMissed(int currentDay, int currentMonth, int currentHour, int currentMinute);
void showMemoryStatus();
String monthlyMessage(int month);
void cleanData();   
void createID();
/* NTP server config */
const char* ntpServer = "hora.roa.es"; // Servidor NTP para sincronizar la hora
/* Terminal configuration for hygrometer and DHT11 */
void getDHTValues();               // Método para obtener los valores del sensor DHT11
void getHigroValues();             // Método para obtener los valores del sensor higrómetro
void handleDrip();                 // Método para el manejo de los procesos de riego
void handleScheduleDrip();         // Método para el manedo del riego dentro del horario activo
void handleOutOfScheduleDrip();    // Método para el manejo del riego fuera de horario activo
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
/* Instance to store in flash memory */
Preferences preferences;
char key[20], substrateKey[20], humidityKey[20], tempKey[20], dripKey[20], dayKeyHigro[20], dayKeyHum[20], dayKeyTemp[20], dayKeyRiego[20], emailBuffer[4100], lineBuffer[128], flagSensorKey[20], flagDripKey[20];
bool dripData[31] = {false};
bool dataStoredFlag[31], dripStoredFlag[31];
int substrateData[31], humidityData[31], tempData[31];
void inicializarDatos() {
  for (int i = 0; i < 31; i++) {
      substrateData[i] = -100;
      humidityData[i] = -100;
      tempData[i] = -100;
  }
}
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
void setup() {
  Serial.begin(9600);
  inicializarDatos();
   if (!SPIFFS.begin(true)) {
        Serial.println("No se pudo montar SPIFFS, se requiere formateo.");
    } else {
        Serial.println("SPIFFS montado correctamente.");
        Serial.printf("Tamaño total: %u bytes\n", SPIFFS.totalBytes());
        Serial.printf("Espacio usado: %u bytes\n", SPIFFS.usedBytes());
    }
    /* Start preferences */
  preferences.begin("sensor_data", true);        
  idNumber = preferences.getUInt("device_id", 0); // Obtener el id único del dispositivo almacenado
  preferences.end(); 
  /* Creación de ID único */
  createID();
  Serial.print("ID único CRC32: ");
  Serial.println(idNumber, HEX);  // Muestra el id único del dispositivo en formato hexadecimal
  idSDHex += String(idNumber, HEX);
  preferences.begin("sensor_data", true);  
  showErrorMail = preferences.getString("lastMailError", " No mail errors " );
  showErrorMailConnect = preferences.getString("erSMTPServ", " No SMTP connect error ");
  preferences.end();     
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
  /* Mail de inicio de riego de Smart Drip System */
  mailDripOn.sender.name = "Smart Drip System";
  mailDripOn.sender.email = AUTHOR_EMAIL;
  mailDripOn.subject = "Inicio Riego Smart Drip";
  mailDripOn.addRecipient("Pablo", "falder24@gmail.com");
  /* Mail de finalización de riego de Smart Drip System */
  mailDripOff.sender.name = "Smart Drip System";
  mailDripOff.sender.email = AUTHOR_EMAIL;
  mailDripOff.subject = "Fin Riego Smart Drip";
  mailDripOff.addRecipient("Pablo", "falder24@gmail.com");
  /* Mail de error en electroválvula de riego */
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
  //getCalibrateHigroData();
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
  storeDailyData(currentDay, currentMonth, currentHour, currentMinute, substrateHumidity, humidity, temp);
  storeDripData(currentDay, currentMonth, currentHour, currentMinute, dripActived);
  if (currentHour > endHour || (currentHour == endHour && currentMinute > endMinute)) {
    backupStoreIfMissed(currentDay, currentMonth, currentHour, currentMinute);
  }
  /* Comprobacion y envío de mail mensual con los datos almacenados */
  checkAndSendEmail();
  /* Comprobación de horario activo */
  withinSchedule = isWithinSchedule(currentHour, currentMinute);
  /* Comprobar si el temporizador de riego está habilitado */
  dripActived = checkTimer;  // Actualizar el estado de la activación del riego
  Serial.print("Log Error conectando con el servidor smtp almacenado: ");
  Serial.println(showErrorMailConnect);
  Serial.print("Log Error enviando mails almacenado: ");
  Serial.println(showErrorMail);
  if (withinSchedule) {   // Si estamos dentro del horario de riego
    /* Manejar el proceso de riego cuando estamos dentro del horario programado */
    handleScheduleDrip();
  } else {
    /* Manejar situaciones de riego fuera del horario programado */
    handleOutOfScheduleDrip();
  }
  /* Finalizar el proceso de riego si el tiempo de riego ha terminado */
  finalizeDrip();
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
void handleScheduleDrip(){
  getHigroValues();
  mailNoActiveScheduleCheck = false;
  Serial.println("Active irrigation schedule");
  if (!mailActiveScheduleCheck && mailActiveScheduleActive) {  
    dataMonthlyMessage = monthlyMessage(currentMonth);
    Serial.println(dataMonthlyMessage);
    mailActiveSchedule(dataMonthlyMessage);                 // Envío mail horario de riego activo
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
    startDripTime = millis();                                 // Marcar el tiempo de inicio del riego
  }
  if (!dripValve) {
    openDripValve();
    checkTimer = true;
    mailDripOffSended = false;
    if(flowSensorEnabled) {
      flowMeter();                                            // Solo se llama si el sensor de flujo está habilitado
    }
    Serial.println("Drip process underway");  
  } else {
    if(flowSensorEnabled) {
      flowMeter();                                            // Solo se llama si el sensor de flujo está habilitado
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
  elapsedTime = millis() - startDripTime;                     // Calcular el tiempo transcurrido desde el inicio del riego en milisegundos
  remainingTime = dripTime - elapsedTime;                     // Mostrar el tiempo restante
  remainingMinutes = remainingTime / 60000;
  remainingSeconds = (remainingTime % 60000) / 1000;
  Serial.print("Drip in progress. Time remaining: ");
  Serial.print(remainingMinutes);
  Serial.print(" minutes, ");
  Serial.print(remainingSeconds);
  Serial.println(" seconds.");
  if (flowSensorEnabled) {
    flowMeter();
    Serial.print("Caudal: ");
    Serial.print(caudal);
    Serial.print(" L/min - Volumen acumulado: ");
    Serial.print(totalLitros);
    Serial.println(" L.");
  }
}
/* Handle Out of Schedule Irrigation */
void handleOutOfScheduleDrip() {
  Serial.println("Fuera de horario de riego");
  Serial.print("Caudal de riego fuera de horario: ");  
  Serial.println(caudal);
  mailActiveScheduleCheck = false;
  if (!mailNoActiveScheduleCheck && mailNoActiveScheduleActive) {
    dataMonthlyMessage = monthlyMessage(currentMonth);
    Serial.println(dataMonthlyMessage);
    mailNoActiveSchedule(dataMonthlyMessage);
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
  if(checkTimer){
    elapsedTime = millis() - startDripTime;                     // Calcular el tiempo transcurrido desde el inicio del riego en milisegundos
    remainingTime = dripTime - elapsedTime;                     // Mostrar el tiempo restante
    remainingMinutes = remainingTime / 60000;
    remainingSeconds = (remainingTime % 60000) / 1000;
    Serial.print("Drip in progress. Time remaining: ");
    Serial.print(remainingMinutes);
    Serial.print(" minutes, ");
    Serial.print(remainingSeconds);
    Serial.println(" seconds.");                
    if (elapsedTime >= dripTime) {
      Serial.println("Drip process completed");
      if (dripValve == true) {
        closeDripValve();
        checkTimer = false;                                    // Finalizar el proceso de riego
        mailDripOnSended = false;
        if(!mailDripOffSended && mailSmartDripOffActive){
          mailSmartDripOff();
        }
        getHigroValues();
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
    preferences.begin("sensor_data", false);  // Modo escritura
    preferences.putUInt("device_id", idNumber);
    preferences.end();  // 🔹 Cierra Preferences después de eliminar los datos
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
/* Function to save the last synchronized time in NVS memory */
void saveLastSyncTime(time_t timestamp) {
  preferences.begin("sensor_data", false); 
  preferences.putULong64("lastSync", timestamp);  // Guarda el timestamp en NVS
  preferences.end(); 
}
/* Function to retrieve the last synchronized time from NVS memory */
time_t getLastSyncTime() {
  preferences.begin("sensor_data", true);                   // Modo lectura (true)
  time_t lastSync = preferences.getULong64("lastSync", 0);  // Recupera el timestamp, 0 si no hay ninguno
  preferences.end();                                        // Cierra Preferences después de leer
  return lastSync;
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
/* Storage Data Sensors */
void storeDailyData(int currentDay, int currentMonth, int currentHour, int currentMinute, int newSubstrate, int newHumidity, int newTemp) {
  // ✅ Asegurar que solo se ejecuta en el minuto exacto en que termina el horario activo
  if (currentHour != endHour || currentMinute != endMinute) {
      return;  // Salir si no estamos en el momento exacto de guardado
  }
  if (verifyStoredData(currentDay, currentMonth)) {
      Serial.printf("✅ Datos del día %d ya estaban almacenados, no se guardan de nuevo.\n", currentDay);
      showMemoryStatus();
      return;  // Salir si ya estaban guardados
  }
  snprintf(substrateKey, sizeof(substrateKey), "Higro_%d", currentMonth);
  snprintf(humidityKey, sizeof(humidityKey), "Humedad_%d", currentMonth);
  snprintf(tempKey, sizeof(tempKey), "Temp_%d", currentMonth);
  snprintf(flagSensorKey, sizeof(flagSensorKey), "FlagSens_%d", currentMonth);
  preferences.begin("sensor_data", false);  // Modo escritura
  size_t intArraySize = 31 * sizeof(int);
  size_t boolArraySize = 31 * sizeof(bool);
  // Leer antes de modificar
  if (preferences.isKey(substrateKey)) preferences.getBytes(substrateKey, substrateData, intArraySize);
  if (preferences.isKey(humidityKey)) preferences.getBytes(humidityKey, humidityData, intArraySize);
  if (preferences.isKey(tempKey)) preferences.getBytes(tempKey, tempData, intArraySize);
  if (preferences.isKey(flagSensorKey)) preferences.getBytes(flagSensorKey, dataStoredFlag, boolArraySize);
  // Guardar los datos en el array
  substrateData[currentDay - 1] = newSubstrate;
  humidityData[currentDay - 1] = newHumidity;
  tempData[currentDay - 1] = newTemp;
  dataStoredFlag[currentDay - 1] = true;
  // Guardar en la memoria
  preferences.putBytes(substrateKey, substrateData, intArraySize);
  preferences.putBytes(humidityKey, humidityData, intArraySize);
  preferences.putBytes(tempKey, tempData, intArraySize);
  preferences.putBytes(flagSensorKey, dataStoredFlag, boolArraySize);
  Serial.printf("📥 Datos del día %d almacenados en memoria.\n", currentDay);
  preferences.end();  // Cerrar memoria
  showMemoryStatus();
}
/* Storage Drip Data */
void storeDripData(int currentDay, int currentMonth, int currentHour, int currentMinute, bool dripActive) {
  // ✅ Solo se ejecuta en el minuto exacto en que termina el horario activo
  if (currentHour != endHour || currentMinute != endMinute) {
      return;  // Salir si no es el momento exacto
  }
  if (verifyDripStored(currentDay, currentMonth)) {
      Serial.printf("✅ Riego del día %d ya estaba almacenado, no se guarda de nuevo.\n", currentDay);
      return;  // Salir si ya se guardó
  }
  char dripKey[16];
  snprintf(dripKey, sizeof(dripKey), "Drip_%d", currentMonth);
  snprintf(flagDripKey, sizeof(flagDripKey), "FlagDrip_%d", currentMonth);
  preferences.begin("sensor_data", false);  // Modo escritura
  size_t boolArraySize = 31 * sizeof(bool);
  // Leer antes de modificar
  if (preferences.isKey(dripKey)) preferences.getBytes(dripKey, dripData, boolArraySize);
  if (preferences.isKey(flagDripKey)) preferences.getBytes(flagDripKey, dripStoredFlag, boolArraySize);
  // Guardar el estado del riego
  dripData[currentDay - 1] = dripActive;
  dripStoredFlag[currentDay - 1] = true;
  preferences.putBytes(dripKey, dripData, boolArraySize);
  preferences.putBytes(flagDripKey, dripStoredFlag, boolArraySize);
  Serial.printf("💾 Datos de riego almacenados para el día %d del mes %d: %s\n",
                currentDay, currentMonth, dripActive ? "Sí" : "No");
  preferences.end();  // Cerrar Preferences
  // Actualizar contador de días sin riego
  counterDripDays = dripActive ? 0 : counterDripDays + 1;
  lastDayDrip = currentDay;
  dripActived = false;
  if (counterDripDays == 25) {  
      Serial.println("⚠ Advertencia: Han pasado 25 días sin activarse el riego.");
  }
}
/* Stored Sensor Data Verification */
bool verifyStoredData(int day, int month) {
  snprintf(substrateKey, sizeof(substrateKey), "Higro_%d", month);
  snprintf(humidityKey, sizeof(humidityKey), "Humedad_%d", month);
  snprintf(tempKey, sizeof(tempKey), "Temp_%d", month);
  size_t dataSize = 31 * sizeof(int);
  preferences.begin("sensor_data", true);  // MODO LECTURA
  bool foundData = false;  // Bandera para saber si hay datos guardados
  if (preferences.isKey(substrateKey)) {
      preferences.getBytes(substrateKey, substrateData, dataSize);
      if (substrateData[day - 1] != -100) foundData = true;
  }
  if (preferences.isKey(humidityKey)) {
      preferences.getBytes(humidityKey, humidityData, dataSize);
      if (humidityData[day - 1] != -100) foundData = true;
  }
  if (preferences.isKey(tempKey)) {
      preferences.getBytes(tempKey, tempData, dataSize);
      if (tempData[day - 1] != -100) foundData = true;
  }
  preferences.end();  // Cerrar memoria
  return foundData;  // Devolver si hay datos o no
}
/* Stored Drip Verification */
bool verifyDripStored(int day, int month) {
  char dripKey[16];
  snprintf(dripKey, sizeof(dripKey), "Drip_%d", month);
  size_t boolArraySize = 31 * sizeof(bool);
  preferences.begin("sensor_data", true);  // Modo lectura
  bool stored = false;
  if (preferences.isKey(dripKey)) {
      preferences.getBytes(dripKey, dripData, boolArraySize);
      stored = dripData[day - 1];  // Ver si hay datos guardados para ese día
  }
  preferences.end();  // Cerrar memoria
  return stored;
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
/* Sender Monthly Mail */
void checkAndSendEmail() {
  int currentTotalMinutes = currentHour * 60 + currentMinute;
  int endTotalMinutes = endHour * 60 + endMinute;
  int lastDayOfMonth = getLastDayOfMonth(currentMonth, currentYear);
  if (currentDay == lastDayOfMonth && currentTotalMinutes > endTotalMinutes && !emailSentToday) { 
      dataMonthlyMessage = monthlyMessage(currentMonth);
      if (mailMonthData(dataMonthlyMessage)) {  // ✅ Solo borrar datos si el envío fue exitoso
          Serial.println("✅ Informe mensual enviado con éxito");
          cleanData();
          Serial.println("🗑 Datos eliminados después del informe mensual");
      } else {
          Serial.println("⚠️ Error en el envío del informe mensual, datos no eliminados");
      }
      emailSentToday = true;  
  }
  if (currentDay != lastDayOfMonth) {  
      emailSentToday = false;
  }
}
void backupStoreIfMissed(int currentDay, int currentMonth, int currentHour, int currentMinute) {
  if (!verifyStoredData(currentDay, currentMonth)) {
      Serial.println("⚠️ No se detectaron datos de sensores guardados hoy. Guardando valores actuales...");
      getHigroValues();
      getDHTValues();
      storeDailyData(currentDay, currentMonth, currentHour, currentMinute, substrateHumidity, humidity, temp);
  }

  if (!verifyDripStored(currentDay, currentMonth)) {
      Serial.println("⚠️ No se detectó estado de riego guardado hoy. Guardando estado actual...");
      bool drip = dripActived;
      storeDripData(currentDay, currentMonth, currentHour, currentMinute, drip);
  }
}
/* Get Last Day of the Month */
int getLastDayOfMonth(int month, int year) {
  switch (month) {
      case 1: case 3: case 5: case 7: case 8: case 10: case 12:
          return 31;
      case 4: case 6: case 9: case 11:
          return 30;
      case 2: // Verificamos si es un año bisiesto
          return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
      default:
          return 30;  // Fallback (no debería ocurrir)
  }
}
/* Monthly Data Message Maker */
String monthlyMessage(int month) {
  preferences.begin("sensor_data", true);
  emailBuffer[0] = '\0';
  // Obtener el día actual
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Error obteniendo la hora.");
    return "";
  }
  int today = timeinfo.tm_mday;  // Día actual
  // Inicializar arrays con -100 y false
  for (int i = 0; i < 31; i++) {
    substrateData[i] = -100;
    humidityData[i] = -100;
    tempData[i] = -100;
    dripData[i] = false;
  }
  // Leer datos almacenados
  snprintf(substrateKey, sizeof(substrateKey), "Higro_%d", month);
  snprintf(humidityKey, sizeof(humidityKey), "Humedad_%d", month);
  snprintf(tempKey, sizeof(tempKey), "Temp_%d", month);
  snprintf(dripKey, sizeof(dripKey), "Drip_%d", month);
  size_t intArraySize = 31 * sizeof(int);
  size_t boolArraySize = 31 * sizeof(bool);
  if (preferences.isKey(substrateKey)) preferences.getBytes(substrateKey, substrateData, intArraySize);
  if (preferences.isKey(humidityKey)) preferences.getBytes(humidityKey, humidityData, intArraySize);
  if (preferences.isKey(tempKey)) preferences.getBytes(tempKey, tempData, intArraySize);
  if (preferences.isKey(dripKey)) preferences.getBytes(dripKey, dripData, boolArraySize);
  preferences.end();
  // Variables de último valor válido
  int lastSubstrate = -100, lastHumidity = -100, lastTemp = -100;
  for (int day = 1; day <= today; day++) {  // 🔥 Solo hasta el día actual
      if (substrateData[day - 1] != -100) lastSubstrate = substrateData[day - 1];
      if (humidityData[day - 1] != -100) lastHumidity = humidityData[day - 1];
      if (tempData[day - 1] != -100) lastTemp = tempData[day - 1];
      // Verificar si hay datos
      bool hasData = (lastSubstrate != -100) || (lastHumidity != -100) || (lastTemp != -100) || dripData[day - 1];
      if (!hasData) continue;
      snprintf(lineBuffer, sizeof(lineBuffer),
               "Día %d: Riego: %s | Humedad sustrato: %d%% | Humedad ambiental: %d%% | Temp: %d°C\n",
               day, dripData[day - 1] ? "Sí" : "No", lastSubstrate, lastHumidity, lastTemp);
      strncat(emailBuffer, lineBuffer, sizeof(emailBuffer) - strlen(emailBuffer) - 1);
  }
  return String(emailBuffer);
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
  if(!smtp.connect(&session)){
    saveMailError("erSMTPServ", smtp.errorReason());                                 
    return; 
  }
  if(!MailClient.sendMail(&smtp, &mailStartSDS)){
    saveMailError("lastMailError", smtp.errorReason());
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
}
/* Mail Active Schedule */
void mailActiveSchedule(String message) {
  nowTime = rtc.getTime();
  date = rtc.getDate();
  currentMonth = rtc.getMonth() + 1;
  preferences.begin("sensor_data", true);                                          // Leer errores previos de la memoria
  showErrorMail = preferences.getString("lastMailError", " No mail errors ");
  showErrorMailConnect = preferences.getString("erSMTPServ", " No SMTP connect error ");
  preferences.end();
  bool sensoresGuardados = verifyStoredData(currentDay, currentMonth);
  bool riegoGuardado = verifyDripStored(currentDay, currentMonth);
  snprintf(textMsg, sizeof(textMsg),                                               // Construcción del mensaje
           "%s \n%s \n"
           "SmartDrip%s: Inicio de horario activo de riego. \n"
           "RTC: con fecha: %s | hora: %s\n"
           "Datos de configuración guardados: \n"
         "  - Tiempo de riego: %d min. \n"
         "  - Límite de humedad: %d%% \n"
         "  - Horario de riego: %s - %s\n"
         "  - Humedad sustrato: %d%% \n"
         "Datos del día %d:\n"
         "  - Sensores: %s\n"
         "  - Riego: %s\n\n"
         "Datos almacenados del mes %d:\n%s\n"
         "Estado de la memoria:\n"
         "  - Total: %d bytes\n"
         "  - Usada: %d bytes\n"
         "  - Libre: %d bytes\n"
         "Errores recientes en el envío de correos:\n"
         "  - Conexión SMTP: %s\n"
         "  - Envío de correo: %s\n",
         idSDHex.c_str(), idUser.c_str(), idSmartDrip.c_str(),
         date.c_str(), nowTime.c_str(),
         dripTimeLimit, dripHumidityLimit, startTime.c_str(), endTime.c_str(),
         substrateHumidity,
         currentDay,
         sensoresGuardados ? "Sí" : "No",
         riegoGuardado ? "Sí" : "No",
         currentMonth, message.c_str(),
         totalHeap, usedHeap, freeHeap,
         showErrorMailConnect.c_str(), showErrorMail.c_str());
  finalMessage = String(textMsg);                                                     
  mailActivSchedule.text.content = finalMessage.c_str();                              
  mailActivSchedule.text.charSet = "us-ascii";                                        
  mailActivSchedule.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;     
  mailActivSchedule.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal; 
  if (!smtp.connect(&session)) {                                                      // Intentar conectar al servidor SMTP
      saveMailError("erSMTPServ", smtp.errorReason());                                 
      return;                                                                         
  }                                                                                   
  if (!MailClient.sendMail(&smtp, &mailActivSchedule)) {                              // Intentar enviar el email
      saveMailError("lastMailError", smtp.errorReason());
  } else {
      Serial.println("✅ Correo enviado con éxito");
  }
  ESP_MAIL_PRINTF("💾 Memoria libre tras envío: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
  mailActiveScheduleCheck = true;
}
/* Mail No ACtive Schedule */
void mailNoActiveSchedule(String message){
  nowTime = rtc.getTime();
  date = rtc.getDate();
  currentMonth = rtc.getMonth() + 1;
  bool sensoresGuardados = verifyStoredData(currentDay, currentMonth);
  bool riegoGuardado = verifyDripStored(currentDay, currentMonth);
  snprintf(textMsg, sizeof(textMsg),
         "%s \n%s \n"
         "SmartDrip%s: Fuera de horario activo de riego. \n"
         "RTC: con fecha: %s | hora: %s\n"
         "Datos de configuración guardados: \n"
         "  - Tiempo de riego: %d min. \n"
         "  - Límite de humedad: %d%% \n"
         "  - Horario de riego: %s - %s\n"
         "  - Humedad sustrato: %d%% \n"
         "Datos del día %d:\n"
         "  - Sensores: %s\n"
         "  - Riego: %s\n\n"
         "Datos almacenados del mes %d:\n%s\n"
         "Estado de la memoria:\n"
         "  - Total: %d bytes\n"
         "  - Usada: %d bytes\n"
         "  - Libre: %d bytes\n"
         "Errores recientes en el envío de correos:\n"
         "  - Conexión SMTP: %s\n"
         "  - Envío de correo: %s\n",
         idSDHex.c_str(), idUser.c_str(), idSmartDrip.c_str(),
         date.c_str(), nowTime.c_str(),
         dripTimeLimit, dripHumidityLimit, startTime.c_str(), endTime.c_str(),
         substrateHumidity,
         currentDay,
         sensoresGuardados ? "Sí" : "No",
         riegoGuardado ? "Sí" : "No",
         currentMonth, message.c_str(),
         totalHeap, usedHeap, freeHeap,
         showErrorMailConnect.c_str(), showErrorMail.c_str());
  finalMessage = String(textMsg);                         
  mailNoActivSchedule.text.content = finalMessage.c_str();
  mailNoActivSchedule.text.charSet = "us-ascii";
  mailNoActivSchedule.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailNoActivSchedule.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session)){
    saveMailError("erSMTPServ", smtp.errorReason());                                   
    return;
  }
  if(!MailClient.sendMail(&smtp, &mailNoActivSchedule)){
    saveMailError("lastMailError", smtp.errorReason());
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
  if(!smtp.connect(&session)){
    saveMailError("erSMTPServ", smtp.errorReason());                                   
    return;
  }
  if(!MailClient.sendMail(&smtp, &mailDripOn)){
    saveMailError("lastMailError", smtp.errorReason());
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  mailDripOnSended = true;
  smtp.closeSession();
}
/* Mail Drip Off */
void mailSmartDripOff(){
  nowTime = rtc.getTime();  //Probar si no es necesario actualizar hora y fecha para el envío del mail  
  date = rtc.getDate(); 
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "Con fecha: %s\n"
           "Riego finalizado correctamente en Smart Drip%s a las: %s\n"
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
  mailDripOff.text.content = finalMessage.c_str();
  mailDripOff.text.charSet = "us-ascii";
  mailDripOff.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailDripOff.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session)){
    saveMailError("erSMTPServ", smtp.errorReason());                                   
    return;
  }
  if(!MailClient.sendMail(&smtp, &mailDripOff)){
    saveMailError("lastMailError", smtp.errorReason());
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  mailDripOffSended = true;
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
  if(!smtp.connect(&session)){
    saveMailError("erSMTPServ", smtp.errorReason());                                   
    return;
  }
  if(!MailClient.sendMail(&smtp, &mailErrValve)){
    saveMailError("lastMailError", smtp.errorReason());
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
  if(!smtp.connect(&session)){
    saveMailError("erSMTPServ", smtp.errorReason());                                   
    return;
  }
  if(!MailClient.sendMail(&smtp, &mailErrorDHT)){
    saveMailError("lastMailError", smtp.errorReason());
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
  if(!smtp.connect(&session)){
    saveMailError("erSMTPServ", smtp.errorReason());                                   
    return;
  }
  if(!MailClient.sendMail(&smtp, &mailErrorHigro)){
    saveMailError("lastMailError", smtp.errorReason());
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
  if(!smtp.connect(&session)){
    saveMailError("erSMTPServ", smtp.errorReason());                                   
    return;
  }
  if(!MailClient.sendMail(&smtp, &mailCalibratSensor)){
    saveMailError("lastMailError", smtp.errorReason());
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
bool mailMonthData(String message) {
  date = rtc.getDate();
  nowTime = rtc.getHour();
  int prevMonth = currentMonth - 1;          // Obtener el mes anterior al actual
  if (prevMonth == 0) prevMonth = 12;        
  snprintf(textMsg, sizeof(textMsg),
           "📩 *Informe mensual de Smart Drip*\n\n"
           "📌 **Dispositivo:** %s\n"
           "👤 **Usuario:** %s\n"
           "🔢 **ID Smart Drip:** %s\n"
           "📅 **Fecha de envío:** %s\n"
           "📊 **Datos correspondientes a:** %s\n\n"
           "%s\n",
           idSDHex.c_str(),                  // ID del SD
           idUser.c_str(),                   // ID del usuario
           idSmartDrip.c_str(),              // ID del Smart Drip
           date.c_str(),                     // Fecha del envío
           getMonthName(prevMonth).c_str(),  // Nombre del mes anterior
           message.c_str());                 // Mensaje mensual con los datos
  finalMessage = String(textMsg);            // Convertir a String para el envío
  mailMonthlyData.text.content = finalMessage.c_str();
  mailMonthlyData.text.charSet = "us-ascii";
  mailMonthlyData.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailMonthlyData.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if (!smtp.connect(&session)) {
    saveMailError("erSMTPServ", smtp.errorReason());                                   
    return false;  // ❌ Falló la conexión SMTP
  }
  if (!MailClient.sendMail(&smtp, &mailMonthlyData)) {
    saveMailError("lastMailError", smtp.errorReason());
    return false;  // ❌ Falló el envío del correo
  } else {
    Serial.println("✅ Correo mensual enviado con éxito.");
  }
  ESP_MAIL_PRINTF("🛠 Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  smtp.closeSession();
  return true;  // ✅ Envío exitoso
}
/* Get Month Name */
String getMonthName(int month) {
  const char* months[] = {"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
                          "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};
  return (month >= 1 && month <= 12) ? String(months[month - 1]) : "Desconocido";
}
/* Save SMTP errors in memory */
void saveMailError(const char *key, String newError) {
  preferences.begin("sensor_data", false);             // Modo escritura
  String storedError = preferences.getString(key, ""); // Leer el error previo almacenado
  if (storedError != newError) {                       // Guardar solo si el nuevo error es diferente del anterior
      preferences.putString(key, newError);
      Serial.println("⚠ Nuevo error guardado en memoria:");
      Serial.println(newError);
  } else {
      Serial.println("⚠ Error detectado, pero ya estaba registrado. No se sobrescribe.");
  }
  preferences.end();  // Cerrar memoria
}
/* Clean Data Preferences */
void cleanData() {
  preferences.begin("sensor_data", false);  // Modo escritura
  // Eliminar arrays mensuales por clave (basado en mes actual)
  char key[20];
  snprintf(key, sizeof(key), "Higro_%d", currentMonth);
  preferences.remove(key);
  snprintf(key, sizeof(key), "Humedad_%d", currentMonth);
  preferences.remove(key);
  snprintf(key, sizeof(key), "Temp_%d", currentMonth);
  preferences.remove(key);
  snprintf(key, sizeof(key), "Drip_%d", currentMonth);
  preferences.remove(key);
  snprintf(key, sizeof(key), "FlagSens_%d", currentMonth);
  preferences.remove(key);
  snprintf(key, sizeof(key), "FlagDrip_%d", currentMonth);
  preferences.remove(key);
  preferences.end();
  // Resetear arrays en RAM
  memset(substrateData, -100, sizeof(substrateData));
  memset(humidityData, -100, sizeof(humidityData));
  memset(tempData, -100, sizeof(tempData));
  memset(dripData, false, sizeof(dripData));
  memset(dataStoredFlag, false, sizeof(dataStoredFlag));
  memset(dripStoredFlag, false, sizeof(dripStoredFlag));
  Serial.printf("🗑 Datos del mes %d eliminados de Preferences.\n", currentMonth);
  Serial.println("🔄 Arrays reiniciados en RAM.");
}