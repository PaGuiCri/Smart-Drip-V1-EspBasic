#ifndef MAIL_MANAGER_H
#define MAIL_MANAGER_H

#include <Arduino.h>
#include <ESP_Mail_Client.h>
#include <ESP32Time.h>

// 📡 CONFIGURACIÓN SMTP
void configureSmtpSession();
void setupMail(SMTP_Message &msg, const char *subject);
void smtpCallback(SMTP_Status status);
// 📨 FUNCIONES DE ENVÍO DE CORREO
void mailStartSystem(const String& smartDripId, const String& userId, const String& sdHex,
    int tiempoRiego, int humedadLimite, const String& horaInicio,
    const String& horaFin, int humedadSustrato, const String& resumenErrores);
void mailActiveSchedule(const String& smartDripId, const String& userId, const String& sdHex, const String& currentDate,
    const String& currentTime, int currentMonth, int currentDay, int tiempoRiego, int humedadLimite, const String& horaInicio,
    const String& horaFin, int humedadSustrato, bool datosGuardados, bool huboRiego, const String& resumenMes, size_t heapTotal,
    size_t heapUsada, size_t heapLibre, const String& erroresResumen);     
void mailNoActiveSchedule(const String& idSmartDrip, const String& idUser, const String& idSDHex, const String& date, const String& nowTime,
    int currentMonth, int currentDay, int dripTimeLimit, int dripHumidityLimit, const String& startTime, const String& endTime,
    int substrateHumidity, bool sensoresActivos, bool dripActived, const String& resumenMensual, size_t totalHeap, size_t usedHeap,
    size_t freeHeap, const String& showErrorSummary);
void mailSmartDripOn(const String& user, const String& device, const String& hexId, const String& fecha, const String& hora, int tiempo,
        int humedadLimite, int humedadSustrato);
void mailSmartDripOff(const String& user, const String& device, const String& hexId, const String& fecha, const String& hora, int tiempoRiego,
    int limiteHumedad, int humedadSustrato, float humedadAmbiente, float temperatura);
void mailAnnualReport(const String& deviceHexId, const String& userId, const String& deviceId, int anio);
// ⚠️ FUNCIONES DE ERRORES
void mailErrorValve(const String& deviceHexId, const String& userId, const String& deviceId);
void mailErrorFlowSensor(const String& idSDHex, const String& idUser, const String& idSmartDrip);
void mailErrorDHT11(const String& idSDHex, const String& idUser, const String& idSmartDrip);
void mailErrorSensorHigro(const String& idSDHex, const String& idUser, const String& idSmartDrip);

// SMTP y sesión
extern SMTPSession smtp;
extern ESP_Mail_Session session;
extern String smtpServer;
extern int smtpPort;
extern String smtpEmail;
extern String smtpPass;

// Mensajes de correo
extern SMTP_Message mailStartSDS;
extern SMTP_Message mailDripOn;
extern SMTP_Message mailDripOff;
extern SMTP_Message mailErrValve;
extern SMTP_Message mailErrFlowSensor;
extern SMTP_Message mailErrorDHT;
extern SMTP_Message mailErrorHigro;
extern SMTP_Message mailActivSchedule;
extern SMTP_Message mailNoActivSchedule;
extern SMTP_Message mailAnualReport;

// Flags de envío completado
extern bool mailStartSystemSended;
extern bool mailActiveScheduleSended;
extern bool mailNoActiveScheduleSended;
extern bool mailDripOnSended;
extern bool mailDripOffSended;
extern bool mailErrorValveSended;
extern bool mailErrorDHTSended;
extern bool mailErrorHigroSended;
extern bool mailErrorFlowSensorSended;
extern bool mailAnnualReportSended;

// Flags de activación por usuario
extern bool mailStartSystemActive;
extern bool mailActiveScheduleActive;
extern bool mailNoActiveScheduleActive;
extern bool mailSmartDripOnActive;
extern bool mailSmartDripOffActive;
extern bool mailErrorValveActive;
extern bool mailErrorDHTActive;
extern bool mailErrorHigroActive;
extern bool mailErrorFlowSensorActive;
extern bool mailAnnualReportActive;

// Errores
extern String showErrorMail;
extern String showErrorMailConnect;
extern String showErrorWiFi;
extern String showErrorSummary;
extern String fechaSMTP;
extern String fechaEnvio;
extern String fechaWiFi;
extern int smtpCount;
extern int envioCount;
extern int wifiCount;
extern char errorBuffer[512];

// Depuración
extern bool debugSmtp;

#endif
