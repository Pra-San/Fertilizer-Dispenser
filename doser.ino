/* ##################################################################################################################################################################################*/
// IMPORTS

#include <BluetoothSerial.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Preferences.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run make menuconfig to and enable it
#endif

/* ##################################################################################################################################################################################*/
Preferences preferences;
BluetoothSerial SerialBT;


short targetHour ;  // Target hour to turn on the motor
short targetMinute ;  // Target minute to turn on the motor

int duration;         // Duration in milliseconds
long interval;        // Repeating interval in seconds

double calibrationFactor;
/* ##################################################################################################################################################################################*/
// MOTOR CONTROLS

const int motorPin = 13;  // GPIO pin connected to the motor
const int pwmChannel = 0;
const int pwmFreq = 5000;     // 5 kHz PWM frequency
const int pwmResolution = 8;  // 8-bit resolution (0-255)

int dutyCycle = 100;  // Duty cycle in percent

unsigned long lastRunTime = 0;
bool isOff = true;


void runMotor() {
  int pwmValue = map(dutyCycle, 0, 100, 0, 255);
  ledcWrite(pwmChannel, pwmValue);
  delay(duration*calibrationFactor);
  ledcWrite(pwmChannel, 0);
}

void startMotor() {
  int pwmValue = map(dutyCycle, 0, 100, 0, 255);
  ledcWrite(pwmChannel, pwmValue);
  isOff = false;
}

void stopMotor() {
  ledcWrite(pwmChannel, 0);
  isOff = true;
}

//##################################################################################################################################################################################/

/* ##################################################################################################################################################################################*/
// BLUETOOTH CONTROLS

void processCommand(String command) {
  int separatorIndex = command.indexOf(':');
  if (separatorIndex != -1) {
    String key = command.substring(0, separatorIndex);
    Serial.println(key);

    //Settings
    if (key == "duration") {
      int value = command.substring(separatorIndex + 1).toInt();
      duration = value;
      preferences.putInt("duration", value);
      Serial.println("Updated " + key + " to " + String(value));

    } else if (key == "interval") {
      long value = atol(command.substring(separatorIndex + 1).c_str());
      interval = value;
      preferences.putLong("interval", value);

          Serial.println("Updated " + key + " to " + String(value));

    } else if (key == "clearsettings")
      preferences.clear();
    // Motor Functions
    else if (key == "start")
      startMotor();
    else if (key == "stop")
      stopMotor();
    else if (key == "calibrate"){
      startMotor();
      delay(5000);
      stopMotor();
    }
    else if( key == "setfactor"){
      double value = command.substring(separatorIndex + 1).toDouble();
      preferences.putDouble("factor",5/value);
      calibrationFactor = 5/value;
      Serial.println("Updated " + key + " to " + String(value));
    }
    else if(key == "dispense") {
      double value = command.substring(separatorIndex + 1).toDouble();
      Serial.println("start dispense " + key + " to " + String(value));
      startMotor();
      delay(value*1000*calibrationFactor);
      stopMotor();
      Serial.println("stop dispense " + key + " to " + String(value));
    }

    // Timer Functions
    else if (key == "showtime")
      int temp=getTime("");
    else if (key == "sethour") {
      int intValue = command.substring(separatorIndex + 1).toInt();
      short value = static_cast<short>(intValue);
      targetHour = value;
      preferences.putInt("targetHour", value);
      Serial.println("Updated " + key + " to " + String(value));
      
    } else if (key == "setminute") {
      int intValue = command.substring(separatorIndex + 1).toInt();
      short value = static_cast<short>(intValue);
      targetMinute = value;
      preferences.putInt("targetMinute", value);
      Serial.println("Updated " + key + " to " + String(value));
    }
  }
}

/* ##################################################################################################################################################################################*/

/* ##################################################################################################################################################################################*/
// TIMER CONTROLS

const int IO = 12;   // DAT
const int SCLK = 4;  // CLK
const int CE = 2;    // RST

ThreeWire myWire(IO, SCLK, CE);
RtcDS1302<ThreeWire> Rtc(myWire);

#define countof(a) (sizeof(a) / sizeof(a[0]))

void currentTimeValid() {
  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);
  Serial.println();
  if (!now.IsValid()) {
    // Common Causes:
    //    1) the battery on the device is low or even missing and the power line was disconnected
    SerialBT.println("Lost confidence in the DateTime!");
  }
}

int getTime(char* msg) {
RtcDateTime now = Rtc.GetDateTime();
printDateTime(now);
if(msg == "min") return now.Minute();
else if (msg == "hour") return now.Hour();
else return now.Day();
}

void printDateTime(const RtcDateTime& dt) {
  char datestring[20];
  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Month(),
             dt.Day(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  // Serial.println(datestring);
  SerialBT.print(datestring);

}

void clockSetup(){
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid()) {
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected()) {
    Serial.println("RTC was write protected, enabling writing now");
    Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("RTC is newer than compile time. (this is expected)");
  } else if (now == compiled) {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
}
//##################################################################################################################################################################################/

/* ##################################################################################################################################################################################*/
// SETUP AND LOOP


void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Motor_Control");  // Bluetooth device name

  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(motorPin, pwmChannel);

  Serial.println("The device started, now you can pair it with Bluetooth!");

  preferences.begin("motor-settings", false);
  clockSetup();
  duration = preferences.getInt("duration", 5000);
  interval = preferences.getLong("interval", 86400);

  targetHour = preferences.getShort("TargetHour",-1);
  targetMinute = preferences.getShort("TargetHour",-1);

  calibrationFactor=preferences.getDouble("factor",1);
}

void loop() {
  if (SerialBT.available()) {
    String command = SerialBT.readStringUntil('\n');
    processCommand(command);
  }

  unsigned long currentTime = millis();
  int temp = getTime("hour");
  if (targetHour < temp && targetHour !=-1) {
    // runMotor();
    Serial.println("Works");
    // delay(5000);
  }
}

/* ##################################################################################################################################################################################*/