/**
 * Created by K. Suwatchai (Mobizt)
 *
 * Email: k_suwatchai@hotmail.com
 *
 * Github: https://github.com/mobizt/Firebase-ESP8266
 *
 * Copyright (c) 2023 mobizt
 *
 */

#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#include <FirebaseESP32.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#elif defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#include <FirebaseESP8266.h>
#endif

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

/* 1. Define the WiFi credentials */
#define WIFI_SSID "WIFI_AP"
#define WIFI_PASSWORD "WIFI_PASSWORD"

// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. Define the API Key */
#define API_KEY "API_KEY"

/* 3. Define the RTDB URL */
#define DATABASE_URL "URL" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "USER_EMAIL"
#define USER_PASSWORD "USER_PASSWORD"

// Define Firebase Data objects
FirebaseData fbdo;
FirebaseData stream;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis1;

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiMulti multi;
#endif

void setup()
{

  Serial.begin(115200);

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  multi.addAP(WIFI_SSID, WIFI_PASSWORD);
  multi.run();
#else
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#endif

  Serial.print("Connecting to Wi-Fi");
  unsigned long ms = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    if (millis() - ms > 10000)
      break;
#endif
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  // The WiFi credentials are required for Pico W
  // due to it does not have reconnect feature.
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  config.wifi.clearAP();
  config.wifi.addAP(WIFI_SSID, WIFI_PASSWORD);
#endif

  // Or use legacy authenticate method
  // config.database_url = DATABASE_URL;
  // config.signer.tokens.legacy_token = "<database secret>";

  // To connect without auth in Test Mode, see Authentications/TestMode/TestMode.ino

  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);

  // You can use TCP KeepAlive For more reliable stream operation and tracking the server connection status, please read this for detail.
  // https://github.com/mobizt/Firebase-ESP8266#enable-tcp-keepalive-for-reliable-http-streaming
  // stream.keepAlive(5, 5, 1);

  if (!Firebase.beginStream(stream, "/test/stream/data"))
    Serial.printf("sream begin error, %s\n\n", stream.errorReason().c_str());

  /** Timeout options, below is default config.

  //WiFi reconnect timeout (interval) in ms (10 sec - 5 min) when WiFi disconnected.
  config.timeout.wifiReconnect = 10 * 1000;

  //Socket begin connection timeout (ESP32) or data transfer timeout (ESP8266) in ms (1 sec - 1 min).
  config.timeout.socketConnection = 30 * 1000;

  //ESP32 SSL handshake in ms (1 sec - 2 min). This option doesn't allow in ESP8266 core library.
  config.timeout.sslHandshake = 2 * 60 * 1000;

  //Server response read timeout in ms (1 sec - 1 min).
  config.timeout.serverResponse = 10 * 1000;

  //RTDB Stream keep-alive timeout in ms (20 sec - 2 min) when no server's keep-alive event data received.
  config.timeout.rtdbKeepAlive = 45 * 1000;

  //RTDB Stream reconnect timeout (interval) in ms (1 sec - 1 min) when RTDB Stream closed and want to resume.
  config.timeout.rtdbStreamReconnect = 1 * 1000;

  //RTDB Stream error notification timeout (interval) in ms (3 sec - 30 sec). It determines how often the readStream
  //will return false (error) when it called repeatedly in loop.
  config.timeout.rtdbStreamError = 3 * 1000;

  */
}

void loop()
{

  // Firebase.ready() should be called repeatedly to handle authentication tasks.

  if (!Firebase.ready())
    return;

  // Please avoid to use delay or any third party libraries that use delay internally to wait for hardware to be ready in this loop.

  if (!Firebase.readStream(stream))
    Serial.printf("sream read error, %s\n\n", stream.errorReason().c_str());

  if (stream.streamTimeout())
  {
    Serial.println("stream timed out, resuming...\n");

    if (!stream.httpConnected())
      Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
  }

  if (stream.streamAvailable())
  {
    Serial.printf("sream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
                  stream.streamPath().c_str(),
                  stream.dataPath().c_str(),
                  stream.dataType().c_str(),
                  stream.eventType().c_str());
    printResult(stream); // see addons/RTDBHelper.h
    Serial.println();

    // This is the size of stream payload received (current and max value)
    // Max payload size is the payload size under the stream path since the stream connected
    // and read once and will not update until stream reconnection takes place.
    // This max value will be zero as no payload received in case of ESP8266 which
    // BearSSL reserved Rx buffer size is less than the actual stream payload.
    Serial.printf("Received stream payload size: %d (Max. %d)\n\n", stream.payloadLength(), stream.maxPayloadLength());
  }

  if (millis() - sendDataPrevMillis1 > 15000)
  {
    sendDataPrevMillis1 = millis();

    // Create demo data
    uint8_t data[256];
    for (int i = 0; i < 256; i++)
      data[i] = i;
    data[255] = rand();

    Serial.printf("Set BLOB... %s\n", Firebase.setBlob(fbdo, "/test/stream/data", data, sizeof(data)) ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Free Heap, %d\n", (int)ESP.getFreeHeap());
    Serial.println();
  }
}