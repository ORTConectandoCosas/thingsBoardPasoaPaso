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
char requestTopic[] = "v1/devices/me/rpc/request/+";  //RPC - El Servidor usa este topico para enviar rquests, cliente response
char attributesTopic[] = "v1/devices/me/attributes";  // Permite recibir o enviar mensajes dependindo de atributos compartidos

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
   
  lastSend = 0; // para controlar cada cuanto tiempo se envian datos
}

// función loop micro
void loop()
{
  if ( !client.connected() ) {
    reconnect();
  }

  if ( millis() - lastSend > elapsedTime ) { // Update and send only after 1 seconds
    // Enviar datos de telemetria
    getAndSendData();
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

/* 
 *  Este callback se llaman cuando se utilizan widgets de control que envian mensajes por el topico requestTopic
 *  en la función de reconnect se realiza la suscripción al topico de request
 */
void on_message(const char* topic, byte* payload, unsigned int length) 
{
  // Mostrar datos recibidos del servidor
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

  // Obtener el nombre del método invocado, esto lo envia el switch de la puerta y el knob del motor que están en el dashboard
  String methodName = String((const char*)data["method"]);
  Serial.print("Nombre metodo:");
  Serial.println(methodName);

  //responder segun el método 
  if (methodName.equals("openDoor")) {
    bool action = data["params"];
    String doorStatus = openDoor(action);

    // responder al servidor con el estado de la puerta
    replyDoorRequest(doorStatus, topic);
    

  }
  else if (methodName.equals("rotateMotorValue")) {
    String gradosTemp = (data["params"]);
    int grados = gradosTemp.toInt();

    // se llama al motor para que gire los grados del parametro
    moverMotor(grados);
    replyMotorRequest(grados, topic);
  }
 
}

/*
 * función que "abre" la puerta (simulada)
 */
String openDoor(bool action)
{
  String returnValue = "NO ANDA";
  if (action == true) {
    Serial.println("Abriendo puerta");
    returnValue = "ABIERTA";
  }
   else {
    Serial.println("Cerrando puerta");
    returnValue = "CERRADA";
   }
   return returnValue;
}

/*
 * 
 *Reply al servidro del estado de la puerta. se modifica el atributo compartido doorState y se refleja en la card asociada al misimo
 *
 */
void replyDoorRequest(String doorStatus, const char* topic)
{
    // cambiar el topico de RPC a RESPONSE
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");  //Notar que se cambio la palabra request por response en la cadena del topico
    Serial.println(responseTopic);
    
     // Prepare a JSON payload string dicendo el estado de la puerta, Notar que la tarjeta del dashboar tiene este atributo definido
     String payload = "{";
     payload += "\"doorState\":"; payload += "\""  ; payload += doorStatus; payload += "\""; payload += "}";

    // Send payload
    char attributes[100];
    payload.toCharArray( attributes, 100 );
    Serial.print("respuesta puerta: ");
    Serial.println(attributes);

    // se envia la repsuesta la cual se despliegan en las tarjetas creadas para el atrubito 
    client.publish(attributesTopic, attributes);
}

/*
 * función que gira un servo X grados (simulada)
 */
void moverMotor(int grados)
{
  Serial.print("moviendo motor");
  Serial.println(grados);
}

/*
 * 
 *Reply al servidor. se modifica el atributo compartido gradosMotor y se refleja en la card asociada al misimo
 *
 */
void replyMotorRequest(int grados, const char* topic)
{
    // cambiar el topico de RPC a RESPONSE
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");  //Notar que se cambio la palabra request por response en la cadena del topico
    Serial.println(responseTopic);
    
     // Prepare a JSON payload string dicendo el estado de la puerta, Notar que la tarjeta del dashboar tiene este atributo definido
     String payload = "{";
     payload += "\"gradosMotor\":";   ; payload += grados; ; payload += "}";

    // Send payload
    char attributes[100];
    payload.toCharArray( attributes, 100 );
    Serial.print("respuesta motor: ");
    Serial.println(attributes);

    // se envia la repsuesta la cual se despliegan en las tarjetas creadas para el atrubito 
    client.publish(attributesTopic, attributes);
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



