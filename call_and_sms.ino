// SIM card PIN (leave empty, if not defined)
const char simPIN[] = "";
/*
Usefull AT commands:
AT+CMGF=1 - Set SMS text mode
AT+CMGL="ALL" - List all SMS messages
AT+CMGR=1 - Read SMS message at index 1
AT+CMGD=1 - Delete SMS message at index 1
AT+CLIP=1 - Enable missed call notification
AT+DDET=1 - Enable DTMF detection
AT+CLCC - List current calls
ATD+923354888420; - Make a call to +923354888420
AT+CHUP - Hang up current call
AT+CREG? - Check if registered to the network
AT+CSQ - Check signal quality
AT+CBC - Check battery voltage
AT+CMEE=2 - Enable verbose error messages
AT+CMEE=0 - Disable verbose error messages
AT+CMEE? - Check if verbose error messages are enabled
AT+CPIN? - Check if SIM card is inserted
*/
String MOBILE_No = "+923354888420";

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800   // Modem is SIM800
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

#include <TinyGsmClient.h>
#include <Wire.h>

// TTGO T-Call pins
#define MODEM_RST 5
#define MODEM_PWKEY 4
#define MODEM_POWER_ON 23
#define MODEM_TX 27
#define MODEM_RX 26
#define I2C_SDA 21
#define I2C_SCL 22

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

// Define the serial console for debug prints, if needed
// #define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#define IP5306_ADDR 0x75
#define IP5306_REG_SYS_CTL0 0x00

bool setPowerBoostKeepOn(int en) {
  Wire.beginTransmission(IP5306_ADDR);
  Wire.write(IP5306_REG_SYS_CTL0);
  if (en) {
    Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    Wire.write(0x35); // 0x37 is default reg value
  }
  return Wire.endTransmission() == 0;
}

String receivedMessage = ""; // Global variable to store received SMS message

void setup() {
  // Set console baud rate
  SerialMon.begin(115200);

  // Keep power when running from battery
  Wire.begin(I2C_SDA, I2C_SCL);
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // use modem.init() if you don't need the complete restart

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3) {
    modem.simUnlock(simPIN);
  }

  // String smsMessage = "System booted";
  // if (modem.sendSMS(MOBILE_No, smsMessage)) {
  //   SerialMon.println(smsMessage);
  // } else {
  //   SerialMon.println("SMS failed to send");
  // }

  // Enable missed call notification
  modem.sendAT(GF("+CLIP=1"));

  // Enable SMS text mode
  modem.sendAT(GF("+CMGF=1"));
  delay(500);
  // giveMissedCall();
}

void loop() {
  // read full string of SerialMon not char by char and send it to SerialAT
  if (SerialMon.available()) {
    String command = SerialMon.readString();
    if (command.indexOf("call") != -1) {
      SerialMon.println("Calling " + String(MOBILE_No));
      giveMissedCall();
    } else if (command.indexOf("sms") != -1) {
      // fetch sms from input string, sample-> sms : msg here
      String sms = command.substring(command.indexOf("sms") + 6);
      SerialMon.println("Sending SMS : " + sms + " to : " + String(MOBILE_No));
      sendSMS(sms);
    } else if (command.indexOf("check") != -1) {
      SerialMon.println("Checking received message");
      checkReceivedMessage();
    } else {
      SerialMon.println(
          "____________________________________________________\n" + command +
          "____________________________________________________");
      SerialAT.println(command);
    }
  }

  if (SerialAT.available()) {
    char c = SerialAT.read();
    SerialMon.write(c);
    handleIncomingSMS(c);
  }
}

void handleIncomingSMS(char c) {
  static enum { IDLE, RECEIVING, RECEIVED } state = IDLE;
  static String buffer;

  if (state == IDLE) {
    if (c == '+') {
      buffer = "";
      buffer += c;
      state = RECEIVING;
    }
  } else if (state == RECEIVING) {
    buffer += c;
    if (c == '\n') {
      state = RECEIVED;
      receivedMessage = buffer;
      SerialMon.println("Received SMS:");
      SerialMon.println(receivedMessage);
    }
  }
}

void giveMissedCall() {
  SerialAT.println("ATD+ " + MOBILE_No + ";");
  updateSerial();
  // delay(20000);            // wait for 20 seconds...
  // SerialAT.println("ATH"); // hang up
  // updateSerial();
}
void sendSMS(String sms) {
  if (modem.sendSMS(MOBILE_No, sms)) {
    SerialMon.println(sms);
  } else {
    SerialMon.println("SMS failed to send");
  }
}

void checkReceivedMessage() { receivedMessage = ""; }
void updateSerial() {
  delay(500);
  while (SerialMon.available()) {
    SerialAT.write(SerialMon.read());
  }
  while (SerialAT.available()) {
    SerialMon.write(SerialAT.read());
  }
}