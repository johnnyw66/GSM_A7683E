#include "GSM.h"

GSM::GSM(HardwareSerial& modem, HardwareSerial& usb)
: modemSerial(modem), usbSerial(usb) {

}

void GSM::processCommandQueue() {
    //debugPrint("processCommandQueue");
    //debugPrint(commandCount);

    if (awaitingResponse || commandCount == 0) return;

    //debugPrint(commands[0]);
    modemSerial.println(commands[0]);

    // Shift the queue
    for (int i = 1; i < commandCount; i++) {
        commands[i - 1] = commands[i];
    }
    commandCount--;
    awaitingResponse = false;
}

void GSM::debugPrint(const String &str) {
  Serial.println(str) ;
  Serial.flush() ;  
}

void GSM::debugPrint(const int &val) {
  Serial.println(val) ;
  Serial.flush() ;  
}

void GSM::begin(unsigned long usbBaud, unsigned long modemBaud) {
  debugPrint("GSM:begin") ;
  usbSerial.begin(usbBaud) ;
  modemSerial.begin(modemBaud) ;

}


void GSM::addAsyncCallback(const String &pattern , AsyncCallback cb) {
  debugPrint("addAsyncCallBack");
  debugPrint(pattern);
  if (hookCount < MAX_HOOKS) {
        asyncHooks[hookCount++] = {pattern, cb};
    }

}

void GSM::checkAsyncHooks(const String& line) {
  //debugPrint("checkAsyncs");
  //debugPrint(line) ;

  for (int i = 0; i < hookCount; i++) {
      if (line.indexOf(asyncHooks[i].pattern) >= 0) {
          asyncHooks[i].callback(line);
      }
  }
}


void GSM::update() {

  // Forward USB input to modem
  while (usbSerial.available()) {
      modemSerial.write(usbSerial.read());
  }

    // Read modem output
  static String modemBuffer;
  static unsigned long lastReadTime = 0;

  while (modemSerial.available()) {
    char c = modemSerial.read();
    modemBuffer += c;
      lastReadTime = millis();
  }

  // Process each complete line immediately or in the case of reading 
  // SMS after reading OK
  int newlineIndex;
  while ((newlineIndex = modemBuffer.indexOf('\n')) >= 0) {
      String line = modemBuffer.substring(0, newlineIndex);
      line.trim();  // remove CR/LF and spaces
      modemBuffer = modemBuffer.substring(newlineIndex + 1);
  
      if (line.length() == 0) continue;

      // Detect start of multi-line message (+CMGR: or +CMGRD:)
      if (!collectingMultiLine && (line.startsWith("+CMGR:") || line.startsWith("+CMGRD:"))) {
            collectingMultiLine = true;
            multiLineBuffer = line;
            multiLineStartTime = millis();
            continue;
      }

      // echo to Debug terminal
      debugPrint(line);

      if (!collectingMultiLine) {
        // Single-line async messages (e.g., +CMTI:)
        checkAsyncHooks(line);
      } else {
          multiLineBuffer += "\n" + line;
          if (line == "OK") {
            checkAsyncHooks(multiLineBuffer);
            // Flush multiLine buffer
            multiLineBuffer = "" ;
            collectingMultiLine = false;
           
          }  
      }

  }

  processCommandQueue();

}
void GSM::sendSMS(const String& phoneNumber, const String& msg) {
  static String modemBuffer;

  String cmd = "AT+CMGS=\"" + phoneNumber + "\"";

  modemSerial.println(cmd);

  delay(1500);
  while (modemSerial.available()) {
    char c = modemSerial.read();
    modemBuffer += c;
  }
  Serial.println(modemBuffer) ; Serial.flush() ;
  modemSerial.println(msg) ;
  modemSerial.write(26) ;
  delay(1500) ;

}

void GSM::addCommand(const String& cmd) {
  debugPrint("GSM:addCommand") ;
  if (commandCount < MAX_COMMANDS) {
        commands[commandCount++] = cmd ;
  }
}



