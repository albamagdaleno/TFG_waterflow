#include "WiFi.h"
//#include "esp_wpa2.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>


//WiFi Credentials
//String WIFI_SSID = "Livebox6-D220";
//String WIFI_PASS = "X3Er3wZg94Ti";
String Wifi_ssid = "MOVISTAR_PRuEBA_29D0";
String Wifi_pass = "RAFQpUxikyafVCKyxQrt";
String Wifi_username = "";

// WiFi Credentials Unileon
//String WIFI_SSID = "unileon";
//String WIFI_USERNAME = "amagdr00";
//String WIFI_PASS = ""; 

// Adafruit IO
String Aio_server = "io.adafruit.com" ;
int Aio_serverport = 1883 ;
String Aio_username = "amagdr00" ;
String Aio_key = "ai";
String Feed_caudal_frio= "amagdr00/feeds/caudal_frio";
String Feed_caudal_caliente = "amagdr00/feeds/caudal_caliente";

// Objeto WiFi
WiFiClient client;
bool conectadoWifi = 0;

// Cliente MQTT
Adafruit_MQTT_Client *mqtt = nullptr;
bool conectadoMQTT = 0;

 // Feeds de los caudales
 Adafruit_MQTT_Publish *caudalFrio = nullptr;
 Adafruit_MQTT_Publish *caudalCaliente = nullptr;

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
/////////////////////////////////////

//Contador de pulsos calientes
void IRAM_ATTR pulsosCaliente() {
 contadorCaliente ++;
}
/////////////////////////////////////

//Leer configuración desde archivo
String leerValorConfig(const char* nombreArchivo, const char* clave) {
 String valorRetorno="";
 Serial.println("Entrando en leerValorConfig para buscar la clave " + String(clave));
 File archivo = SD.open(nombreArchivo, FILE_READ);
 if (!archivo) {
    Serial.print("No se pudo abrir el archivo: ");
    Serial.println(nombreArchivo);
    return valorRetorno;
 }

 String linea;

 while (archivo.available()) {
    linea = archivo.readStringUntil('\n');
    linea.trim(); // elimina espacios y saltos de línea
    Serial.println ("Leída línea " + linea);
    // Ignorar líneas vacías o comentarios
    if (linea.length() == 0 || linea.startsWith("#") || linea.startsWith(";")) {
        continue;
    }

    int separador = linea.indexOf('=');
    if (separador > 0) {
        String claveArchivo = linea.substring(0, separador);
        String valorArchivo = linea.substring(separador + 1);
        claveArchivo.trim();
        valorArchivo.trim();
        Serial.println ("Clave=" + claveArchivo);
        Serial.println ("Valor=" + valorArchivo);
        if (claveArchivo.equalsIgnoreCase(String(clave))) {
            valorRetorno = valorArchivo; // asigna el nuevo valor
            Serial.println("Encontrada clave=" + String(clave) + " Asignado valor="+ valorArchivo);
            break; // termina la búsqueda
        }
    }
 }

 archivo.close();
 return valorRetorno;
}//fin de leerValorConfig
/////////////////////////////////////

//Conexión Wifi
bool connectWiFi(uint8_t reintentosMax = 10){
 if (Wifi_ssid=""){
    leerConfiguracion();
 }
 WiFi.mode(WIFI_STA);
 WiFi.begin(Wifi_ssid.c_str(),Wifi_pass.c_str());
 Serial.println("Conectando a WiFi " + String(Wifi_ssid));
 

 uint8_t intentos = 0;
 while (WiFi.status() != WL_CONNECTED && intentos < reintentosMax) {
 delay(1000);
 Serial.print(".");
 intentos++;
 }

 if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado correctamente");
    Serial.println("Dirección IP: " + WiFi.localIP());
    return true; 
 } else {
    Serial.println("No se pudo conectar a la red WiFi");
    WiFi.disconnect(true);
    return false; 
 }
 
 //esp_eth_config_t config = WPA_CONFIG_INIT_DEFAULT();
 //esp_wifi_sta_wpa2_ent_enable(&config);
 //esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)WIFI_USERNAME, strlen(WIFI_USERNAME));
 //esp_wifi_sta_wpa2_ent_set_username((uint8_t*)WIFI_USERNAME, strlen(WIFI_USERNAME));
 //esp_wifi_sta_wpa2_ent_set_password((uint8_t*)WIFI_PASS, strlen(WIFI_PASS));

}//fin de connect WiFi
/////////////////////////////////////

//Inicializar conexión MQTT
bool inicializarMQTT() {
 if (mqtt != nullptr) {
    delete mqtt; // Limpia si ya existía
 }
 Serial.println("Server="+String(Aio_server));
 Serial.println("Puerto="+String(Aio_serverport));
 Serial.println("Username="+String(Aio_username));
 Serial.println("Key="+String(Aio_key));
 mqtt = new Adafruit_MQTT_Client(
 &client,
 Aio_server.c_str(),
 (uint16_t)Aio_serverport,
 Aio_username.c_str(),
 Aio_key.c_str()
 );

 if (!mqtt) {
    Serial.println(" Error al crear el objeto MQTT");
    return false;
 }

 Serial.println("Objeto MQTT creado correctamente");
 return true;
}//fin de Inicializar conexión MQTT
/////////////////////////////////////

//Conexión de MQTT
bool connectMQTT() {
 int8_t ret;
 
 intentoConexiones=0;
 Serial.println("Conectando a MQTT...");
 while ((ret = mqtt->connect()) != 0 && intentoConexiones < 5) {
 Serial.println(mqtt->connectErrorString(ret));
 mqtt->disconnect();
 delay(5000);
 intentoConexiones+= 1;
 grabarLog("Error n " + String(intentoConexiones) + " de conexion MQTT");
 }

 if(ret == 0){
 Serial.println("conectado");
 return true;
 }else{
 inicializarMQTT();
 return false;
 }
}//fin de connectMQTT
/////////////////////////////////////

//Formateo hora
String printFormatoHora (int valor){
 if (valor<10){
 return "0" + String(valor);
 }
 return String(valor);
}//fin de printFormatoHora
/////////////////////////////////////

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
/////////////////////////////////////

//Grabar log en archivo
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
/////////////////////////////////////


//leer Configuración

void leerConfiguracion(){

 Wifi_ssid=leerValorConfig("/config.txt", "Wifi_ssid");
 Serial.println ("Wifi_ssid= "+Wifi_ssid);
 Wifi_username=leerValorConfig("/config.txt", "Wifi_username");
 Wifi_pass=leerValorConfig("/config.txt", "Wifi_pass");
 Serial.println ("Wifi_pass= "+Wifi_pass);

 Aio_username=leerValorConfig("/config.txt", "Aio_username");
 Aio_key=leerValorConfig("/config.txt", "Aio_key");
 Aio_server=leerValorConfig("/config.txt", "Aio_server");
 Aio_serverport=leerValorConfig("/config.txt", "Aio_serverport").toInt();
 Feed_caudal_frio=leerValorConfig("/config.txt", "Feed_caudal_frio");
 Feed_caudal_caliente=leerValorConfig("/config.txt", "Feed_caudal_caliente");

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

 //Leer archivo de configuración
 leerConfiguracion();
 
 
 // Cliente MQTT
 if (!inicializarMQTT()){
 return;
 }
 //mqtt = new Adafruit_MQTT_Client(&client, Aio_server.c_str(), (uint16_t)Aio_serverport, Aio_username.c_str(), Aio_key.c_str());
 
 // Feeds de los caudales
 caudalFrio = new Adafruit_MQTT_Publish(mqtt, Feed_caudal_frio.c_str());
 caudalCaliente = new Adafruit_MQTT_Publish(mqtt, Feed_caudal_caliente.c_str());
 
 //Conexión a la red WifI
 conectadoWifi = connectWiFi(5);

 //Conexión con MQTT
 if (conectadoWifi){
 Serial.println("Conectado Wifi");
 conectadoMQTT = connectMQTT();
 }
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
 rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Establece la hora de compilación
 }
 return ;

}

void loop() {

 // Conexion con la red WiFi si no está hecha
 if(!conectadoWifi){
 conectadoWifi = connectWiFi(5);
 }

 // Conexion con MQTT si no está hecha
 if (!conectadoMQTT && conectadoWifi) {
 Serial.println("Conectado Wifi");
 conectadoMQTT = connectMQTT();
 mqtt->processPackets(10);
 mqtt->ping();
 }

 flowFrio = 0;
 flowCaliente = 0;

 // -- SENSOR DEL CAUDAL FRIO --
 if (millis() - lastTimeFrio > 1000) {
 detachInterrupt(digitalPinToInterrupt(SENSOR_FRIO));

 flowFrio = ((1000.0 * contadorFrio) / (millis() - lastTimeFrio)) / calibrationFactor;
 lastTimeFrio = millis();
 contadorFrio = 0;

 

 //Printeo de aviso para ver si se publica correctamente o no
 if (conectadoMQTT && conectadoWifi && flowFrio > 0) {
 Serial.print("Caudal agua frío registrado: "+ String(flowFrio)+ " L/h" );
 if (!caudalFrio->publish(flowFrio)) {
 Serial.println("✘ Error enviando caudal frio a Adafruit IO");
 conectadoMQTT = connectMQTT();
 } else {
 Serial.println("✔ Caudal frio enviado a Adafruit IO");
 }
 }
 attachInterrupt(digitalPinToInterrupt(SENSOR_FRIO), pulsosFrio, FALLING);
 }


 // -- SENSOR DEL CAUDAL CALIENTE --
 if (millis() - lastTimeCaliente > 1000) {
 detachInterrupt(digitalPinToInterrupt(SENSOR_CALIENTE));

 flowCaliente = ((1000.0 * contadorCaliente) / (millis() - lastTimeCaliente)) / calibrationFactor;
 lastTimeCaliente = millis();
 contadorCaliente = 0;

 

 //Printeo de aviso para ver si se publica correctamente o no
 if (conectadoMQTT && conectadoWifi && flowCaliente > 0) {
 Serial.println("Caudal agua caliente registrado: " + String(flowCaliente)+ " L/h");
 if (!caudalCaliente->publish(flowCaliente)) {
 Serial.println("✘ Error enviando caudal caliente a Adafruit IO");
 conectadoMQTT = connectMQTT();
 } else {
 Serial.println("✔ Caudal caliente enviado a Adafruit IO");
 }
 }
 attachInterrupt(digitalPinToInterrupt(SENSOR_CALIENTE), pulsosCaliente, FALLING);
 }
 
 if (flowFrio > 0 || flowCaliente > 0 ){

 Serial.print("Grabando caudal en SD..............");
 grabarCaudal(flowFrio,flowCaliente);
 
 }

}