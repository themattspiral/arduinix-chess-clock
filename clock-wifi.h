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
const int NTFY_SERVER_PORT = 80;

const char* CONN_KEEP_ALIVE = "keep-alive";
const char* CONN_CLOSE = "close";
const char* NTFY_REQUEST = "POST /%s HTTP/1.1\nHost: ntfy.sh\nContent-Type: text/plain\nContent-Length: %d\nConnection: %s\n\n%s\n";

int wifiConnectionStatus = WL_IDLE_STATUS;

// better characterized as a TCP client
WiFiClient client;

/*
 * ============================================================================
 *  Util Functions
 * ============================================================================
 */

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

/*
 * ============================================================================
 * Setup
 * 
 * Connect to WPA WiFi network as defined in clock-secrets, 
 * ============================================================================
 */

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

  // After WiFi.begin(), reconnect attemps are handled automatically by 
  // the ESP32-S3. This includes after initial failure (tries until it works),
  // or if it disconnects after initially working. 
  //
  // Here, just log the initial connection status (check status again later
  // before attempting to send a notification if initially not connected)
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

/*
 * ============================================================================
 * WiFi Notifications
 * 
 * Using WiFiClient (a TCP client), connect to ntfy.sh server to send HTTP
 * POST messages for notification events (disconnect when finished without)
 * checking on the response).
 * 
 * void notifyNewGame(bool leftPlayersTurn, const char* label)
 * void notifyPlayerTurn(bool leftPlayersTurn)
 * void notifyTimeout(bool leftPlayersTurn)
 * ============================================================================
 */

#define playerTopic(leftPlayersTurn) (leftPlayersTurn ? TOPIC_LEFT : TOPIC_RIGHT)

const int MSG_BUFFER_SIZE = 100;
const int REQ_BUFFER_SIZE = 200;

inline void notifyNewGame(bool leftPlayersTurn, const char* label) {
  // If WiFi was not previously connected, re-check status to be sure.
  //  - If WiFi is still not connected, exit quickly without trying a 
  //    connection to ntfy.sh, so a lengthy (10s) timeout is avoided
  //  - If WiFi is now actually connected, try sending notification
  // 
  // Note: WiFi reconnecting is handled automatically by the ESP32-S3
  if (wifiConnectionStatus != WL_CONNECTED ) {
    wifiConnectionStatus = WiFi.status();
    
    if (wifiConnectionStatus != WL_CONNECTED) {
      return;
    }
  }

  char msgBuffer[MSG_BUFFER_SIZE] = "";
  char reqBuffer1[REQ_BUFFER_SIZE] = "";
  char reqBuffer2[REQ_BUFFER_SIZE] = "";

  snprintf(msgBuffer, MSG_BUFFER_SIZE, NEW_GAME_MSG, label, playerString(leftPlayersTurn));
  snprintf(reqBuffer1, REQ_BUFFER_SIZE, NTFY_REQUEST, TOPIC_LEFT, strlen(msgBuffer), CONN_KEEP_ALIVE, msgBuffer);
  snprintf(reqBuffer2, REQ_BUFFER_SIZE, NTFY_REQUEST, TOPIC_RIGHT, strlen(msgBuffer), CONN_CLOSE, msgBuffer);

  if (client.connect(NTFY_SERVER, NTFY_SERVER_PORT)) {
    client.println(reqBuffer1);
    client.println(reqBuffer2);
    client.stop();
  } else {
    // if connection to ntfy.sh failed (e.g. timed out), it's most likely that
    // WiFi disconnected: update WiFi status to avoid timeouts while it's down
    wifiConnectionStatus = WiFi.status();
  }
}

inline void notifyPlayerTurn(bool leftPlayersTurn) {
  // If WiFi was not previously connected, re-check status to be sure.
  //  - If WiFi is still not connected, exit quickly without trying a 
  //    connection to ntfy.sh, so a lengthy (10s) timeout is avoided
  //  - If WiFi is now actually connected, try sending notification
  // 
  // Note: WiFi reconnecting is handled automatically by the ESP32-S3
  if (wifiConnectionStatus != WL_CONNECTED ) {
    wifiConnectionStatus = WiFi.status();
    
    if (wifiConnectionStatus != WL_CONNECTED) {
      return;
    }
  }

  char reqBuffer[REQ_BUFFER_SIZE] = "";
  snprintf(reqBuffer, REQ_BUFFER_SIZE, NTFY_REQUEST, playerTopic(leftPlayersTurn), strlen(TURN_CHANGE_MSG), CONN_CLOSE, TURN_CHANGE_MSG);

  if (client.connect(NTFY_SERVER, NTFY_SERVER_PORT)) {
    client.println(reqBuffer);
    client.stop();
  } else {
    // if connection to ntfy.sh failed (e.g. timed out), it's most likely that
    // WiFi disconnected: update WiFi status to avoid timeouts while it's down
    wifiConnectionStatus = WiFi.status();
  }
}

inline void notifyTimeout(bool leftPlayersTurn) {
  // If WiFi was not previously connected, re-check status to be sure.
  //  - If WiFi is still not connected, exit quickly without trying a 
  //    connection to ntfy.sh, so a lengthy (10s) timeout is avoided
  //  - If WiFi is now actually connected, try sending notification
  // 
  // Note: WiFi reconnecting is handled automatically by the ESP32-S3
  if (wifiConnectionStatus != WL_CONNECTED ) {
    wifiConnectionStatus = WiFi.status();
    
    if (wifiConnectionStatus != WL_CONNECTED) {
      return;
    }
  }

  char msgBuffer[MSG_BUFFER_SIZE] = "";
  char reqBuffer1[REQ_BUFFER_SIZE] = "";
  char reqBuffer2[REQ_BUFFER_SIZE] = "";

  snprintf(msgBuffer, MSG_BUFFER_SIZE, TIMEOUT_MSG, playerString(leftPlayersTurn));
  snprintf(reqBuffer1, REQ_BUFFER_SIZE, NTFY_REQUEST, TOPIC_LEFT, strlen(msgBuffer), CONN_KEEP_ALIVE, msgBuffer);
  snprintf(reqBuffer2, REQ_BUFFER_SIZE, NTFY_REQUEST, TOPIC_RIGHT, strlen(msgBuffer), CONN_CLOSE, msgBuffer);

  if (client.connect(NTFY_SERVER, NTFY_SERVER_PORT)) {
    client.println(reqBuffer1);
    client.println(reqBuffer2);
    client.stop();
  } else {
    // if connection to ntfy.sh failed (e.g. timed out), it's most likely that
    // WiFi disconnected: update WiFi status to avoid timeouts while it's down
    wifiConnectionStatus = WiFi.status();
  }
}

#endif _CLOCK_WIFI_H
