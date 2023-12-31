
#define _TASK_WDT_IDS

#include <Adafruit_NeoPixel.h>
#include <ArduinoOTA.h>
#include <ESP8266Wifi.h>
#include <ESP8266mDNS.h>
#include <TaskScheduler.h>
#include <WiFiUDP.h>
#include <string.h>
#include <arduino.h>

#define ledpin D3
#define numtasks 1
#define maxcolors 32
#define port 8200
#define commandlen 128

const char *ssid = "TRNNET-2G";
const char *password = "ripcord1";

const char *hostname = "ESP_A_wirewrap";

void parse_command();
void check_udp();
void t1_callback();

WiFiUDP Udp;
WiFiUDP Udp1;
WiFiServer Tcp(port);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(numtasks, ledpin, NEO_GRB + NEO_KHZ800);
Scheduler runner;

Task *tasks[numtasks];

unsigned int numcolors[numtasks], currcolor[numtasks];
unsigned long colors[numtasks][maxcolors];
unsigned long steps[numtasks][maxcolors], currstep[numtasks];
unsigned char first[numtasks];
char command[commandlen + 1];

void connect()
{
  // WiFi.mode(WIFI_AP);
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // MDNS.begin("esp123");

  Serial.println(WiFi.localIP());
  Udp.begin(port);
  Tcp.begin();

  ArduinoOTA.begin();
}

IRAM_ATTR void reset()
{
  // void reset(){
  Serial.println("reset");

  memset(command, 0, commandlen + 1);
  strcpy(command, "0 500 000000");
  parse_command();

  Udp1.beginPacket("192.168.1.255", port);
  Udp1.print("0 500 000000");
  Udp1.endPacket();
}

void check_udp()
{

  int packetsize = Udp.parsePacket();
  if (packetsize)
  {

    memset(command, 0, commandlen + 1);
    Udp.read(command, commandlen);

    Serial.print("UDP: ");
    Serial.println(command);

    parse_command();
  }
}

void check_tcp()
{

  static WiFiClient client;
  static long unsigned noDataCnt;

  if (!client)
  {
    noDataCnt = 0;
    client = Tcp.available();
    if (client)
    {
      Serial.print("Connect:   ");
      Serial.println(client.remoteIP());
    }
  }
  else
  {
    int test = client.available();
    // Serial.print(client.remoteIP());
    // Serial.print("    ");
    // Serial.println(noDataCnt);
    if (test > 0)
    {
      noDataCnt = 0;
      memset(command, 0, commandlen + 1);
      client.readBytesUntil('\n', command, commandlen);

      Serial.print("TCP: ");
      Serial.println(command);
      parse_command();
    }
    else
    {
      noDataCnt++;
      // if(noDataCnt>5012)
      // client.stop(0);
    }
  }
}

void parse_command()
{

  unsigned ledId, pause;
  char *ptr, *ptr1;
  char *sav, *sav1;

  ptr = strtok_r(command, " ", &sav);
  ledId = atoi(ptr);
  (*tasks[ledId]).disable();

  currcolor[ledId] = 0;
  currstep[ledId] = 0;
  ptr = strtok_r(NULL, " ", &sav);

  if (strcmp(ptr, "+"))
  {
    numcolors[ledId] = 0;
    pause = atoi(ptr);
    (*tasks[ledId]).setInterval(pause);
  }

  while (ptr != NULL)
  {
    ptr = strtok_r(NULL, " ", &sav);
    if (ptr != NULL)
    {

      ptr = strupr(ptr);
      if (strchr(ptr, 'X') != NULL)
      {
        ptr1 = strtok_r(ptr, "X", &sav1);
        steps[ledId][numcolors[ledId]] = atol(ptr1);
        ptr1 = strtok_r(NULL, "X", &sav1);
      }
      else
      {
        steps[ledId][numcolors[ledId]] = 1;
        ptr1 = ptr;
      }
      colors[ledId][numcolors[ledId]] = strtol(ptr1, NULL, 16);
      numcolors[ledId]++;
    }
  }
  strip.setPixelColor(ledId, colors[ledId][currcolor[ledId]]);
  strip.show();
  first[ledId] = 1;
  (*tasks[ledId]).enable();
}

void t1_callback()
{
  int ledId;

  Task &taskRef = runner.currentTask();
  ledId = taskRef.getId();

  if (steps[ledId][currcolor[ledId]] != 0 and not first[ledId])
  {
    if (currstep[ledId] == steps[ledId][currcolor[ledId]] - 1)
    {

      if (currcolor[ledId] < numcolors[ledId] - 1)
        currcolor[ledId]++;
      else
        currcolor[ledId] = 0;

      currstep[ledId] = 0;
      strip.setPixelColor(ledId, colors[ledId][currcolor[ledId]]);
      strip.show();
    }
    else
      currstep[ledId]++;
  }
  first[ledId] = 0;
}

void setup(void)
{

  Serial.begin(9600);
  connect();
  ArduinoOTA.begin();

  pinMode(D1, INPUT);
  attachInterrupt(digitalPinToInterrupt(D1), reset, RISING);

  pinMode(D6, OUTPUT);
  digitalWrite(D6, LOW);

  for (int i = 0; i <= numtasks - 1; i++)
  {
    numcolors[i] = 1;
    currcolor[i] = 0;
    currstep[i] = 0;
    first[i] = 0;
    for (int j = 0; j <= maxcolors - 1; j++)
    {
      colors[i][j] = 0;
      steps[i][j] = 1;
    }
  }

  runner.init();
  for (int i = 0; i <= numtasks - 1; i++)
  {
    tasks[i] = new Task(500, TASK_FOREVER, &t1_callback);
    (*tasks[i]).setId(i);

    runner.addTask((*tasks[i]));
    (*tasks[i]).enable();
  }
  pinMode(D5, INPUT_PULLUP);
  strip.begin();
}

void loop(void)
{
  runner.execute();
  ArduinoOTA.handle();

  // MDNS.update();

  check_udp();
  check_tcp();
}
