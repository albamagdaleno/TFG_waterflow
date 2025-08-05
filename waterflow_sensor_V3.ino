#include "WiFi.h"
#include "esp_wpa2.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <SPI.h>
#include <SD.h>


// WiFi Credentials Unileon
//const char* WIFI_SSID = "Livebox6-D220";
//const char* WIFI_PASS = "X3Er3wZg94Ti";

const char* WIFI_SSID = "unileon";
const char* WIFI_USERNAME = "amagdr00";
const char* WIFI_PASS = ""; 

// Adafruit IO
const char* AIO_SERVER      "io.adafruit.com" ;
const int AIO_SERVERPORT  1883 ;
const char* AIO_USERNAME    "amagdr00" ;
const char* AIO_KEY         "";

// Objeto WiFi
WiFiClient client;

// Cliente MQTT
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Feeds de los caudales
Adafruit_MQTT_Publish caudalFrio = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/caudal_frio");
Adafruit_MQTT_Publish caudalCaliente = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/caudal_caliente");

// Sensor de caudal frio
const int SENSOR_FRIO = 15;
volatile int contadorFrio = 0;
unsigned long lastTimeFrio = 0;

// Sensor de caudal caliente
const int SENSOR_CALIENTE = 27;
volatile int contadorCaliente = 0;
unsigned long lastTimeCaliente = 0;

//Factor de calibracion del sensor YF-S201C
float calibrationFactor = 4.5;

//Adaptador microSD y archivo
const int SENSOR_SD = 5; 
File logFile;

void IRAM_ATTR pulsosFrio() {
  contadorFrio ++;
}

void IRAM_ATTR pulsosCaliente() {
  contadorCaliente ++;
}

void setup() {
  Serial.begin(115200);
  delay(100);

  //Configuracion de los pines
  pinMode(SENSOR_FRIO, INPUT_PULLUP);
  pinMode(SENSOR_CALIENTE, INPUT_PULLUP);

  //Activacion de interrupciones de los sensores
  attachInterrupt(digitalPinToInterrupt(SENSOR_FRIO), pulsosFrio, FALLING);
  attachInterrupt(digitalPinToInterrupt(SENSOR_CALIENTE), pulsosCaliente, FALLING);

  // Conexion WiFi 
  
  WiFi.disconect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);

  esp_eth_config_t config = WPA_CONFIG_INIT_DEFAULT();
  esp_wifi_sta_wpa2_ent_enable(&config);
  esp_wifi_sta_wpa2_ent_set_identity((uitn8_t*)WIFI_USERNAME, strlen(WIFI_USERNAME));
  esp_wifi_sta_wpa2_ent_set_username((uitn8_t*)WIFI_USERNAME, strlen(WIFI_USERNAME));
  esp_wifi_sta_wpa2_ent_set_password((uitn8_t*)WIFI_PASS, strlen(WIFI_PASS));

  // WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.begin(WIFI_SSID);
  Serial.print("Conectando WiFi" + WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
     delay(1000);
     Serial.print(".");
  }
  Serial.println(" conectado correctamente");


  //Inicialización del lector de microSD
  Serial.println("Inicializando microSD ");
  if (!SD.begin(SENSOR_SD)) {
    Serial.println("✘ Fallo al inicializar microSD");
  } else {
    Serial.println("✔ Tarjeta microSD lista");
  }
}

void loop() {

  // Conexion con MQTT si no está hecha
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.processPackets(10);
  mqtt.ping();

  // -- SENSOR DEL CAUDAL FRIO --
  if (millis() - lastTimeFrio > 1000) {
    detachInterrupt(digitalPinToInterrupt(SENSOR_FRIO));

    float flowFrio = ((1000.0 * contadorFrio) / (millis() - lastTimeFrio)) / calibrationFactor;
    lastTimeFrio = millis();
    contadorFrio = 0;

    //Printeo por terminal del caudal para probar que se recibe del sensor
    Serial.print("Caudal agua frío registrado: ");
    Serial.print(flowFrio);
    Serial.println(" L/h");

    //Printeo de aviso para ver si se publica correctamente o no
    if (!caudalFrio.publish(flowFrio)) {
      Serial.println("✘ Error enviando caudal frio a Adafruit IO");
    } else {
      Serial.println("✔ Caudal frio enviado a Adafruit IO");
    }

    // --- Almacenar caudal FRÍO en SD ---
    logFile = SD.open("/caudal.csv", FILE_APPEND);
    if (logFile) {
      logFile.print("FRIO,");
      logFile.print(millis());
      logFile.print(",");
      logFile.println(flowFrio);
      logFile.close();
      Serial.println("✔ Caudal FRÍO guardado en SD");
    } else {
      Serial.println("✘ Error al escribir en SD");
    }

    attachInterrupt(digitalPinToInterrupt(SENSOR_FRIO), pulsosFrio, FALLING);
  }


  // -- SENSOR DEL CAUDAL CALIENTE --
  if (millis() - lastTimeCaliente > 1000) {
    detachInterrupt(digitalPinToInterrupt(SENSOR_CALIENTE));

    float flowCaliente = ((1000.0 * contadorCaliente) / (millis() - lastTimeCaliente)) / calibrationFactor;
    lastTimeCaliente = millis();
    contadorCaliente = 0;

    //Printeo por terminal del caudal para probar que se recibe del sensor
    Serial.print("Caudal agua caliente registrado: ");
    Serial.print(flowCaliente);
    Serial.println(" L/h");

    //Printeo de aviso para ver si se publica correctamente o no
    if (!caudalCaliente.publish(flowCaliente)) {
      Serial.println("✘ Error enviando caudal caliente a Adafruit IO");
    } else {
      Serial.println("✔ Caudal caliente enviado a Adafruit IO");
    }

    // --- Almacenar caudal CALIENTE en SD ---
    logFile = SD.open("/caudal.csv", FILE_APPEND);
    if (logFile) {
      logFile.print("CALIENTE,");
      logFile.print(millis());
      logFile.print(",");
      logFile.println(flowCaliente);
      logFile.close();
      Serial.println("✔ Caudal CALIENTE guardado en SD");
    } else {
      Serial.println("✘ Error al escribir en SD");
    }

    attachInterrupt(digitalPinToInterrupt(SENSOR_CALIENTE), pulsosCaliente, FALLING);
  }

}

void connectMQTT() {
  int8_t ret;
  Serial.print("Conectando a MQTT...");
  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    delay(2000);
  }
  Serial.println("conectado");
}
