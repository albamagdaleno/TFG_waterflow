
#include "WiFi.h"
#include "esp_wpa2.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>


//WiFi Credentials
//const char* WIFI_SSID = "Livebox6-D220";
//const char* WIFI_PASS = "X3Er3wZg94Ti";
const char* WIFI_SSID = "MOVISTAR_29D0";
const char* WIFI_PASS = "xxxxxxxxx";

// WiFi Credentials Unileon
//const char* WIFI_SSID = "unileon";
//const char* WIFI_USERNAME = "amagdr00";
//const char* WIFI_PASS = "inTvahB08ca"; 

// Adafruit IO
const char* AIO_SERVER     = "io.adafruit.com" ;
const int AIO_SERVERPORT = 1883 ;
const char* AIO_USERNAME  = "amagdr00" ;
const char* AIO_KEY        = "xxxxxxxx";
const char* FEED_CAUDAL_FRIO = "amagdr00/feeds/caudal_frio";
const char* FEED_CAUDAL_CALIENTE = "amagdr00/feeds/caudal_caliente";

// Objeto WiFi
WiFiClient client;
bool conectadoWifi = 0;

// Cliente MQTT
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
bool conectadoMQTT = 0;

// Feeds de los caudales
Adafruit_MQTT_Publish caudalFrio = Adafruit_MQTT_Publish(&mqtt, FEED_CAUDAL_FRIO);
Adafruit_MQTT_Publish caudalCaliente = Adafruit_MQTT_Publish(&mqtt, FEED_CAUDAL_CALIENTE);

// Sensor de caudal frio
const int SENSOR_FRIO = 15;
volatile int contadorFrio = 0;
unsigned long lastTimeFrio = 0;
float flowFrio = 0;

// Sensor de caudal caliente
const int SENSOR_CALIENTE = 27;
volatile int contadorCaliente = 0;
unsigned long lastTimeCaliente = 0;
float flowCaliente = 0;

//Factor de calibracion del sensor YF-S201C
float calibrationFactor = 4.5;

//Adaptador microSD y archivo
const int SENSOR_SD = 5; 
File logFile;

//Módulo reloj DS3231
RTC_DS3231 rtc;
DateTime now;

//Contador intento de Conexiones
int intentoConexiones = 0;


//Contador de pulsos fríos
void IRAM_ATTR pulsosFrio() {
  contadorFrio ++;
}

//Contador de pulsos calientes
void IRAM_ATTR pulsosCaliente() {
  contadorCaliente ++;
}

//Conexión Wifi
bool connectWiFi(){

  intentoConexiones = 0;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("Conectando a la red WiFi...");
  while(WiFi.status() != WL_CONNECTED && intentoConexiones < 5){

    WiFi.disconnect(true);
    delay(5000);
    intentoConexiones++;
    grabarLog("Error n " + intentoConexiones + " de conexion a la red WiFi")
  }

  if(WiFi.status() == WL_CONNECTED){
    Serial.println("conectado");
    return true;
  }else{
    return false;
  }
  
  //esp_eth_config_t config = WPA_CONFIG_INIT_DEFAULT();
  //esp_wifi_sta_wpa2_ent_enable(&config);
  //esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)WIFI_USERNAME, strlen(WIFI_USERNAME));
  //esp_wifi_sta_wpa2_ent_set_username((uint8_t*)WIFI_USERNAME, strlen(WIFI_USERNAME));
  //esp_wifi_sta_wpa2_ent_set_password((uint8_t*)WIFI_PASS, strlen(WIFI_PASS));

}//fin de connect WiFi

//Conexión de MQTT
bool connectMQTT() {
  int8_t ret;
  
  intentoConexiones=0;
  Serial.println("Conectando a MQTT...");
  while ((ret = mqtt.connect()) != 0 && intentoConexiones < 5) {
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    delay(5000);
    intentoConexiones+= 1;
    grabarLog("Error n " + intentoConexiones + " de conexion MQTT");
  }

  if(ret == 0){
    Serial.println("conectado");
    return true;
  }else{
    return false;
  }

}//fin de connectMQTT

String printFormatoHora (int valor){
  if (valor<10){
    return "0" + String(valor);
  }
  return String(valor);
}
//Grabar caudal en SD
void grabarCaudal(float caudalFrio, float caudalCaliente) {
  if (rtc.begin()){
    now = rtc.now();

    // Crear nombre de archivo: YYYY-MM-DD.csv
    String filename = "/"+ String(now.year()) + "-" + String(now.month()) + "-" + String(now.day()) + ".csv";
    Serial.println(filename);
      // Crear archivo si no existe, con cabecera
      if (!SD.exists(filename)) {
        File file = SD.open(filename, FILE_WRITE);
        if (file) {
          file.println("Hora,Caudal frio,Caudal caliente");
          file.close();
        } else {
          Serial.println("No se pudo crear el archivo.");
          return; // Salir si no se puede crear
        }
    }
    // Formatear hora actual: HH:MM:SS
    String hora = printFormatoHora(now.hour()) + ":" + printFormatoHora(now.minute()) + ":" + printFormatoHora(now.second());
    Serial.println (hora);
    // Escribir los datos
    File file = SD.open(filename, FILE_APPEND);
    if (file) {
      file.print(hora);
      file.print(",");
      file.print(caudalFrio);
      file.print(",");
      file.println(caudalCaliente);
      file.close();
    } else {
      Serial.println("Error al abrir el archivo para escribir.");
    }
  } else{
    grabarLog("Error RTC/SD");
  }
} //fin de grabarCaudal


void grabarLog(String cadena){
  logFile = SD.open("/Errores.log", FILE_APPEND);
    if (rtc.begin()){
      now = rtc.now();
      if (logFile) {
        logFile.print(String(now.day()) + "-" + String(now.month()) + "-" + String(now.year()) + " " + printFormatoHora(now.hour()) + ":" + printFormatoHora(now.minute()) + ":" + printFormatoHora(now.second())+ " - "+ cadena);
        logFile.close();
      }else{
        if (logFile) {
        logFile.print(cadena);
        logFile.close();
        }
      }
    }  
} // fin de grabarLog

void setup() {
  Serial.begin(115200);
  delay(100);

  //Configuracion de los pines
  pinMode(SENSOR_FRIO, INPUT_PULLUP);
  pinMode(SENSOR_CALIENTE, INPUT_PULLUP);

  //Activacion de interrupciones de los sensores
  attachInterrupt(digitalPinToInterrupt(SENSOR_FRIO), pulsosFrio, FALLING);
  attachInterrupt(digitalPinToInterrupt(SENSOR_CALIENTE), pulsosCaliente, FALLING);

  //Conexión a la red WifI
  conexionWifi = connectWiFi();

  //Conexión con MQTT
  conectadoMQTT = connectMQTT();

  //Inicialización del lector de microSD
  Serial.println("Inicializando microSD ");
  if (!SD.begin(SENSOR_SD)) {
    Serial.println("✘ Fallo al inicializar microSD");
  } else {
    Serial.println("✔ Tarjeta microSD lista");
  }

  //Inicialización reloj
   // Inicializar RTC
  if (!rtc.begin()) {
    Serial.println("No se encontró el módulo RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC perdió la hora, configurando...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Establece la hora de compilación
  }

}

void loop() {

  // Conexion con la red WiFi si no está hecha
  if(!conexionWifi){
    conexionWifi = connectWiFi();
  }

  // Conexion con MQTT si no está hecha
  if (!conectadoMQTT && conexionWiFi) {
    connectadoMQTT = connectMQTT();
    mqtt.processPackets(10);
    mqtt.ping();
  }

  flowFrio = 0;
  flowCaliente = 0;

  // -- SENSOR DEL CAUDAL FRIO --
  if (millis() - lastTimeFrio > 1000) {
    detachInterrupt(digitalPinToInterrupt(SENSOR_FRIO));

    flowFrio = ((1000.0 * contadorFrio) / (millis() - lastTimeFrio)) / calibrationFactor;
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

    attachInterrupt(digitalPinToInterrupt(SENSOR_FRIO), pulsosFrio, FALLING);
  }


  // -- SENSOR DEL CAUDAL CALIENTE --
  if (millis() - lastTimeCaliente > 1000) {
    detachInterrupt(digitalPinToInterrupt(SENSOR_CALIENTE));

    flowCaliente = ((1000.0 * contadorCaliente) / (millis() - lastTimeCaliente)) / calibrationFactor;
    lastTimeCaliente = millis();
    contadorCaliente = 0;

    //Printeo por terminal del caudal para probar que se recibe del sensor
    Serial.println("Caudal agua caliente registrado: ");
    Serial.println(flowCaliente);
    Serial.println(" L/h");

    //Printeo de aviso para ver si se publica correctamente o no
    if (!caudalCaliente.publish(flowCaliente)) {
      Serial.println("✘ Error enviando caudal caliente a Adafruit IO");
    } else {
      Serial.println("✔ Caudal caliente enviado a Adafruit IO");
    }

    attachInterrupt(digitalPinToInterrupt(SENSOR_CALIENTE), pulsosCaliente, FALLING);
  }
 
  if (flowFrio > 0 || flowCaliente > 0 ){

    Serial.print("Grabando caudal en SD..............");
    grabarCaudal(flowFrio,flowCaliente);
 
  }

}

