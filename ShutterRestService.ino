//----------------------------------------------------------------------------------------
//File: PhilipsHueApi.ino
//
//Autor: Tobias Rothlin
//Versions:
//          1.0  Inital Version
//----------------------------------------------------------------------------------------


//Include ESP 8266 library
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

//Include humidity & temprature sensor library
#include <DHTesp.h>



//Buffer size for request url
#define REQ_BUF_SZ   50

//String to save the request url
char HTTP_req[REQ_BUF_SZ] = {0};
char req_index = 0;

//Sensor object
DHTesp dht;

//Local Wifi parameters
char* ssid     = "wifiName";
char* password = "wifiPassword";

//Network parameters
IPAddress staticIP(192, 168, 1, 35); //ESP 8266 static Ip Adress
IPAddress gateway(192, 168, 1, 1); //Default gateway -> standard 192, 168, 1, 1
IPAddress subnet(255, 255, 255, 0); 
IPAddress dns(192, 168, 1, 1); //Default dns Server Ip -> standard 192, 168, 1, 1
WiFiServer server(80); // Port number

//Enum to define the current postiton of the shutter 
enum ShutterState {
  Up,
  Down,
};

//Var to save the position of the shutter
enum ShutterState ShutterPosition;

//Defining the Ports on the ESP 8266
const int DownRelays = 4; //D2
const int UpRelays = 5; //D1
const int StopRelays = 12; //D6

const int ReserveRelays = 17; //NotConnected;
const int ConnectedLEd = 14; //D5

const int dhtPin = 13; //D7


//Duration the Relays are switch on
const int delayForRelays = 4000;

//Inital setup
void setup() {
  //Serial communication @Bauderate 115200
  Serial.begin(115200);

  //Initalise the temp/humid sensor
  dht.setup(dhtPin, DHTesp::DHT11);

  //Define the IO ports as outputs
  pinMode(DownRelays, OUTPUT);
  pinMode(UpRelays, OUTPUT);
  pinMode(StopRelays, OUTPUT);
  pinMode(ReserveRelays, OUTPUT);
  pinMode(ConnectedLEd, OUTPUT);

  //Set the output ports to LOW
  digitalWrite(DownRelays, LOW);
  digitalWrite(UpRelays, LOW);
  digitalWrite(StopRelays, LOW);
  digitalWrite(ReserveRelays, LOW);
  digitalWrite(ConnectedLEd, LOW);

  //Connect to the Wifi and print out the status via the serial output
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.config(staticIP, subnet, gateway, dns);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { //Waits until the ESP 8266 has connected to the Wifi
    delay(500);
    Serial.print(".");
  }
  
  //Prints out the IP address should be the same as specified in line 35
  //if not the WiFi.coinfg(....) in line 90 could be wrong. Depending on the version of the 
  //lib and verison of the esp8266 the parameter subnet & staticIP are switched.
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(ConnectedLEd, HIGH); //Sets the status LED to HIGH to know that the ESP 8266 is ready for use

  server.begin(); //Enabels the server
  ShutterPosition = Up; //Deflaut the shutter position is up!
}

void loop() {
  WiFiClient client = server.available();   

  if (client) { //Polling to check if a new request has been sent
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {  
        char c = client.read();  //Gets the next Character in the request string
      
        if (req_index < (REQ_BUF_SZ - 1)) { //Appends the Character to the HTTP_req string
          HTTP_req[req_index] = c;          
          req_index++;
        }
        Serial.print(c);  //Used for debuging the input string  

        
        if (c == '\n' && currentLineIsBlank) { //Enters this if statement when at the end of the request string

          //Parsing the request
          String httpRequest = HTTP_req;
          Serial.println("HTTP_req:" + httpRequest + ":");
          int startPos = httpRequest.indexOf(" /");
          httpRequest = httpRequest.substring(startPos + 1);
          int endPos =  httpRequest.indexOf(" HTTP");
          httpRequest = httpRequest.substring(0, endPos);
          httpRequest.trim();

          //Output the request string
          Serial.println("Request:" + httpRequest + ":");

          //Get the function name as Stirng
          int CGIPos =  httpRequest.indexOf("cgi-local");
          String functionName = httpRequest.substring(CGIPos + 10);

          //Output the function name as String 
          Serial.println("FunctionName:" + functionName + ":");

          //Compares the functionName to the known functions 
          if (functionName == "get-sensor-values") {
            //gets the values from the sonsor
            TempAndHumidity newValues = dht.getTempAndHumidity();

            //Sends the response header back as indicates that the response will be a json struct
            client.println("HTTP/1.1 200 OK");           
            client.println("Access-Control-Allow-Origin: *");
            client.println("Content-type: application/json");
            client.println();                           

            //Sends the json struct back with Temp/ Hum/ State
            client.print("{\"id\": ");
            client.print(1);                           
            client.print(", \"Temp\": \"");
            client.print(newValues.temperature);
            client.print("\", \"Hum\": \"");
            client.print(newValues.humidity);
            client.print("\", \"State\": \"");
            if(ShutterPosition == Up){
               client.print("UP");
            }
            else if (ShutterPosition == Down){
               client.print("Down");
            }
           
            client.print("\"}");                    

          }
          //if the functionName is not to get the sensor values it must be a shutter operation
          else {
            //Pasring the specific shutter operation
            int shuttersPos =  functionName.indexOf("shutters");
            String shutterOperation = functionName.substring(shuttersPos + 9);
            //Outputs the shutterOperation to perform
            Serial.println("shutterOperation:" + shutterOperation + ":");

            //Three shutterOperations -> Up,Down,Stop
            if (shutterOperation == "Up") {
              //Sets the Relays to HIGH -> conection waits the specified lenght in line 62 -> sets Relays to LOW
              digitalWrite(UpRelays, HIGH);
              delay(delayForRelays);
              digitalWrite(UpRelays, LOW);
              //Set the shutter pistiton to Up
              ShutterPosition = Up
            }

            if (shutterOperation == "Down") {
              digitalWrite(DownRelays, HIGH);
              delay(delayForRelays);
              digitalWrite(DownRelays, LOW);
              ShutterPosition = Down;
            }

            if (shutterOperation == "Stop") {
              digitalWrite(StopRelays, HIGH);
              delay(delayForRelays);
              digitalWrite(StopRelays, LOW);
            }

            //Sends the response header back as indicates that the response will be a json struct 
            client.println("HTTP/1.1 200 OK");          
            client.println("Access-Control-Allow-Origin: *");
            client.println("Content-type: application/json");
            client.println();                          

            //Sends the json struct back with the performed shutterOperation
            client.print("{\"id\": ");
            client.print(1);                          
            client.print(", \"shtterOperation\": \"");
            client.print(shutterOperation);
            client.print("\"}");                       
          }

          //Cleans up 
          req_index = 0;
          StrClear(HTTP_req, REQ_BUF_SZ);
          break;
        }
       
        if (c == '\n') {
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          currentLineIsBlank = false;
        }
      } 
    } 
    //Terminates the connection the client
    delay(1); 
    client.stop(); 
  } 

}

//Clears the char array used to store the inital request string
void StrClear(char *str, char length)
{
  for (int i = 0; i < length; i++) {
    str[i] = 0;
  }
}
