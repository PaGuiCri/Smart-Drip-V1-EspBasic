#include <Arduino.h>
#include <SimpleDHT.h>
#include <NTPClient.h>
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
void mailSmartDripOn();
void mailStartSystem();
void mailErrorValve();
void mailErrorDHT11();
void mailErrorSensorHigro();
void mailActiveSchedule();
void mailNoActiveSchedule();
void mailWeekData(String mesage);
ESP_Mail_Session session;
SMTP_Message mailStartSDS;
SMTP_Message mailDripOn;
SMTP_Message mailErrValve;
SMTP_Message mailErrorFlowSensor;
SMTP_Message mailErrorDHT;
SMTP_Message mailErrorHigro;
SMTP_Message mailActivSchedule; 
SMTP_Message mailNoActivSchedule;
SMTP_Message mailWeeklyData;
bool mailDripOnSended = false;
bool mailErrorValveSended = false;
bool mailErrorDHTSended = false;
bool mailActiveScheduleCheck = false;
bool mailNoActiveScheduleCheck = false;

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
String startHourStr, startMinuteStr, endHourStr, endMinuteStr;
int startHour, startMinute, endHour, endMinute, dayWeek;
int currentHour, currentMinute;
bool withinSchedule = false;
void extractTimeValues();
void createID();
/* Sincronización del RTC con NTP */
const char* ntpServer = "pool.ntp.org";
const long gmtOFFset_sec = 3600;
const int daylightOffset_sec = 3600;

/* Configuración de terminales para higrómetro y DHT11  */
void getDHTValues();
void getHigroValues();
void handleDrip();
void handleOutOfScheduleDrip();
void finalizeDrip();
#define PinHigro 34  // Nueva configuración de pines antes 34. volvemos al pin 34 desde el 13
#define PinDHT 4     // El pin del sensor DHT tiene q ser el 4 si se trabaja con la biblioteca SimpleDHT
float temp = 0, humidity = 0;
SimpleDHT11 DHT(PinDHT);
unsigned long TiempoDHT = 0;
#define SampleDHT 1200
int getHigroValue = 0;
int substrateHumidity = 0;
int counter = 0;
bool outputEstatus = false;
const int dry = 445;
const int wet = 2;                   // Si se incrementa, el máximo (100%) sera mayor y viceversa
int dripTime, dripTimeCheck = 0;
int dripHumidity, dripHumidityCheck = 0;
int dripTimeLimit = 5;
int dripHumidityLimit = 45;  

/* Variables para el cálculo del caudal */
volatile int pulses = 0;
float caudal = 0.0;
float waterVolume = 0.0; // *** redefinir variables de caudal de agua
float totalLitros = 0.0;
unsigned long oldTime = 0;
bool flowMeterEstatus = false;
bool flowSensorEnabled = false;  // Habilita o deshabilita el sensor de flujo de caudal
/* Interrupción llamada cada vez que se detecta un pulso del sensor */
void flowMeter();
void pulseCounter(){
  pulses++;
}
/* Checking Active Schedule */
bool isWithinSchedule(int currentHour, int currentMinute);
/* Instancia para almacenar datos de riego en memoria flash */
Preferences preferences;
unsigned long currentMillis, previousMillis = 0;
const unsigned long intervalDay = 86400000; // 1 día en milisegundos (24 horas)
/* Contadores de días y semanas */ 
int dayCounter = 1;    // es necesario?
/* Array con los nombres de los días de la semana */ 
const char* diasSemana[] = {"Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo"};
/* Mensaje semanal */
void weeklyMesage();
void cleanData();
#define dripValveVin1 27  // Nueva configuración de pines antes 32. Salida Electroválvula 1
#define dripValveGND1 26  // Nueva configuración de pines antes 25. Salida Electroválvula 1
#define dripValveVin2 25  // Segunda válvula opcional
#define dripValveGND2 33  // Segunda válvula opcional
#define flowSensor  13    // Nueva configuración de pines antes 20 pendiente test pin 13
#define pinLed 2          // pin para señal luminosa desde la caja

bool AUTO, Ok, dripValve, reset, activePulse;
bool dhtOk, dhtOkCheck, checkTimer, dripActived = false;
/* Pulse Variables */
const unsigned long pulseTime = 50; // Duración del pulso en milisegundos = 50ms
unsigned long startTimePulse = 0;
int closeValveCounter = 10;

void setup() {
  Serial.begin(9600);
  /* Inicio preferences */
  preferences.begin("sensor_data", false);
  idNumber = preferences.getUInt("device_id", 0); // Obtener el id único del dispositivo almacenado
  /* Creación de ID único */
  createID();
  Serial.print("ID único CRC32: ");
  Serial.println(idNumber, HEX);  // Muestra el id único del dispositivo en formato hexadecimal
  idSDHex += String(idNumber, HEX);
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
  pinMode(pinLed, OUTPUT);
  /* Configuración de la interrupción para detectar los pulsos del sensor de flujo */
  attachInterrupt(digitalPinToInterrupt(flowSensor), pulseCounter, FALLING);
  /* Temporizador */
  timer1 = timerBegin(0, 80, true);
  timerAttachInterrupt(timer1, &onTimer1, true);
  timerAlarmWrite(timer1, 1000000, true);
  timerAlarmEnable(timer1);
  timerAlarmDisable(timer1);
  /* Configuración de emails */
  smtp.debug(1);
  smtp.callback(smtpCallback);
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
  mailWeeklyData.sender.name = "Smart Drip System";
  mailWeeklyData.sender.email = AUTHOR_EMAIL;
  mailWeeklyData.subject = "Mail semanal de humedades";
  mailWeeklyData.addRecipient("Pablo", "falder24@gmail.com"); 
  stopPulse();
  getHigroValues();
  mailStartSystem();
}
void loop() {
  /* Verificar cada hora la conexión WiFi y reconecta si se ha perdido */
  handleWiFiReconnection();
  /* Extraer valores de tiempo actual */
  extractTimeValues();
  /* Comprobación de horario */
  currentHour = rtc.getHour(true); // Obtenemos la hora actual. True para formato 24h  
  currentMinute = rtc.getMinute();
  Serial.print("Hora actual: ");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.print(currentMinute);
  Serial.println();
  bool withinSchedule = isWithinSchedule(currentHour, currentMinute);   // True para formato de 24h
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
    Serial.println("Irrigation process underway");
  }
    // Mostrar los valores actuales de tiempo y humedad de riego
  Serial.print("Irrigation time: ");
  Serial.println(dripTime);
  Serial.print("Irrigation humidity: ");
  Serial.println(dripHumidity);
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
  startHourStr = startTime.substring(0, 2);
  startMinuteStr = startTime.substring(3, 5);
  endHourStr = endTime.substring(0, 2);
  endMinuteStr = endTime.substring(3, 5);
  startHour = startHourStr.toInt();
  startMinute = startMinuteStr.toInt();
  endHour = endHourStr.toInt();
  endMinute = endMinuteStr.toInt();
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
  if (!mailActiveScheduleCheck) {  
    mailActiveSchedule();  // Envío mail horario de riego activo - desactivado
  }
  if (currentMillis - previousMillis >= intervalDay) {
    previousMillis = currentMillis;
    // Lee los datos de los sensores si es necesario        
    // Guarda los datos en la memoria no volátil
    preferences.putInt(("Humedad_day" + String(dayWeek)).c_str(), substrateHumidity);
    preferences.putBool(("Riego_day" + String(dayWeek)).c_str(), dripActived);
    preferences.putInt("dayWeek", dayWeek);
    dripActived = false;
    if (dayWeek >= 7) {  // Cuando es domingo se crea y se envía el mensaje semanal con la información sobre el estado de Smart Drip
      weeklyMesage();
      dayWeek = 1;
      preferences.putInt("dayWeek", dayWeek);
    }
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
      if (!mailDripOnSended) {  
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
  if (!mailNoActiveScheduleCheck) {
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
/* Getting Higro Measurements */
void getHigroValues(){
  getHigroValue = analogRead(PinHigro);
  substrateHumidity = map(getHigroValue, wet, dry, 100, 0);
  Serial.print("Substrate humidity: "); 
  Serial.println(substrateHumidity + "%"); 
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
}
/* Flow meter */
void flowMeter(){
  /* Cálculo del caudal cada segundo */
  if ((millis() - oldTime) > 1000){
    /* Desactiva las interrupciones mientras se realiza el cálculo */
    detachInterrupt(digitalPinToInterrupt(flowSensor));
    Serial.print("Pulsos: ");
    Serial.println(pulses);
    /* Calcula el caudal en litros por minuto */
    caudal = pulses / 5.5;                         // factor de conversión, siendo K=7.5 para el sensor de ½”, K=5.5 para el sensor de ¾” y 3.5 para el sensor de 1”
    // Reinicia el contador de pulsos
    pulses = 0;
    // Calcula el volumen de agua en mililitros
    waterVolume = (caudal / 60) * 1000/1000;
    // Incrementa el volumen total acumulado
    totalLitros += waterVolume;
    // Activa las interrupciones nuevamente
    attachInterrupt(digitalPinToInterrupt(flowSensor), pulseCounter, FALLING);
    // Actualiza el tiempo anterior
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
  const unsigned long interval = 5000;  // 5 segundos
  const unsigned long waitTime = 15000;  // 15 segundos para dar tiempo al WiFi
  // Continuar mientras no esté conectado y no se hayan agotado los intentos
  while (state != WL_CONNECTED && tries < MAX_CONNECT) {
    // Verificar si han pasado 5 segundos
    if (millis() - initTime >= interval) {
      Serial.print(".");
      Serial.print("Intento de conectar a la red WiFi: " + String(SSID) + " ");
      Serial.print(tries + 1);
      Serial.println(" de conexión...");
      // Verificar si el tiempo de espera total ha pasado para intentar reconectar
      if (millis() - initTime >= waitTime) {
        WiFi.reconnect();
        initTime = millis(); // Reiniciar el temporizador solo después de reconectar
      }
      state = WiFi.status();
      tries++;
    }
    // Aquí puedes ejecutar otras tareas mientras esperas
  }
  // Verificar si la conexión fue exitosa
  if (state == WL_CONNECTED) {
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
    lastConnectionTry = millis();   // Actualizar el tiempo del último chequeo
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
  if (!getLocalTime(&timeinfo)) {  // Obtener la hora local
    Serial.println("Error al obtener la hora");
    return;
  }
  Serial.println("Hora sincronizada con NTP:");
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");  // Mostrar la fecha y hora formateada
  // Sincronizar el RTC del ESP32 con la hora local
  rtc.setTimeStruct(timeinfo);  // Establecer el RTC con la estructura de tiempo obtenida
  Serial.print("Hora configurada en RTC: ");
  Serial.println(rtc.getTime("%A, %B %d %Y %H:%M:%S"));  // Mostrar la hora en formato legible
  nowTime = rtc.getTime();
  date = rtc.getDate();
  dayWeek = rtc.getDayofWeek() == 0 ? 7 : rtc.getDayofWeek();
  if(dayWeek == 7){
    dayWeek = preferences.getInt("dayWeek", dayWeek);
  }
}
/* Mail Start System */
void mailStartSystem(){
  String textMsg = idSDHex + " \n" + idUser + " \n"
                   " SmartDrip" + idSmartDrip + " conectado a la red y en funcionamiento. \n"  // Nuevo diseño del mail para mejorar su visualización
                   " Datos de configuración guardados: \n"
                   " Tiempo de riego: " + String(dripTimeLimit) + "min. \n"
                   " Limite de humedad de riego: " + String(dripHumidityLimit) + "% \n"
                   " Horario de activación de riego: \n"
                   " Hora de inicio: " + String(startTime) + "\n"
                   " Hora de fin: " + String(endTime) + "\n"
                   " Humedad sustrato: " + String(substrateHumidity) + "\n";
  mailStartSDS.text.content = textMsg.c_str();
  mailStartSDS.text.charSet = "us-ascii";
  mailStartSDS.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailStartSDS.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    return;
  if(!MailClient.sendMail(&smtp, &mailStartSDS)){
    Serial.println("Error envío Email, " + smtp.errorReason());
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
  String textMsg = idSDHex + " \n" + idUser + " \n"
                   " SmartDrip" + idSmartDrip + ": inicia horario activo de riego. \n"
                   " RTC: con fecha: " + String(date) + "\n"
                   "      hora: " + String(nowTime) + "\n" // Nuevo diseño del mail para mejorar su visualización
                   " Datos de configuración guardados: \n"
                   " Tiempo de riego: " + String(dripTimeLimit) + "min. \n"
                   " Limite de humedad de riego: " + String(dripHumidity) + "% \n"
                   " Horario de activación de riego: \n"
                   " Hora de inicio: " + String(startTime) + "\n"
                   " Hora de fin: " + String(endTime) + "\n"
                   " Humedad sustrato: " + String(substrateHumidity) + "\n";
  mailStartSDS.text.content = textMsg.c_str();
  mailStartSDS.text.charSet = "us-ascii";
  mailStartSDS.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailStartSDS.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    return;
  if(!MailClient.sendMail(&smtp, &mailStartSDS)){
    Serial.println("Error envío Email, " + smtp.errorReason());
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
  String textMsg = idSDHex + " \n" + idUser + " \n"
                   " SmartDrip " + idSmartDrip + ": Fuera horario activo de riego. \n" 
                   " RTC: con fecha: " + String(date) + "\n"
                   "      hora: " + String(nowTime) + "\n" // Nuevo diseño del mail para mejorar su visualización
                   " Datos de configuración guardados: \n"
                   " Tiempo de riego: " + String(dripTimeLimit) + "min. \n"
                   " Limite de humedad de riego: " + String(dripHumidity) + "% \n"
                   " Horario de activación de riego: \n"
                   " Hora de inicio: " + String(startTime) + "\n"
                   " Hora de fin: " + String(endTime) + "\n"
                   " Humedad sustrato: " + String(substrateHumidity) + "\n";
  mailStartSDS.text.content = textMsg.c_str();
  mailStartSDS.text.charSet = "us-ascii";
  mailStartSDS.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailStartSDS.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    return;
  if(!MailClient.sendMail(&smtp, &mailStartSDS)){
    Serial.println("Error envío Email, " + smtp.errorReason());
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
  String textMsg = idSDHex + " \n" + idUser + " \n"
                   " Con fecha: " + date + "\n"
                   " Riego conectado correctamente en Smart Drip" + idSmartDrip + " a las: " + nowTime + "\n"
                   " Tiempo de riego: " + String(dripTimeLimit) + "min. \n"
                   " Limite de humedad de riego: " + String(dripHumidity) + "% \n"
                   " Humedad sustrato: " + String(substrateHumidity) + "\n";
  mailDripOn.text.content = textMsg.c_str();
  mailDripOn.text.charSet = "us-ascii";
  mailDripOn.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailDripOn.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;

  if(!smtp.connect(&session))
    return;

  if(!MailClient.sendMail(&smtp, &mailDripOn)){
    Serial.println("Error envío Email, " + smtp.errorReason());
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  mailDripOnSended = true;
  smtp.closeSession();
}
/* Mail Solenoid Valve Error */
void mailErrorValve(){
  String textMsg = idSDHex + " \n" + idUser + " \n"
                   "Error en la electrovávula de riego del Smart Drip" + idSmartDrip + "\n"
                   "Se detiene el proceso de riego automático. \n"
                   "Los sensores indican que el agua continúa fluyendo. \n"
                   "Por favor revise la instalación lo antes posible.";
  mailErrValve.text.content = textMsg.c_str();
  mailErrValve.text.charSet = "us-ascii";
  mailErrValve.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrValve.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;

   if(!smtp.connect(&session))
    return;

  if(!MailClient.sendMail(&smtp, &mailErrValve)){
    Serial.println("Error envío Email, " + smtp.errorReason());
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  mailErrorValveSended = true;
  smtp.closeSession();
} 
/* Mail DHT Sensor Error */
void mailErrorDHT11(){
  String textMsg = idSDHex + " \n" + idUser + " \n"
                   " El sensor de datos ambientales del Smart Drip " + idSmartDrip + " está desconectado o dañado \n"
                   " proceda a su inspección o llame al servicio técnico \n";
  mailErrorDHT.text.content = textMsg.c_str();
  mailErrorDHT.text.charSet = "us-ascii";
  mailErrorDHT.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrorDHT.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;

  if(!smtp.connect(&session))
    return;

  if(!MailClient.sendMail(&smtp, &mailErrorDHT)){
    Serial.println("Error envío Email, " + smtp.errorReason());
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap());
  mailErrorDHTSended = true;
  smtp.closeSession();
}
/* Mail Hygro Error */
void mailErrorSensorHigro(){
  String textMsg = idSDHex + " \n" + idUser + " \n"
                   " El sensor de humedad del sustrato del Smart Drip" + idSmartDrip + " está fuera de rango o dañado \n"
                   " proceda a su inspección o llame al servicio técnico \n";
  mailErrorHigro.text.content = textMsg.c_str();
  mailErrorHigro.text.charSet = "us-ascii";
  mailErrorHigro.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrorHigro.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    return;
  if(!MailClient.sendMail(&smtp, &mailErrorHigro)){
    Serial.println("Error envío Email, " + smtp.errorReason());
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
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
/* Data Weekly Mesage */
void weeklyMesage(){
  String mesage = "";
  for(int i = 1; i <= 7; i++){
    int sensor1Data = preferences.getInt(("Humedad_day" + String(i)).c_str(), 0);
    bool drip = preferences.getBool(("Riego_day" + String(i)).c_str(), false);
    // Añade los datos al mensaje con el nombre del día correspondiente
    mesage += String(diasSemana[i-1]) + ": Humedad = " + String(sensor1Data) + ", Riego = " + (drip ? "Activado" : "No Activado") + "\n";
  }
  // Llama a la función de envío de correo
  mailWeekData(mesage);
  // Limpia los datos después de enviar el correo
  cleanData();
}
/* Mail Data Weekly */
void mailWeekData(String mesage){
  date = rtc.getDate();
  String textMsg = idSDHex + " \n" + idUser + " \n"
                   " Mensaje semanal de comprobación de humedades del sustrato del Smart Drip " + idSmartDrip + " \n" 
                   + " con fecha " + String(date) + " los datos son: " + " \n"
                   + String(mesage) + "\n";
  mailErrorHigro.text.content = textMsg.c_str();
  mailErrorHigro.text.charSet = "us-ascii";
  mailErrorHigro.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrorHigro.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  if(!smtp.connect(&session))
    return;
  if(!MailClient.sendMail(&smtp, &mailErrorHigro)){
    Serial.println("Error envío Email, " + smtp.errorReason());
  }else{
    Serial.println("Correo enviado con exito");
  }
  ESP_MAIL_PRINTF("Liberar memoria: %d/n", MailClient.getFreeHeap()); 
  smtp.closeSession();
}
/* Clean Data Preferences */
void cleanData(){
  for(int i = 1; i <= 7; i++){
     preferences.remove(("Humedad_day" + String(i)).c_str());
     preferences.remove(("Riego_day" + String(i)).c_str());
  }
}