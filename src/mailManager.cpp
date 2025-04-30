#include "mailManager.h"
#include "configManager.h"
#include "storageManager.h"
#include <ESP_Mail_Client.h>
#include <ESP32Time.h>
#include "time.h"

// 🔄 Instancias globales de email
SMTPSession smtp;
ESP_Mail_Session session;

// 📨 Instancias de correo
SMTP_Message mailStartSDS, mailDripOn, mailDripOff, mailErrValve;
SMTP_Message mailErrFlowSensor, mailErrorDHT, mailErrorHigro;
SMTP_Message mailActivSchedule, mailNoActivSchedule, mailAnualReport;

// Flags de envío completado
bool mailStartSystemSended = false;
bool mailActiveScheduleSended = false;
bool mailNoActiveScheduleSended = false;
bool mailDripOnSended = false;
bool mailDripOffSended = false;
bool mailErrorValveSended = false;
bool mailErrorDHTSended = false;
bool mailErrorHigroSended = false;
bool mailErrorFlowSensorSended = false;
bool mailAnnualReportSended = false;
// Flags de activación por usuario
bool mailStartSystemActive = true;
bool mailActiveScheduleActive = true;
bool mailNoActiveScheduleActive = true;
bool mailSmartDripOnActive = true;
bool mailSmartDripOffActive = true;
bool mailErrorValveActive = true;
bool mailErrorDHTActive = false;
bool mailErrorHigroActive = false;
bool mailErrorFlowSensorActive = false;
bool mailAnnualReportActive = true;
// ⚠️ Errores
String showErrorMail, showErrorMailConnect, showErrorWiFi, showErrorSummary;
String fechaSMTP, fechaEnvio, fechaWiFi;
int smtpCount = 0, envioCount = 0, wifiCount = 0;
char errorBuffer[512];
// 🧪 Depuración
bool debugSmtp = false;
// 💬 Mensaje temporal
char textMsg[2048];
String finalMessage;
// 📡 CONFIGURACIÓN SMTP
void configureSmtpSession() {
  session.server.host_name = smtpServer.c_str();
  session.server.port = smtpPort;
  session.login.email = smtpEmail.c_str();
  session.login.password = smtpPass.c_str();
  session.login.user_domain = "";
  session.time.ntp_server = "pool.ntp.org";
  session.time.gmt_offset = 1;
  session.time.day_light_offset = 1;

  if (debugSmtp) {
    smtp.debug(1);
    smtp.callback(smtpCallback);
  }
}
// 📨 CONFIGURAR MENSAJE
void setupMail(SMTP_Message& msg, const char* subject) {
  msg.sender.name = "Smart Drip System";
  msg.sender.email = smtpEmail.c_str();
  msg.subject = subject;
  msg.addRecipient(idUser.c_str(), smtpEmail.c_str());
}
// 🔄 CALLBACK DE ESTADO
void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
  if (status.success()) {
    Serial.println("📬 Resultado del envío:");
    ESP_MAIL_PRINTF("✓ Mensajes completados: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("✗ Fallidos: %d\n", status.failedCount());
    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      struct tm dt;
      localtime_r(&ts, &dt);
      ESP_MAIL_PRINTF("→ #%d [%s] %d/%d/%d %02d:%02d\n",
                      i + 1,
                      result.completed ? "OK" : "ERR",
                      dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday,
                      dt.tm_hour, dt.tm_min);
      ESP_MAIL_PRINTF("   📬 A: %s | 📨 Asunto: %s\n", result.recipients, result.subject);
    }
  }
}

// 📆 Devuelve el nombre de un mes
String getMonthName(int month) {
  const char* months[] = {
    "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
    "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"
  };
  return (month >= 1 && month <= 12) ? String(months[month - 1]) : "Desconocido";
}
void mailStartSystem(const String& smartDripId, const String& userId, const String& sdHex,
  int tiempoRiego, int humedadLimite, const String& horaInicio,
  const String& horaFin, int humedadSustrato, const String& resumenErrores) {
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
    "✅ El sistema está listo y en funcionamiento.\n\n"
    "⚠️ *Errores recientes en el sistema:*\n%s\n",
    smartDripId.c_str(), userId.c_str(), sdHex.c_str(),
    tiempoRiego, humedadLimite,
    horaInicio.c_str(), horaFin.c_str(),
    humedadSustrato, resumenErrores.c_str());
  finalMessage = String(textMsg);
  mailStartSDS.text.content = finalMessage.c_str();
  mailStartSDS.text.charSet = "us-ascii";
  mailStartSDS.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailStartSDS.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  configureSmtpSession();
  setupMail(mailStartSDS, "Inicio Smart Drip System");
if (!smtp.connect(&session)) {
  updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
  return;
}
if (!MailClient.sendMail(&smtp, &mailStartSDS)) {
  updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
} else {
  Serial.println("📩 Mail de inicio de sistema enviado.");
  mailStartSystemSended = true;
}
ESP_MAIL_PRINTF("🧠 Memoria tras envío: %d\n", MailClient.getFreeHeap());
smtp.closeSession();
}
void mailActiveSchedule(const String& smartDripId, const String& userId, const String& sdHex, const String& currentDate, const String& currentTime,
  int currentMonth, int currentDay, int tiempoRiego, int humedadLimite, const String& horaInicio, const String& horaFin, int humedadSustrato,
  bool datosGuardados, bool huboRiego, const String& resumenMes, size_t heapTotal, size_t heapUsada, size_t heapLibre, const String& erroresResumen) {
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
    smartDripId.c_str(), userId.c_str(), sdHex.c_str(),
    currentDate.c_str(), currentTime.c_str(),
    tiempoRiego, humedadLimite,
    horaInicio.c_str(), horaFin.c_str(),
    humedadSustrato,
    currentDay,
    datosGuardados ? "Sí" : "No",
    huboRiego ? "Sí" : "No",
    currentMonth, resumenMes.c_str(),
    heapTotal, heapUsada, heapLibre,
    erroresResumen.c_str());
  finalMessage = String(textMsg);
  mailActivSchedule.text.content = finalMessage.c_str();
  mailActivSchedule.text.charSet = "us-ascii";
  mailActivSchedule.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailActivSchedule.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  configureSmtpSession();
  setupMail(mailActivSchedule, "Horario de riego activo");
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailActivSchedule)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("✅ Correo de horario activo enviado con éxito");
    mailActiveScheduleSended = true;
  }
  ESP_MAIL_PRINTF("💾 Memoria libre tras envío: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
}
void mailNoActiveSchedule(const String& idSmartDrip, const String& idUser, const String& idSDHex, const String& date, const String& nowTime,
  int currentMonth, int currentDay, int dripTimeLimit, int dripHumidityLimit, const String& startTime, const String& endTime, int substrateHumidity,
  bool sensoresActivos, bool dripActived, const String& resumenMensual, size_t totalHeap, size_t usedHeap, size_t freeHeap, const String& showErrorSummary) {
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
    sensoresActivos ? "Sí" : "No",
    dripActived ? "Sí" : "No",
    currentMonth, resumenMensual.c_str(),
    totalHeap, usedHeap, freeHeap,
    showErrorSummary.c_str());
  finalMessage = String(textMsg);
  mailNoActivSchedule.text.content = finalMessage.c_str();
  mailNoActivSchedule.text.charSet = "us-ascii";
  mailNoActivSchedule.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailNoActivSchedule.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  configureSmtpSession();
  setupMail(mailNoActivSchedule, "SmartDrip: Fin de horario activo");
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailNoActivSchedule)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📧 Correo enviado con éxito");
    mailNoActiveScheduleSended = true;
  }
  ESP_MAIL_PRINTF("🧠 Liberar memoria: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
}
void mailSmartDripOn( const String& user, const String& device, const String& hexId, const String& fecha, const String& hora, int tiempo,
  int humedadLimite, int humedadSustrato) {
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
        device.c_str(), user.c_str(), hexId.c_str(),
        fecha.c_str(), hora.c_str(),
        tiempo, humedadLimite, humedadSustrato);
  finalMessage = String(textMsg);
  mailDripOn.text.content = finalMessage.c_str();
  mailDripOn.text.charSet = "us-ascii";
  mailDripOn.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailDripOn.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  configureSmtpSession();
  setupMail(mailDripOn, "Inicio Riego Smart Drip");
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailDripOn)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📩 Mail de inicio de riego enviado.");
    mailDripOnSended = true;
  }
  ESP_MAIL_PRINTF("🧠 Memoria tras envío: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
}
void mailSmartDripOff(const String& user, const String& device, const String& hexId, const String& fecha, const String& hora, int tiempoRiego,
  int limiteHumedad, int humedadSustrato, float humedadAmbiente, float temperatura) {
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
        "• Humedad ambiental: %.1f%%\n"
        "• Temperatura ambiente: %.1f°C\n",
        device.c_str(), user.c_str(), hexId.c_str(),
        fecha.c_str(), hora.c_str(),
        tiempoRiego,
        limiteHumedad,
        humedadSustrato,
        humedadAmbiente,
        temperatura);
  finalMessage = String(textMsg);
  mailDripOff.text.content = finalMessage.c_str();
  mailDripOff.text.charSet = "us-ascii";
  mailDripOff.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailDripOff.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  configureSmtpSession();
  setupMail(mailDripOff, "SmartDrip: Fin del Riego");
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailDripOff)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📩 Mail de fin de riego enviado.");
    mailDripOffSended = true;
  }
  ESP_MAIL_PRINTF("🧠 Memoria tras envío: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
}
void mailAnnualReport(const String& deviceHexId, const String& userId, const String& deviceId, int anio) {
  String fullReport = "";
  for (int m = 1; m <= 12; m++) {
    String monthName = getMonthName(m);
    String data = printMonthlyDataJson(m, anio, false);
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
           deviceHexId.c_str(),
           userId.c_str(),
           deviceId.c_str(),
           anio,
           fullReport.c_str());
  finalMessage = String(textMsg);
  mailAnualReport.text.content = finalMessage.c_str();
  mailAnualReport.text.charSet = "us-ascii";
  mailAnualReport.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailAnualReport.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  configureSmtpSession();
  setupMail(mailAnualReport, "📊 Informe Anual Smart Drip");
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
void mailErrorValve(const String& deviceHexId, const String& userId, const String& deviceId) {
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "❌ Error en la electroválvula de riego del Smart Drip%s\n"
           "Se detiene el proceso de riego automático. \n"
           "Los sensores indican que el agua continúa fluyendo. \n"
           "Por favor revise la instalación lo antes posible.",
           deviceHexId.c_str(), userId.c_str(), deviceId.c_str());
  finalMessage = String(textMsg);
  mailErrValve.text.content = finalMessage.c_str();
  mailErrValve.text.charSet = "us-ascii";
  mailErrValve.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrValve.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  configureSmtpSession();
  setupMail(mailErrValve, "⚠️ Error válvula Smart Drip");
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailErrValve)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📧 Mail de error válvula enviado con éxito");
    mailErrorValveSended = true;
  }
  smtp.closeSession();
}
void mailErrorDHT11(const String& idSDHex, const String& idUser, const String& idSmartDrip) {
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "❌ El sensor de datos ambientales del Smart Drip%s está desconectado o dañado.\n"
           "Proceda a su inspección o contacte con el servicio técnico.",
           idSDHex.c_str(), idUser.c_str(), idSmartDrip.c_str());
  finalMessage = String(textMsg);
  mailErrorDHT.text.content = finalMessage.c_str();
  mailErrorDHT.text.charSet = "us-ascii";
  mailErrorDHT.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrorDHT.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  configureSmtpSession();
  setupMail(mailErrorDHT, "⚠️ Error sensor ambiente (DHT)");
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailErrorDHT)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📧 Mail de error DHT11 enviado con éxito");
    mailErrorDHTSended = true;
  }
  ESP_MAIL_PRINTF("🧠 Memoria tras envío: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
}
void mailErrorSensorHigro(const String& idSDHex, const String& idUser, const String& idSmartDrip) {
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "❌ El sensor de humedad del sustrato del Smart Drip%s está fuera de rango o dañado.\n"
           "Se recomienda recalibración.\n"
           "Proceda a su inspección o contacte con el servicio técnico.",
           idSDHex.c_str(), idUser.c_str(), idSmartDrip.c_str());
  finalMessage = String(textMsg);
  mailErrorHigro.text.content = finalMessage.c_str();
  mailErrorHigro.text.charSet = "us-ascii";
  mailErrorHigro.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrorHigro.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  configureSmtpSession();
  setupMail(mailErrorHigro, "⚠️ Error sensor humedad sustrato");
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailErrorHigro)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📧 Mail de error higro enviado con éxito");
    mailErrorHigroSended = true;
  }
  ESP_MAIL_PRINTF("🧠 Memoria tras envío: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
}
void mailErrorFlowSensor(const String& idSDHex, const String& idUser, const String& idSmartDrip) {
  snprintf(textMsg, sizeof(textMsg),
           "%s \n%s \n"
           "❌ El sensor de flujo del sistema Smart Drip%s no está registrando caudal durante el riego.\n"
           "Esto puede indicar una obstrucción, fallo del sensor o un problema en la instalación.\n"
           "Por favor, revise el sistema de inmediato.",
           idSDHex.c_str(), idUser.c_str(), idSmartDrip.c_str());
  finalMessage = String(textMsg);
  mailErrFlowSensor.text.content = finalMessage.c_str();
  mailErrFlowSensor.text.charSet = "us-ascii";
  mailErrFlowSensor.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  mailErrFlowSensor.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  configureSmtpSession();
  setupMail(mailErrFlowSensor, "⚠️ Error sensor de flujo en Smart Drip");
  if (!smtp.connect(&session)) {
    updateErrorLog("smtp", smtp.errorReason(), getCurrentDateKey());
    return;
  }
  if (!MailClient.sendMail(&smtp, &mailErrFlowSensor)) {
    updateErrorLog("envio", smtp.errorReason(), getCurrentDateKey());
  } else {
    Serial.println("📧 Mail de error sensor de flujo enviado con éxito");
    mailErrorFlowSensorSended = true;
  }
  ESP_MAIL_PRINTF("🧠 Memoria tras envío: %d\n", MailClient.getFreeHeap());
  smtp.closeSession();
}

