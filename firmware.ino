/*
  ESP8266 firmware for x-mass smart relay module

  (c) 2018 dUkk (dukk@softdev.online)
*/
#include <os_type.h>
#include <OneWire.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <RTClib.h>
extern "C" {
#include <user_interface.h>
}

#define ADC_FILTER1_SAMPLES 200
//#define DEBUG 1

#define SSID_NAME "SSID"
#define SSID_PASSWORD "mypassword"
#define HTTP_HOST "somehost.local"
#define OUR_HOSTNAME "smartrl"

int32_t SecondsElapsed = 0, PrevMin = 60;
int32_t RelayCurPeriodLimit[3] = { 0, 0, 0}, RelayCurPeriod[3] = { 0, 0, 0};
bool RelaysCurrentState[3] = {false, false, false};
DateTime Timestamp;
bool first_time = true;

struct ST_SETTINGS
{
  int32_t RelaysDesiredState[3];
  int32_t RelayLowTime[3];
  int32_t RelayHighTime[3];
  int32_t RelayMinPeriod[3];
  int32_t RelayMaxPeriod[3];
  int32_t CommInterval;
  float txpower;
  char ssid[16];
  char password[32];
};

ST_SETTINGS settings;
RTC_DS1307 RTC;

String extractParam(String& authReq, const String& param, const char delimit);
void saveSettings();
void loadSettings();
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max);
bool is_between(const int32_t value, const int32_t rangestart, const int32_t rangeend);
void Radio_OFF();

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
#endif
  EEPROM.begin(128);
  delay(10);
#ifdef DEBUG
  Serial.println("starting");
#endif

  pinMode(13, OUTPUT); //GPIO13 = K1
  pinMode(12, OUTPUT); //GPIO12 = K2
  pinMode(14, OUTPUT); //GPIO14 = K3
  digitalWrite(13, LOW); //Initial state
  digitalWrite(12, LOW); //Initial state
  digitalWrite(14, LOW); //Initial state

  loadSettings();

  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  WiFi.setSleepMode(WIFI_MODEM_SLEEP);
  WiFi.setOutputPower(settings.txpower);
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);

  //RTC Clock
  Wire.begin(4, 5);
  if (!RTC.begin())
  {
#ifdef DEBUG
    Serial.println("Couldn't find RTC!");
#endif
  }
  if (!RTC.isrunning())
  {
#ifdef DEBUG
    Serial.println("RTC is not initialized! setting some fake timestamp");
#endif
    Timestamp = DateTime(1514800527);
    RTC.adjust(Timestamp);
    RTC.writeSqwPinMode(OFF);
  }
  else {
#ifdef DEBUG
    Serial.println("RTC initialized getting timestamp");
#endif
    Timestamp = RTC.now();
  }

  SecondsElapsed = settings.CommInterval;
}

void loop()
{
  Timestamp = RTC.now();
  int32_t CurTs = Timestamp.minute();

  //re-check rules needed? (only do it once at minute)
  if (CurTs != PrevMin)
  {
    PrevMin = CurTs;
    CurTs = (Timestamp.hour() * 60 * 60) + (CurTs * 60);

#ifdef DEBUG
    Serial.printf("CurTs= %d\r\n", CurTs);
#endif

    //OFF
    if (settings.RelaysDesiredState[0] == 0)
    {
      RelaysCurrentState[0] = false;
#ifdef DEBUG
      Serial.printf("R0 FORCED OFF\r\n");
#endif
    }
    //ON
    else if (settings.RelaysDesiredState[0] == 1)
    {
      RelaysCurrentState[0] = true;
#ifdef DEBUG
      Serial.printf("R0 FORCED ON\r\n");
#endif
    }
    //AUTO by TIME
    else if (settings.RelaysDesiredState[0] == 2)
    {
      if (is_between(CurTs, settings.RelayLowTime[0], settings.RelayHighTime[0]))
      {
        RelaysCurrentState[0] = false;
#ifdef DEBUG
        Serial.printf("R0 TIME OFF\r\n");
#endif
      }
      else
      {
        RelaysCurrentState[0] = true;
#ifdef DEBUG
        Serial.printf("R0 TIME ON\r\n");
#endif
      }
    }
    //AUTO by TIME with period
    else if (settings.RelaysDesiredState[0] == 3)
    {
      //
      if (is_between(CurTs, settings.RelayLowTime[0], settings.RelayHighTime[0]))
      {
        RelaysCurrentState[0] = false;
#ifdef DEBUG
        Serial.printf("R0 TIME OFF\r\n");
#endif
      }
      else
      {
        //check if current period ticks is reached?
        if (RelayCurPeriod[0]++ >= RelayCurPeriodLimit[0])
        {
#ifdef DEBUG
          Serial.printf("R0 cur period tick reached cur limit, switching\r\n");
#endif
          //reached -> switch
          if (RelaysCurrentState[0])
          {
            RelaysCurrentState[0] = false;
          }
          else
          {
            RelaysCurrentState[0] = true;
          }
          RelayCurPeriod[0] = 0;
          RelayCurPeriodLimit[0] = settings.RelayMaxPeriod[0];
        }
      }
    }
    //AUTO by TIME with random period
    else if (settings.RelaysDesiredState[0] == 4)
    {
      //
      if (is_between(CurTs, settings.RelayLowTime[0], settings.RelayHighTime[0]))
      {
        RelaysCurrentState[0] = false;
#ifdef DEBUG
        Serial.printf("R0 TIME OFF\r\n");
#endif
      }
      else
      {
        //check if current period ticks is reached?
        if (RelayCurPeriod[0]++ >= RelayCurPeriodLimit[0])
        {
#ifdef DEBUG
          Serial.printf("R0 cur period tick reached cur limit, switching\r\n");
#endif
          //reached -> switch
          if (RelaysCurrentState[0])
          {
            RelaysCurrentState[0] = false;
          }
          else
          {
            RelaysCurrentState[0] = true;
          }
          RelayCurPeriod[0] = 0;
          RelayCurPeriodLimit[0] = secureRandom(settings.RelayMinPeriod[0], settings.RelayMaxPeriod[0]);
#ifdef DEBUG
          Serial.printf("R0 next period max is %d\r\n", RelayCurPeriodLimit[0]);
#endif
        }
      }
    }
    digitalWrite(13, RelaysCurrentState[0] ? HIGH : LOW);


    delay(200);
    //OFF
    if (settings.RelaysDesiredState[1] == 0)
    {
      RelaysCurrentState[1] = false;
      Serial.printf("R1 FORCED OFF\r\n");
    }
    //ON
    else if (settings.RelaysDesiredState[1] == 1)
    {
      RelaysCurrentState[1] = true;
      Serial.printf("R1 FORCED ON\r\n");
    }
    //AUTO by TIME
    else if (settings.RelaysDesiredState[1] == 2)
    {
      //
      if (is_between(CurTs, settings.RelayLowTime[1], settings.RelayHighTime[1]))
      {
        RelaysCurrentState[1] = false;
        Serial.printf("R1 TIME OFF\r\n");
      }
      else
      {
        RelaysCurrentState[1] = true;
        Serial.printf("R1 TIME ON\r\n");
      }
    }
    //AUTO by TIME with period
    else if (settings.RelaysDesiredState[1] == 3)
    {
      //
      if (is_between(CurTs, settings.RelayLowTime[1], settings.RelayHighTime[1]))
      {
        RelaysCurrentState[1] = false;
#ifdef DEBUG
        Serial.printf("R1 TIME OFF\r\n");
#endif
      }
      else
      {
        //check if current period ticks is reached?
        if (RelayCurPeriod[1]++ >= RelayCurPeriodLimit[1])
        {
#ifdef DEBUG
          Serial.printf("R1 cur period tick reached cur limit, switching\r\n");
#endif
          //reached -> switch
          if (RelaysCurrentState[1])
          {
            RelaysCurrentState[1] = false;
          }
          else
          {
            RelaysCurrentState[1] = true;
          }
          RelayCurPeriod[1] = 0;
          RelayCurPeriodLimit[1] = settings.RelayMaxPeriod[1];
        }
      }
    }
    //AUTO by TIME with random period
    else if (settings.RelaysDesiredState[1] == 4)
    {
      //
      if (is_between(CurTs, settings.RelayLowTime[1], settings.RelayHighTime[1]))
      {
        RelaysCurrentState[1] = false;
#ifdef DEBUG
        Serial.printf("R1 TIME OFF\r\n");
#endif
      }
      else
      {
        //check if current period ticks is reached?
        if (RelayCurPeriod[1]++ >= RelayCurPeriodLimit[1])
        {
#ifdef DEBUG
          Serial.printf("R1 cur period tick reached cur limit, switching\r\n");
#endif
          //reached -> switch
          if (RelaysCurrentState[1])
          {
            RelaysCurrentState[1] = false;
          }
          else
          {
            RelaysCurrentState[1] = true;
          }
          RelayCurPeriod[1] = 0;
          RelayCurPeriodLimit[1] = secureRandom(settings.RelayMinPeriod[1], settings.RelayMaxPeriod[1]);
#ifdef DEBUG
          Serial.printf("R1 next period max is %d\r\n", RelayCurPeriodLimit[1]);
#endif
        }
      }
    }
    digitalWrite(12, RelaysCurrentState[1] ? HIGH : LOW);

    delay(200);
    //OFF
    if (settings.RelaysDesiredState[2] == 0)
    {
      RelaysCurrentState[2] = false;
      Serial.printf("R2 FORCED OFF\r\n");
    }
    //ON
    else if (settings.RelaysDesiredState[2] == 1)
    {
      RelaysCurrentState[2] = true;
      Serial.printf("R2 FORCED ON\r\n");
    }
    //AUTO by TIME
    else if (settings.RelaysDesiredState[2] == 2)
    {
      //
      if (is_between(CurTs, settings.RelayLowTime[2], settings.RelayHighTime[2]))
      {
        RelaysCurrentState[2] = false;
        Serial.printf("R2 TIME OFF\r\n");
      }
      else
      {
        RelaysCurrentState[2] = true;
        Serial.printf("R2 TIME ON\r\n");
      }
    }
    //AUTO by TIME with period
    else if (settings.RelaysDesiredState[2] == 3)
    {
      //
      if (is_between(CurTs, settings.RelayLowTime[2], settings.RelayHighTime[2]))
      {
        RelaysCurrentState[2] = false;
#ifdef DEBUG
        Serial.printf("R2 TIME OFF\r\n");
#endif
      }
      else
      {
        //check if current period ticks is reached?
        if (RelayCurPeriod[2]++ >= RelayCurPeriodLimit[2])
        {
#ifdef DEBUG
          Serial.printf("R2 cur period tick reached cur limit, switching\r\n");
#endif
          //reached -> switch
          if (RelaysCurrentState[2])
          {
            RelaysCurrentState[2] = false;
          }
          else
          {
            RelaysCurrentState[2] = true;
          }
          RelayCurPeriod[2] = 0;
          RelayCurPeriodLimit[2] = settings.RelayMaxPeriod[2];
        }
      }
    }
    //AUTO by TIME with random period
    else if (settings.RelaysDesiredState[2] == 4)
    {
      //
      if (is_between(CurTs, settings.RelayLowTime[2], settings.RelayHighTime[2]))
      {
        RelaysCurrentState[2] = false;
#ifdef DEBUG
        Serial.printf("R2 TIME OFF\r\n");
#endif
      }
      else
      {
        //check if current period ticks is reached?
        if (RelayCurPeriod[2]++ >= RelayCurPeriodLimit[2])
        {
#ifdef DEBUG
          Serial.printf("R2 cur period tick reached cur limit, switching\r\n");
#endif
          //reached -> switch
          if (RelaysCurrentState[2])
          {
            RelaysCurrentState[2] = false;
          }
          else
          {
            RelaysCurrentState[2] = true;
          }
          RelayCurPeriod[2] = 0;
          RelayCurPeriodLimit[2] = secureRandom(settings.RelayMinPeriod[2], settings.RelayMaxPeriod[2]);
#ifdef DEBUG
          Serial.printf("R2 next period max is %d\r\n", RelayCurPeriodLimit[2]);
#endif
        }
      }
    }
    digitalWrite(14, RelaysCurrentState[2] ? HIGH : LOW);

  }

  //check is we need to connect with server?
  if (SecondsElapsed++ != settings.CommInterval)
  {
    delay(1000);
    return;
  }
  SecondsElapsed = 0;

  WiFi.forceSleepWake();
  delay(1);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(F(OUR_HOSTNAME));
  WiFi.begin(settings.ssid, settings.password);
  //abort loop if can't connect for 20sec
  for (int a = 0; a < 80; a++)
  {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(250);
#ifdef DEBUG
    Serial.print(".");
#endif
  }

  if (WiFi.status() != WL_CONNECTED) 
  {
#ifdef DEBUG
  Serial.println("WIFI CANT CONNECT!!!");
#endif
    Radio_OFF();
    return;
  }

#ifdef DEBUG
  Serial.println("WIFI connected, IP: ");
  Serial.println(WiFi.localIP());
  Serial.printf("BSSID: %s\n", WiFi.BSSIDstr().c_str());
#endif

  HTTPClient http;
  String payload;
  int32_t an = AnalogRead();
  yield();

  payload.concat(F("/senshandle.php?hid="));
  payload.concat(WiFi.hostname().c_str());
  payload.concat(F("&relay0="));
  payload.concat(RelaysCurrentState[0]);
  payload.concat(F("&relay1="));
  payload.concat(RelaysCurrentState[1]);
  payload.concat(F("&relay2="));
  payload.concat(RelaysCurrentState[2]);
  payload.concat(F("&vbatt="));
  payload.concat(mapfloat(an, 0.0, 1024.0, 0.0, 3.3));
  if (first_time)
  {
    first_time = false;
    rst_info *resetInfo;
    resetInfo = ESP.getResetInfoPtr();
    payload.concat(F("&boot="));
    payload.concat(resetInfo->reason);
  }

  yield();
  http.begin(F(HTTP_HOST), 80, payload);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    yield();
    payload = http.getString();
    if (extractParam(payload, "TIMESTAMP=", '&').length() > 0)
    {
      DateTime TSsync(extractParam(payload, "TIMESTAMP=", '&').toInt());
#ifdef DEBUG
      Serial.printf("TimeStamp %d\r\n", TSsync.secondstime());
#endif
      //if difference is too big - sync RTC
      if (abs(TSsync.secondstime() - Timestamp.secondstime()) > 10)
      {
#ifdef DEBUG
        Serial.printf("our clock is skew from server timestamp! - RESYNCing %d\r\n", TSsync.secondstime() - Timestamp.secondstime());
#endif
        RTC.adjust(TSsync);
      }
    }
    yield();
    if (extractParam(payload, "RELAY0=", '&').length() > 0)
    {
      settings.RelaysDesiredState[0] = extractParam(payload, "RELAY0=", '&').toInt();
      RelayCurPeriod[0] = 0;
      RelayCurPeriodLimit[0] = 0;
    }
    yield();
    if (extractParam(payload, "R0LT=", '&').length() > 0)
    {
      settings.RelayLowTime[0] = extractParam(payload, "R0LT=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay0 LT %d\r\n", settings.RelayLowTime[0]);
#endif
    }
    yield();
    if (extractParam(payload, "R0HT=", '&').length() > 0)
    {
      settings.RelayHighTime[0] = extractParam(payload, "R0HT=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay0 HT %d\r\n", settings.RelayHighTime[0]);
#endif
    }
    yield();
    if (extractParam(payload, "R0MINPERIOD=", '&').length() > 0)
    {
      settings.RelayMinPeriod[0] = extractParam(payload, "R0MINPERIOD=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay0 min period %d\r\n", settings.RelayMinPeriod[0]);
#endif
    }
    yield();
    if (extractParam(payload, "R0MAXPERIOD=", '&').length() > 0)
    {
      settings.RelayMaxPeriod[0] = extractParam(payload, "R0MAXPERIOD=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay0 max period %d\r\n", settings.RelayMaxPeriod[0]);
#endif
    }
    yield();
    if (extractParam(payload, "RELAY1=", '&').length() > 0)
    {
      settings.RelaysDesiredState[1] = extractParam(payload, "RELAY1=", '&').toInt();
      RelayCurPeriod[1] = 0;
      RelayCurPeriodLimit[1] = 0;
    }
    yield();
    if (extractParam(payload, "R1LT=", '&').length() > 0)
    {
      settings.RelayLowTime[1] = extractParam(payload, "R1LT=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay1 LT %d\r\n", settings.RelayLowTime[1]);
#endif
    }
    yield();
    if (extractParam(payload, "R1HT=", '&').length() > 0)
    {
      settings.RelayHighTime[1] = extractParam(payload, "R1HT=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay1 HT %d\r\n", settings.RelayHighTime[1]);
#endif
    }
    yield();
    if (extractParam(payload, "R1MINPERIOD=", '&').length() > 0)
    {
      settings.RelayMinPeriod[1] = extractParam(payload, "R1MINPERIOD=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay1 min period %d\r\n", settings.RelayMinPeriod[1]);
#endif
    }
    yield();
    if (extractParam(payload, "R1MAXPERIOD=", '&').length() > 0)
    {
      settings.RelayMaxPeriod[1] = extractParam(payload, "R1MAXPERIOD=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay1 max period %d\r\n", settings.RelayMaxPeriod[1]);
#endif
    }
    yield();
    if (extractParam(payload, "RELAY2=", '&').length() > 0)
    {
      settings.RelaysDesiredState[2] = extractParam(payload, "RELAY2=", '&').toInt();
      RelayCurPeriod[2] = 0;
      RelayCurPeriodLimit[2] = 0;
    }
    yield();
    if (extractParam(payload, "R2LT=", '&').length() > 0)
    {
      settings.RelayLowTime[2] = extractParam(payload, "R2LT=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay2 LT %d\r\n", settings.RelayLowTime[2]);
#endif
    }
    yield();
    if (extractParam(payload, "R2HT=", '&').length() > 0)
    {
      settings.RelayHighTime[2] = extractParam(payload, "R2HT=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay2 HT %d\r\n", settings.RelayHighTime[2]);
#endif
    }
    yield();
    if (extractParam(payload, "R2MINPERIOD=", '&').length() > 0)
    {
      settings.RelayMinPeriod[2] = extractParam(payload, "R2MINPERIOD=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay2 min period %d\r\n", settings.RelayMinPeriod[2]);
#endif
    }
    yield();
    if (extractParam(payload, "R2MAXPERIOD=", '&').length() > 0)
    {
      settings.RelayMaxPeriod[2] = extractParam(payload, "R2MAXPERIOD=", '&').toInt();
#ifdef DEBUG
      Serial.printf("Relay2 max period %d\r\n", settings.RelayMaxPeriod[2]);
#endif
    }
    yield();
    if (extractParam(payload, "SSID=", '&').length() > 0)
    {
      strcpy(settings.ssid, extractParam(payload, "SSID=", '&').c_str());
    }
    yield();
    if (extractParam(payload, "PASSWD=", '&').length() > 0)
    {
      strcpy(settings.password, extractParam(payload, "PASSWD=", '&').c_str());
    }
    yield();
    if (extractParam(payload, "CONINT=", '&').length() > 0)
    {
      settings.CommInterval = extractParam(payload, "CONINT=", '&').toInt();
      //need substract time needed to connect wifi from this (TODO: calculate average time spent while connecting)
    }
    yield();
    if (extractParam(payload, "TXPWR=", '&').length() > 0)
    {
      settings.txpower = extractParam(payload, "TXPWR=", '&').toFloat();
    }
    yield();
    if (extractParam(payload, "CONFSAVE=", '&').length() > 0)
    {
#ifdef DEBUG
      Serial.printf("eeprom write\r\n");
#endif
      saveSettings();
    }
    yield();
    if (extractParam(payload, "REBOOT=", '&').length() > 0)
    {
#ifdef DEBUG
      Serial.printf("mcu reset\r\n");
#endif
      for (int a = 0; a <= 60; a++)
      {
        delay(1000);
#ifdef DEBUG
        Serial.print(".");
#endif
      }
      ESP.restart();
    }
  }
#ifdef DEBUG
  else
  {
    Serial.printf("HTTP GET %d error: %s\r\n", httpCode, http.errorToString(httpCode).c_str());
  }
#endif

  http.end();

  Radio_OFF();
}

void Radio_OFF()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);  
}

String extractParam(String& authReq, const String& param, const char delimit)
{
  int _begin = authReq.indexOf(param);
  if (_begin == -1)
    return "";
  return authReq.substring(_begin + param.length(), authReq.indexOf(delimit, _begin + param.length()));
}

void loadSettings()
{
  uint8_t buffer[sizeof(settings) + 1];  // Use the last byte for CRC

  for (int i = 0; i < sizeof(buffer); i++)
  {
    buffer[i] = uint8_t(EEPROM.read(i));
  }

  // Check CRC
  if (OneWire::crc8(buffer, sizeof(settings)) == buffer[sizeof(settings)])
  {
    memcpy(&settings, buffer, sizeof(settings));
#ifdef DEBUG
    Serial.println("loading settings");
#endif
  }
  else
  {
    for (int a = 0; a < 3; a++)
    {
      settings.RelaysDesiredState[a] = 0;
      settings.RelayLowTime[a] = 25200;
      settings.RelayHighTime[a] = 82800;
      settings.RelayMinPeriod[a] = 5;
      settings.RelayMaxPeriod[a] = 15;
    }
    settings.CommInterval = 60;
    settings.txpower = 10.0;
    strcpy_P(settings.ssid, PSTR(SSID_NAME));
    strcpy_P(settings.password, PSTR(SSID_PASSWORD));
#ifdef DEBUG
    Serial.println("init settings");
#endif
  }
}

void saveSettings()
{
  uint8_t buffer[sizeof(settings) + 1];  // Use the last byte for CRC

  memcpy(buffer, &settings, sizeof(settings));
  buffer[sizeof(settings)] = OneWire::crc8(buffer, sizeof(settings));

  for (int i = 0; i < sizeof(buffer); i++)
  {
    EEPROM.write(i, buffer[i]);
  }
  EEPROM.commit();
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int32_t AnalogRead()
{
  int32_t Raw = 0;

  //sample average
  for (int i = 0; i < ADC_FILTER1_SAMPLES; i++) Raw += analogRead(A0);
  Raw /= ADC_FILTER1_SAMPLES;

  return Raw;
}

bool is_between(const int32_t value, const int32_t rangestart, const int32_t rangeend)
{
  if (rangestart <= rangeend)
  {
    return (rangestart <= value) && (rangeend > value);
  }
  //cross midnight
  else
  {
    return (rangestart <= value) || (rangeend > value);
  }
}
