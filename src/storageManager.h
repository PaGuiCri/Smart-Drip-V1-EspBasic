#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H
#include <Arduino.h>
// Guarda o actualiza datos del día (sensores y estado del riego)
void storeOrUpdateDailyDataJson(int day, int month, int year,
    int newSubstrate, int newHumidity, int newTemp,
    bool dripActive, bool forceOverwrite = false,
    String customDateKey = "");
// Verifica si ya existen datos guardados para una fecha específica
bool isDataStoredForDate(const String& dateKey);
// Devuelve la fecha actual en formato YYYY-MM-DD
String getCurrentDateKey();
// Comprueba si existe data.json. Si no, lo crea vacío
void checkStorageFile();
// Actualiza los errores de correo (SMTP y envío)
void updateErrorLog(String smtpError, String mailError);
// Imprime por Serial todos los datos diarios (debug)
void printDailyData();
// Devuelve los datos de un mes como string. Si debug = true, los imprime por Serial también
String printMonthlyDataJson(int month, int year, bool debug = false);
#endif
