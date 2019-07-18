#include <ESP8266WiFi.h>
#include "credentials.h"

#define INPUTX 16
#define INPUTY 14
#define GREENLED 2
#define REDLED 0

#define WAITONTIME         5000        //In milliseconds
#define WAITOFFTIME        5000        //In milliseconds

#define VIBE_THRESHOLD  0.10    //unitless
#define COUNTS 5 //moving average counts

const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASS;

const char MakerIFTTT_Key[] = IFTTT_KEY;
const char MakerIFTTT_Event[] = IFTTT_EVENT;

const char* host = "maker.ifttt.com";

//These messages are sent as notifications when the dryer has finished the cycle.
char* textToSend[] = {"Come and get me while I'm all warm (or wet).",
                      "Hurry up before I eat one of your socks!",
                      "Don't you forget about me!",
                      "Washer/Dryer is finished!",
                      "Stop being lazy. Get your clothes.",
                      "Do you want wrinkled or stinky clothes? Hurry up!",
                      "It's time to move some clothes!"
                     };

int rawx = 0;
int rawy = 0;
int lastx = 0;
int lasty = 0;
float movingx = 0;
float movingy = 0;

static unsigned long runStartTime;

//These are the various states during monitoring. WAITON and WAITOFF both have delays
//that can be set with the WAITONTIME and WAITOFFTIME parameters
enum STATE {
  WAITON,    //Dryer is off, waiting for it to turn on
  RUNNING,   //Exceeded current threshold, waiting for dryer to turn off
  WAITOFF,    //Dryer is off, sends notifications, and returns to WAITON
  DISCONNECTED  //internet connection is lost
};
//This keeps up with the current state of the FSM, default startup setting is the first state of waiting for the dryer to turn on.
STATE dryerState = WAITON;

//used for testing connection
WiFiClient client;
WiFiServer server(80);

const unsigned long wdCheckInterval = 60000;  //check after a minute into program and then increment below value
unsigned long wdLastChecked;

boolean debug = 0;

boolean nopost = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  Serial.println("Starting Dryer Messenger.");
  //Initialize random seed using unused analog 0
  randomSeed(analogRead(A0));

  pinMode(INPUTX, INPUT);
  pinMode(INPUTY, INPUT);
  pinMode(GREENLED, OUTPUT);
  pinMode(REDLED, OUTPUT);
  digitalWrite(GREENLED, LOW);
  digitalWrite(REDLED, LOW);

  Serial.println("Setting up Wi-Fi...");

  //connect to network
  setupWiFi();

  //connect to internet. returns 0 if no connection
  if (!IsNetworkUp()) {
    Serial.print("Error joining network!");
    delay(10);
    digitalWrite(GREENLED, HIGH);
    digitalWrite(REDLED, HIGH);
    dryerState = DISCONNECTED;
  }

  digitalWrite(REDLED, LOW);

  Serial.println("Now running. Debug options:");
  Serial.println("Press '1' to send a test tweet");
  Serial.println("Press '2' to toggle measurements");
  Serial.println("Press '3' to access the network terminal");
  Serial.println("Press '4' to toggle ability to post to web");
  Serial.println("");
}

void loop() {
  monitorDryer();
  digitalWrite(GREENLED, !digitalRead(GREENLED)); //blink green light. keep red constant
  //digitalWrite(REDLED, LOW);

  if (Serial.available())
  {
    char c = Serial.read();
    switch (c)
    {
      case '1':
        PostToTwitter(GetAMessage());

        break;
      case '2':
        debug = !debug;
        break;
      case '3':
        Terminal();

        //prints status of connection
        IsNetworkUp();

        break;

      case '4':
        nopost = !nopost;
        if (nopost) {
          Serial.println("Ability to post to web Disabled");
        }
        else if (!nopost) {
          Serial.println("Ability to post to web Enabled");
        }
        break;

      default:
        break;
    }

  }
} //loop


void monitorDryer()
{
  static unsigned long startTime;

  static boolean waitCheck;

  switch (dryerState)
  {

    case WAITON:

      //watchdog timer
      if (millis() - wdLastChecked > wdCheckInterval)
      {
        //debug
        Serial.print(F("Current time: "));
        Serial.println(millis());
        Serial.print(F("Last Checked time: "));
        Serial.println(wdLastChecked);
        Serial.print(F("Check interval: "));
        Serial.println(wdCheckInterval);

        if ( !IsNetworkUp()) {
          // dryerState = DISCONNECTED;
        }
      }

      if (takeVibeMeasurement() > VIBE_THRESHOLD)
      {
        if (!waitCheck)
        {
          startTime = millis();
          waitCheck = true;
          Serial.println("Started watching in WAITON. startTime: " + (String) startTime);
        }
        else
        {
          if ( (millis() - startTime ) >= WAITONTIME)
          {
            Serial.println("Changing dryer state to RUNNING");
            dryerState = RUNNING;
            waitCheck = false;
            runStartTime = millis();
          }
        }
      }
      else
      {
        if (waitCheck)
        {
          Serial.println("False alarm, not enough time elapsed to advance to running");
        }
        waitCheck = false;
      }
      break;

    case RUNNING:

      digitalWrite(REDLED, !digitalRead(REDLED));
      digitalWrite(GREENLED, !digitalRead(REDLED)); //make sure leds dont flash at same time

      if (takeVibeMeasurement() < VIBE_THRESHOLD)
      {
        Serial.println("Changing dryer state to WAITOFF");
        dryerState = WAITOFF;
      }

      break;

    case WAITOFF:
      if (!waitCheck)
      {
        startTime = millis();
        waitCheck = true;
        Serial.println("Entered WAITOFF. startTime: " + (String) startTime);
        digitalWrite(REDLED, LOW);
        digitalWrite(GREENLED, LOW);
      }

      if (takeVibeMeasurement() > VIBE_THRESHOLD)
      {
        Serial.println("False Alarm, dryer not off long enough, back to RUNNING we go!");
        waitCheck = false;
        dryerState = RUNNING;
      }
      else
      {
        if ( (millis() - startTime) >= WAITOFFTIME)
        {
          Serial.println("Dryer cycle complete");
          PostToTwitter(GetAMessage());

          Serial.println("Resetting state back to WAITON");
          dryerState = WAITON;
          waitCheck = false;

          digitalWrite(REDLED, LOW);
        }
      }

      //    case DISCONNECTED:
      //      Serial.println("State DISCONNECTED. Connection to internet lost. Waiting");
      //
      //      digitalWrite(GREENLED, LOW);
      //      digitalWrite(REDLED, LOW);
      //
      //      byte i = 0; //used for disconnected state
      //      while (!IsNetworkUp()) {
      //
      //        delay(exp(i) * 1000); //wait for exponential time
      //        Serial.print(".");
      //        ++i;  //rolls over after 49 min
      //      }

      //dryerState = WAITON;

      //Serial.println("Changing dryer state to WAITON");

  }
}

float calcMovingAverage(float prev, float next, int count) {
  //computationally efficient
  return (prev * count + next - prev) / count;
}

float calcMagnitude(float x, float y) {
  return sqrt(pow(x, 2) + pow(y, 2));
}

boolean setupWiFi()
{
  //uncomment if router does take care of IP addressing
  Serial.println("connecting ...");
  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //test whether or not client can connect
  if (IsNetworkUp())
  {
    return true;
  } else
  {
    return false;
  }

}

boolean IsNetworkUp()
{
  if (client.connect(host, 80)) {
    Serial.println("...connected...");
    client.println("GET / search ? q = arduino HTTP / 1.0");
    client.stop();
    client.println();
    wdLastChecked = millis(); //used for wd
    return true;
  } else {
    client.stop();
    Serial.println("...connection failed...");
    wdLastChecked = millis(); //used for wd
    return false;
  }
}

char* GetAMessage()
{
  int lengthOfArray = sizeof(textToSend) / sizeof(char*);
  int randomNum = random(0, lengthOfArray);
  return textToSend[randomNum];
}

void PostToTwitter(char* charStr)
{
  Serial.println("Posting to Twitter...");
  if (!IsNetworkUp())
  {
    Serial.print("Not connected to network. Reconnecting");
    setupWiFi();
    delay(1000);
  }

  Serial.print("Connecting to ");
  Serial.println(host);

  const int httpPort = 80;//This is port at destination... not of sending ESP8266
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
  }

  Serial.println("Posting: ");
  Serial.println(charStr);

  //trigger message
  String url = "/trigger/";
  url += MakerIFTTT_Event;
  url += "/with/key/";
  url += MakerIFTTT_Key;

  String head = "POST " + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Content-Type: application/x-www-form-urlencoded\r\n" +
                "Content-Length: ";
  String content = "value1=" + (String)charStr + //replace space with %20?
                   "&value2=" + String((int)(millis() - runStartTime) / 1000 / 60) + //Note the "&" in front
                   "&value3=" + String((millis() - runStartTime) / 1000 % 60 );

  String full = head + (String)content.length() + "\r\n\r\n" + content + "\r\n";

  Serial.println(full);

  if (!nopost) {
    client.print(full);
  }
  else if (nopost) {
    Serial.println("No message sent. Posting disabled");
  }

  client.stop();

  delay(1000);
}

long MilSecsToMins(long milli)
{
  double totalRun = (double) milli / 1000 / 60;

  return (long) totalRun;
}

void Terminal()
{

  Serial.println("IP Addr : " + WiFi.localIP());

}

double takeVibeMeasurement() {

  rawx = digitalRead(INPUTX);
  rawy = digitalRead(INPUTY);
  movingx = calcMovingAverage(movingx, abs(rawx - lastx), COUNTS);
  movingy = calcMovingAverage(movingy, abs(rawy - lasty), COUNTS);

  float mag = calcMagnitude(movingx, movingy);

  if (debug) {
    Serial.print("x,y,mag:");
    Serial.print(movingx);
    Serial.print(',');
    Serial.print(movingy);
    Serial.print(',');
    Serial.println(mag);
  }

  lastx = digitalRead(INPUTX);
  lasty = digitalRead(INPUTY);

  delay(100);

  return mag;
}

