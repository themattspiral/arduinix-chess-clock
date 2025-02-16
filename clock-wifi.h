#ifndef _CLOCK_WIFI_H
#define _CLOCK_WIFI_H

#include <WiFiS3.h>

#include "clock-strings.h"
#include "clock-secrets.h"

const char* SSID = SECRET_SSID;
const char* PASS = SECRET_PASS;

const char* TOPIC_LEFT = SECRET_TOPIC_LEFT;
const char* TOPIC_RIGHT = SECRET_TOPIC_RIGHT;
const char* NTFY_SERVER = "ntfy.sh";

const char* CONN_KEEP_ALIVE = "keep-alive";
const char* CONN_CLOSE = "close";
const char* NTFY_REQUEST = "POST /%s HTTP/1.1\nHost: ntfy.sh\nContent-Type: text/plain\nContent-Length: %d\nConnection: %s\n\n%s\n";

int wifiConnectionStatus = WL_IDLE_STATUS;
WiFiClient client;

inline void printMacAddress(byte mac[]) {
  for (int i = 0; i < 6; i++) {
    if (i > 0) {
      Serial.print(":");
    }
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
  }
  Serial.println();
}

inline void printNetworkInfo() {
  Serial.print("IP Address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  Serial.print("MAC Address: ");
  byte mac[6];
  WiFi.macAddress(mac);
  printMacAddress(mac);
}

inline void printWifiInfo() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // AP/Router MAC
  byte bssid[6];
  Serial.print("BSSID: ");
  WiFi.BSSID(bssid);
  printMacAddress(bssid);

  Serial.print("Signal Strength (RSSI): ");
  long rssi = WiFi.RSSI();
  Serial.println(rssi);
}

inline const char* statusCodeString(int status) {
  switch(status) {
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
    case WL_AP_LISTENING: return "WL_AP_LISTENING";
    case WL_AP_CONNECTED: return "WL_AP_CONNECTED";
    case WL_AP_FAILED: return "WL_AP_FAILED";
    case WL_NO_SHIELD: return "WL_NO_SHIELD/WL_NO_SHIELD";
    default: return "Unknown";
  }
}

inline bool setupWifi() {
  wifiConnectionStatus = WiFi.status();

  if (wifiConnectionStatus == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    return false;
  } else {
    Serial.print("WiFi module status: ");
    Serial.println(statusCodeString(wifiConnectionStatus));
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the WiFi module firmware.");
    Serial.print("Current: ");
    Serial.println(fv);
    Serial.print("Latest: ");
    Serial.println(WIFI_FIRMWARE_LATEST_VERSION);
  } else {
    Serial.print("WiFi module firmware is up-to-date: ");
    Serial.println(fv);
  }

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(SSID);
  wifiConnectionStatus = WiFi.begin(SSID, PASS);

  if (wifiConnectionStatus == WL_CONNECTED) {
    Serial.println("WiFi Connected!");
    printWifiInfo();
    printNetworkInfo();
  } else {
    Serial.print("Error connecting to WiFi network! Status: ");
    Serial.println(statusCodeString(wifiConnectionStatus));
  }

  return wifiConnectionStatus == WL_CONNECTED;
}

inline bool loopCheckWifiConnection(unsigned long loopNow) {
  // if (loopNow - lastWifiCheckReconnectTime >= WIFI_CHECK_AND_RECONNECT_FREQUENCY_MS) {
    Serial.println("Checking WiFi Status...");

    unsigned long before = micros();
    int currentStatus = WiFi.status();
    unsigned long diff = micros() - before;

    // lastWifiCheckReconnectTime = loopNow;

    // take action only if status has changed
    if (wifiConnectionStatus != currentStatus) {
      if (currentStatus == WL_CONNECTED) {
        Serial.println("WiFi Connected!");
        printWifiInfo();
        printNetworkInfo();
      } else {
        Serial.print("WiFi Connection Lost - Current Status: ");
        Serial.println(statusCodeString(currentStatus));

        Serial.print("Attempting to reconnect to SSID: ");
        Serial.println(SSID);

        WiFi.begin(SSID, PASS);
      }

      wifiConnectionStatus = currentStatus;
    }
    else {
      Serial.print("No WiFi status change. Time to check (us): ");
      Serial.println(diff);
    }

    return wifiConnectionStatus == WL_CONNECTED;
  // }
}

#define playerTopic(leftPlayersTurn) (leftPlayersTurn ? TOPIC_LEFT : TOPIC_RIGHT)

inline void notifyNewGame(bool leftPlayersTurn, const char* label) {
  char messageBuffer[100] = "";
  char reqBuffer1[200] = "";
  char reqBuffer2[200] = "";

  snprintf(messageBuffer, 100, NEW_GAME_MSG, label, playerString(leftPlayersTurn));
  snprintf(reqBuffer1, 200, NTFY_REQUEST, TOPIC_LEFT, strlen(messageBuffer), CONN_KEEP_ALIVE, messageBuffer);
  snprintf(reqBuffer2, 200, NTFY_REQUEST, TOPIC_RIGHT, strlen(messageBuffer), CONN_CLOSE, messageBuffer);

  if (client.connect(NTFY_SERVER, 80)) {
    client.println(reqBuffer1);
    client.println(reqBuffer2);
    client.stop();
  }
}

inline void notifyPlayerTurn(bool leftPlayersTurn) {
  char reqBuffer[150] = "";
  snprintf(reqBuffer, 150, NTFY_REQUEST, playerTopic(leftPlayersTurn), strlen(TURN_CHANGE_MSG), CONN_CLOSE, TURN_CHANGE_MSG);

  if (client.connect(NTFY_SERVER, 80)) {
    client.println(reqBuffer);
    client.stop();
  }
}

inline void notifyTimeout(bool leftPlayersTurn) {
  char messageBuffer[100] = "";
  char reqBuffer1[200] = "";
  char reqBuffer2[200] = "";

  snprintf(messageBuffer, 100, TIMEOUT_MSG, playerString(leftPlayersTurn));
  snprintf(reqBuffer1, 200, NTFY_REQUEST, TOPIC_LEFT, strlen(messageBuffer), CONN_KEEP_ALIVE, messageBuffer);
  snprintf(reqBuffer2, 200, NTFY_REQUEST, TOPIC_RIGHT, strlen(messageBuffer), CONN_CLOSE, messageBuffer);

  if (client.connect(NTFY_SERVER, 80)) {
    client.println(reqBuffer1);
    client.println(reqBuffer2);
    client.stop();
  }
}

#endif _CLOCK_WIFI_H
