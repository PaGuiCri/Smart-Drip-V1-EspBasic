#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
extern String ssid;
extern String pass;
extern String idUser;
extern String idSmartDrip;
extern String idSDHex;
extern unsigned long idNumber;
extern int smtpPort;
extern String smtpServer, smtpEmail, smtpPass;
void loadConfigFromJson();  // Carga los datos del archivo config.json
void saveConfigToJson();    // Guarda los datos en el archivo config.json
String xorEncryptDecrypt(const String &input, const String &key);
String stringToHex(const String &input);
String hexToString(const String &hexString);
String decodeAndDecrypt(const String &hex, const String &key);
String encryptAndEncode(const String &plainText, const String &key);
#endif
