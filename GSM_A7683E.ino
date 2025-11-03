#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "secret.h"

#ifndef _SECRET_H
  #error "secret.h is not defined! Define it before compiling. Include WIFI_SSID, WIFI_PASSWORD, MQTT_BROKER, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD"
#else
  #pragma message("secret.h is defined! - Make sure this is ignored in your .gitignore file!!!!")
#endif

// Your GSM handling code and includes
#include "GSM.h"


WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char* mqttTopicIncoming = "gsm/incoming_sms";
const char* mqttTopicOutgoing = "gsm/send_sms";

unsigned long lastMQTTReconnectAttempt = 0;

// Forward declarations
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool mqttConnect();
void publishIncomingSMS(const String& phone, const String& message);


//GSMTerminal gsm(Serial1, Serial);

GSM *gsm = nullptr ; //(Serial1, Serial) ;


void debugPrint(const int &l) {
  Serial.print(l) ;
  Serial.flush();
}

void debugPrint(const String &l) {
  Serial.print(l) ;
  Serial.flush();
}

void debugPrintln(const String &l) {
  Serial.println(l) ;
  Serial.flush();
}
void debugPrintln(const int &l) {
  Serial.println(l) ;
  Serial.flush();
}

void software_reset() {
  debugPrintln("Rebooting....") ;
  delay(5000);
  watchdog_reboot(0, 0, 0);  // Triggers a full chip reset
}


// Extract phone number, timestamp, and SMS body from a full +CMGR multi-line response.
// Returns true if parsing was successful.
bool extractCMGRData(const String &response, String &phoneOut, String &timeOut, String &textOut) {
  phoneOut = "";
  timeOut = "";
  textOut = "";

  // 1. Locate the +CMGR header line

  //int headerStart = response.indexOf("+CMGR:");
  //if (headerStart < 0) {
  //  return false;
  //}

  int idxCMGR = response.indexOf("+CMGR:");
  int idxCMGRD = response.indexOf("+CMGRD:");

  int headerStart = -1;
  if (idxCMGR != -1 && (idxCMGRD == -1 || idxCMGR < idxCMGRD)) {
      headerStart = idxCMGR;
  } else if (idxCMGRD != -1) {
      headerStart = idxCMGRD;
  } else {
      // Neither found
      return false;
  }

  // Get end of header line
  int headerEnd = response.indexOf('\n', headerStart);
  String headerLine;
  if (headerEnd >= 0) {
    headerLine = response.substring(headerStart, headerEnd);
  } else {
    headerLine = response.substring(headerStart);
  }
  headerLine.trim();

  // 2. Parse quoted fields from the header
  const int n = headerLine.length();
  int quoteIdx[16];
  int qcount = 0;
  for (int i = 0; i < n && qcount < (int)(sizeof(quoteIdx) / sizeof(quoteIdx[0])); ++i) {
    if (headerLine[i] == '"') {
      quoteIdx[qcount++] = i;
    }
  }

  // Extract phone (2nd quoted field)
  if (qcount >= 4) {
    int pStart = quoteIdx[2] + 1;
    int pEnd = quoteIdx[3];
    phoneOut = headerLine.substring(pStart, pEnd);
  }

  // Extract timestamp (last quoted field)
  if (qcount >= 2) {
    int tStart = quoteIdx[qcount - 2] + 1;
    int tEnd = quoteIdx[qcount - 1];
    timeOut = headerLine.substring(tStart, tEnd);
  }

  phoneOut.trim();
  timeOut.trim();

  // 3. Extract SMS text (everything after header until "OK")
  int bodyStart = headerEnd + 1;  // start right after header line
  int okPos = response.lastIndexOf("OK");
  if (okPos > bodyStart) {
    textOut = response.substring(bodyStart, okPos);
    textOut.trim();
  } else {
    // If no "OK" found, assume rest is text
    textOut = response.substring(bodyStart);
    textOut.trim();
  }

  return phoneOut.length() > 0;
}



int extractVerificationCode(const String& text) {
  const String pattern = "Your verification code is ";
  int start = text.indexOf(pattern);

  if (start == -1) {
    // Pattern not found
    return -1;
  }

  // Move start to the position after the pattern
  start += pattern.length();

  // Extract the substring starting at 'start' until non-digit
  int end = start;
  while (end < text.length() && isDigit(text[end])) {
    end++;
  }

  if (end == start) {
    // No digits found
    return -1;
  }

  String codeStr = text.substring(start, end);
  return codeStr.toInt();
}


void readSMSHandler(const String& line) {
  debugPrintln(">> READ SMS detected: ");
  String phone, timestamp, message;
  bool ok = extractCMGRData(line, phone, timestamp, message);

  debugPrintln(ok ? "Parsed successfully" : "Failed to parse");
  debugPrint("Phone: '"); debugPrint(phone); debugPrintln("'");
  debugPrint("Time: '"); debugPrint(timestamp); debugPrintln("'");
  debugPrint("Message: '"); debugPrint(message); debugPrintln("'");

  if (ok) {
    String jsonMsg = "{";
    jsonMsg += "\"phone\": \"" + phone + "\",";
    jsonMsg += "\"timestamp\": \"" + timestamp + "\",";
    jsonMsg += "\"message\": \"" + message + "\"";
    jsonMsg += "}";

    mqttClient.publish(mqttTopicIncoming, jsonMsg.c_str());
  }


  if (line.indexOf("verification code") != -1) {
    int code = extractVerificationCode(line);
    if (code != -1) {
      debugPrint("Code: "); debugPrintln(String(code));
    } else {
      debugPrintln("Pattern found, but no digits.");
       
    }
  } else {
    debugPrintln("No verification code in message.");
  }

}

void smsHandler(const String& atResponse) {
  debugPrint(">> Incoming SMS detected: ");
  debugPrintln(atResponse);

  // Example +CMTI: "SM",3
  int commaPos = atResponse.indexOf(',');
  int quotePos = atResponse.indexOf('"');

  if (commaPos > 0) {
    // Extract index (number after comma)
    String indexStr = atResponse.substring(commaPos + 1);
    indexStr.trim();  // remove whitespace/newlines
    int msgIndex = indexStr.toInt();

    // Form AT command to read this message (and delete!)
    String rdCmd = "AT+CMGRD=" + String(msgIndex);

    debugPrint("Generated AT command: ");
    debugPrintln(rdCmd);

    // You can now queue this AT command to GSMTerminal
    gsm->addCommand(rdCmd);  
    
    //String rmCmd = "AT+CMGD=" + String(msgIndex);
    //gsm->addCommand(rmCmd);  
    
  }

}

void ringHandler(const String& atResponse) {
  debugPrint(">> RING detected: ");
  debugPrintln(atResponse);
}

void clccHandlerDeprecated(const String& atResponse) {
  debugPrint(">> CLCC detected: ");
  debugPrintln(atResponse);
  String phoneNumber = "";

  int firstQuoteIndex = atResponse.indexOf('"');
  if (firstQuoteIndex != -1) {
    // Find the second quote
    int secondQuoteIndex = atResponse.indexOf('"', firstQuoteIndex + 1);
    if (secondQuoteIndex != -1) {
      // Extract the substring between the quotes
      phoneNumber = atResponse.substring(firstQuoteIndex + 1, secondQuoteIndex);
    }
    debugPrintln("Extracted Number: " + phoneNumber);
  }

 
}

void clccHandler(const String& atResponse) {
  debugPrint(">> CLCC detected: ");
  debugPrintln(atResponse);

  String phoneNumber = "";
  int callStatus = -1; // default to an invalid value

  // Extract phone number between quotes
  int firstQuoteIndex = atResponse.indexOf('"');
  if (firstQuoteIndex != -1) {
    int secondQuoteIndex = atResponse.indexOf('"', firstQuoteIndex + 1);
    if (secondQuoteIndex != -1) {
      phoneNumber = atResponse.substring(firstQuoteIndex + 1, secondQuoteIndex);
    }
  }

  // Extract the third number (call status)
  // Split the response by commas
  int comma1 = atResponse.indexOf(',');
  int comma2 = atResponse.indexOf(',', comma1 + 1);
  int comma3 = atResponse.indexOf(',', comma2 + 1);

  if (comma2 != -1 && comma3 != -1) {
    String statusStr = atResponse.substring(comma2 + 1, comma3);
    statusStr.trim();
    callStatus = statusStr.toInt();
  }

  debugPrintln("Extracted Number: " + phoneNumber);
  debugPrint("Call Status: ");
  debugPrintln(String(callStatus));

  if (callStatus == 6) {
    //software_reset() ;
    //gsm->sendSMS("+447753432247","This is the winter!! ") ;

  }

}
#define PWR_PIN 15 

void power_on() {
  // Power ON for 3 seconds
  digitalWrite(PWR_PIN, LOW) ;
  delay(1000);
  digitalWrite(PWR_PIN, HIGH) ;

}
void power_off() {
  // Now Power Off
  digitalWrite(PWR_PIN, LOW) ;
  delay(2500);
  digitalWrite(PWR_PIN, HIGH) ;

}
void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PWR_PIN, OUTPUT);

  power_on() ;
  delay(2000);
  power_off() ;
  delay(3000) ;
  power_on() ;
  delay(6000) ;


  gsm = new GSM(Serial1, Serial);
  gsm->begin(115200, 115200);
  gsm->addCommand("AT"); 
  gsm->addCommand("ATE0"); 
  gsm->addCommand("AT+CMGF=1"); 
  gsm->addCommand("AT+CMGD=1,4"); 
  
  
  gsm->addAsyncCallback("+CMTI:", smsHandler);
  gsm->addAsyncCallback("+CMGR:", readSMSHandler);
  gsm->addAsyncCallback("+CMGRD:", readSMSHandler);
  gsm->addAsyncCallback("RING", ringHandler);
  gsm->addAsyncCallback("+CLCC:", clccHandler);
 
  // 1. Scan
  debugPrintln("Scanning Networks") ;
  int n = WiFi.scanNetworks();
  debugPrint("Found ") ;
  debugPrint(n) ;
  debugPrintln(" Networks!") ;

  for (int i = 0; i < n; i++) {
    debugPrintln(WiFi.SSID(i)) ;
  }


  // 2. Connect to WiFi
  debugPrintln(WIFI_SSID);
  debugPrintln(WIFI_PASSWORD);
  debugPrintln(WiFi.macAddress());

  debugPrintln("Connecting to WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(WiFi.status());
    delay(500);
    debugPrint(".");
  }
  debugPrintln(" connected!");

  // 3. Initialize MQTT
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  mqttConnect();

}

bool led_state = false ;

void loop() {
  digitalWrite(LED_BUILTIN, led_state) ;
  led_state = !led_state ;
  //delay(1000);

  if (gsm != NULL) {
        gsm->update() ;
  }

    // Handle MQTT
  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMQTTReconnectAttempt > 5000) {
      lastMQTTReconnectAttempt = now;
      if (mqttConnect()) {
        lastMQTTReconnectAttempt = 0;
      }
    }
  } else {
    mqttClient.loop();
  }

}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // Null-terminate payload
  String message = String((char*)payload);

  debugPrintln("MQTT message received: " + message);

  // Parse as `phone|message` format
  int sepIdx = message.indexOf('|');
  if (sepIdx > 0) {
    String phone = message.substring(0, sepIdx);
    String text = message.substring(sepIdx + 1);

    gsm->sendSMS(phone, text);
  }
}

bool mqttConnect() {
  debugPrint("Attempting MQTT connection...");
  if (mqttClient.connect("GSMClient")) {
    debugPrintln(" connected!");
    // Subscribe to outgoing SMS topic
    mqttClient.subscribe(mqttTopicOutgoing);
  } else {

    debugPrint("failed, rc=");
    debugPrintln(mqttClient.state());

  }
  return mqttClient.connected();
}

