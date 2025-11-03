#ifndef _GSM_H
#define _GSM_H

#include <Arduino.h>

typedef void (*AsyncCallback)(const String&);

struct AsyncHook {
    String pattern;
    AsyncCallback callback;
};

class GSM {
public:
    GSM(HardwareSerial& modem, HardwareSerial& usb);
    
    void begin(unsigned long usbBaud, unsigned long modemBaud);

    void debugPrint(const String &str) ;
    void debugPrint(const int &val) ;

    void update();

    void addCommand(const String& cmd);
    void addAsyncCallback(const String& pattern, AsyncCallback cb);
    void sendSMS(const String& phone, const String& msg) ;


private:
    void processCommandQueue();
    void checkAsyncHooks(const String& line);

    static const int MAX_HOOKS = 10;
    AsyncHook asyncHooks[MAX_HOOKS];
    int hookCount = 0;

    HardwareSerial& modemSerial;
    HardwareSerial& usbSerial;
    static const int MAX_COMMANDS = 20;
    String commands[MAX_COMMANDS];
    int commandCount = 0 ;
    bool awaitingResponse = false;

    bool collectingMultiLine = false;
    String multiLineBuffer;
    String multiLinePattern;
    unsigned long multiLineStartTime = 0;


} ;


#endif
