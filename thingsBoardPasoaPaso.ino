/* ejemplo para utilizar la comunicación con thingboards paso a paso con nodeMCU v2
 *  Gastón Mousqués
 *  Basado en varios ejemplos de la documentación de  https://thingsboard.io
 *  
 */

// includes de bibliotecas para comunicación
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

// includes de bibliotecas sensores
#include "DHT.h"

//  configuración datos wifi
//#define WIFI_AP "SSID RED"
//#define WIFI_PASSWORD "PASSWORD RED"




//  configuración datos thingsboard
//#define NODE_ID "NOMBRE DISPOSITIVO"
//#define NODE_TOKEN "TOKEN DISPOSITIVO"



char thingsboardServer[] = "demo.thingsboard.io";

/*definir topicos.
 * telemetry - para enviar datos de los sensores
 * request - para recibir una solicitud y enviar datos 
 * attributes - para recibir comandos en baes a atributtos shared definidos en el dispositivo
 */
char telemetryTopic[] = "v1/devices/me/telemetry";
char requestTopic[] = "v1/devices/me/rpc/request/+";
char attributesTopic[] = "v1/devices/me/attributes";

// configuración sensores
#define DHTPIN D1
#define DHTTYPE DHT11


// declarar cliente Wifi y PubSus
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// declarar variables control loop (para no usar delay() en loop
unsigned long lastSend;
const int elapsedTime = 1000; // tiempo transcurrido entre envios al servidor

// Declarar e Inicializar sensores.
DHT dht(DHTPIN, DHTTYPE);

// para el que el ejemplo funcione
#define GPIO0 0
#define GPIO2 2

#define GPIO0_PIN 3
#define GPIO2_PIN 5

// We assume that all GPIOs are LOW
boolean gpioState[] = {false, false};

// función setup micro
void setup()
{
  Serial.begin(9600);
  dht.begin();
  delay(10);

  // inicializar wifi y pubsus
  connetToWiFi();
  client.setServer( thingsboardServer, 1883 );

  // agregado para recibir callbacks
  client.setCallback(on_message);
   
  lastSend = 0;
}

// función loop micro
void loop()
{
  if ( !client.connected() ) {
    reconnect();
  }

  if ( millis() - lastSend > elapsedTime ) { // Update and send only after 1 seconds
    //getAndSendData();
    lastSend = millis();
  }

  client.loop();
}

/*
 * función para leer datos de sensores y enviar telementria al servidor
 */
void getAndSendData()
{
  Serial.println("Collecting temperature data.");

  // Reading temperature or humidity takes about 250 milliseconds!
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");

  String temperature = String(t);
  String humidity = String(h);


  // Just debug messages
  Serial.print( "Sending temperature and humidity : [" );
  Serial.print( temperature ); Serial.print( "," );
  Serial.print( humidity );
  Serial.print( "]   -> " );

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"temperature\":"; payload += temperature; payload += ",";
  payload += "\"humidity\":"; payload += humidity;
  payload += "}";

  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  if (client.publish( telemetryTopic, attributes ) == true)
    Serial.println("publicado ok");
  
  Serial.println( attributes );

}

// Agregado callback
// The callback for when a PUBLISH message is received from the server.
void on_message(const char* topic, byte* payload, unsigned int length) {

  Serial.println("On message");

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(json);

  // Decode JSON request
 
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);


  Serial.print("Nombre metodo:");
  Serial.println(methodName);
 
  if (methodName.equals("openDoor")) {
    bool action = data["params"];
    openDoor(action);
    // Reply with GPIO status
    //String responseTopic = String(topic);
    //responseTopic.replace("request", "response");
    //client.publish(responseTopic.c_str(), get_gpio_status().c_str());
  } 
  else if (methodName.equals("rotateMotorValue")) {
    String gradosTemp = (data["params"]);
    int grados = gradosTemp.toInt();
    moverMotor(grados);

    /*
    // Update GPIO status and reply
    set_gpio_status(data["params"]["pin"], data["params"]["enabled"]);
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
    client.publish("v1/devices/me/attributes", get_gpio_status().c_str());
    */
  }
 
}

void openDoor(bool action)
{
  if (action == true)
    Serial.println("Abriendo puerta");
   else
    Serial.println("Cerrando puerta");
}


void moverMotor(int grados)
{
  Serial.print("moviendo motor:);
  Serial.println(grados);
}

String get_gpio_status() {
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data[String(GPIO0_PIN)] = gpioState[0] ? true : false;
  data[String(GPIO2_PIN)] = gpioState[1] ? true : false;
  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  Serial.print("Get gpio status: ");
  Serial.println(strPayload);
  return strPayload;
}

void set_gpio_status(int pin, boolean enabled) {
  if (pin == GPIO0_PIN) {
    // Output GPIOs state
    digitalWrite(GPIO0, enabled ? HIGH : LOW);
    // Update GPIOs state
    gpioState[0] = enabled;
  } else if (pin == GPIO2_PIN) {
    // Output GPIOs state
    digitalWrite(GPIO2, enabled ? HIGH : LOW);
    // Update GPIOs state
    gpioState[1] = enabled;
  }
}



/*
 * funcion para reconectarse al servidor de thingsboard y suscribirse a los topicos de RPC y Atributos
 */
void reconnect() {
  int statusWifi = WL_IDLE_STATUS;
  // Loop until we're reconnected
  while (!client.connected()) {
    statusWifi = WiFi.status();
    connetToWiFi();
    
    Serial.print("Connecting to ThingsBoard node ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect(NODE_NAME, NODE_TOKEN, NULL) ) {
      Serial.println( "[DONE]" );
      
      // Subscribing to receive RPC requests 
      client.subscribe(requestTopic); 
      
      // Sending current GPIO status to init shared attributes
      //Serial.println("Sending current GPIO status ..."); 
      //client.publish(attributesTopic, get_gpio_status().c_str());
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}

/*
 * función para conectarse a wifi
 */
void connetToWiFi()
{
  Serial.println("Connecting to WiFi ...");
  // attempt to connect to WiFi network

  WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
}



