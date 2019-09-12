/* 
 *  Code for running a more sophisticated webserver on an Arduino Uno
 *  
 *  created 11 Sep 2019 
 *  by Tristan Armitage
 */

#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>

#define debugging true
//boolean debugging=true;

String request_method;
String request_uri;

byte mac[]     = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte subnet[]  = { 255, 255, 255, 0 };
byte gateway[] = { 192, 168, 0, 1 };
IPAddress ip(192, 168, 0, 2);
EthernetServer server(80);


/* Functions for setting up the hardware peripherals, i.e. Ethernet and SD */
void setupEthernet() {
  Ethernet.begin(mac, ip, gateway, subnet);

  if(Ethernet.hardwareStatus() == EthernetNoHardware) {
    if(debugging){ Serial.println("No ethernet shield"); } //Send status if in debug mode
    while(true){ delay(1); } //lock in an infinite loop
  }
  if(Ethernet.linkStatus() == LinkOFF) {
    if(debugging){ Serial.println("No ethernet cable connected"); }
  }
}
void setupSD() {
  if(!SD.begin(4)) {
    if(debugging){ Serial.println("Could not initialize SD card"); }
    while(true){ delay(1); }
  }
}
void setupServer() {
  server.begin();
  if(debugging){ 
    Serial.println("Arduino Server started");
    Serial.print("Server is at ");
    Serial.println(Ethernet.localIP());
  }
}


/* Normal setup function. Calls the specific setup functions implemented above */
void setup() {
  if(debugging){ Serial.begin(9600); }
  setupEthernet();
  setupSD();
  setupServer();
  
}

/* Normal loop function.
 * NOTE: You can pass the client to other functions (not pointer notation needed). */
void loop() {
  // Reset the request Strings
  request_method = "";
  request_uri = "";
  
  // Check for an available client
  EthernetClient client = server.available();
  if(client) {
    if(debugging){ Serial.println("---New Client---"); }
    boolean currentLineIsBlank = true;
    
    /* Read the HTTP request, which should be the first line coming from the client
     * NOTE: A typical URI request should work fine, including folders and the initial '/' character */
    request_method = client.readStringUntil(' ');
    request_uri = client.readStringUntil(' ');
    if(request_uri.endsWith("/")){ request_uri.concat("index.htm"); } //Reference the index.htm file when no file is specified
    if(debugging){
      Serial.println(request_method);
      Serial.println(request_uri);
    }
    
    while(client.connected()) {
      if(client.available()) {
        char c = client.read();
        Serial.write(c);
        if(c=='\n'){ 
          if(currentLineIsBlank){ 
                 if(request_method=="GET"){ processGetRequest(client,request_uri); }
            //else if(request_method=="PUT"){ processPutRequest(client,request_uri); }
            else { client.println("HTTP/1.1 400 Bad Request\n"); }
            break;
          } 
          else{ currentLineIsBlank = true; }
        }
        else if(c!='\r'){ currentLineIsBlank = false; }
        
      }//end if(client.available())
    }//end while(client.connected())
    
    delay(1); // Give the web browser time to receive the data
    client.stop(); // Close the connection  
    if(debugging){ Serial.println("---Client Closed---"); }  
  }//end if(client)
}

void processGetRequest(EthernetClient client, String uri) {
  File file = SD.open(uri);
  if(file) {
    client.println("HTTP/1.1 200 OK");
    client.print  ("Content-Type: ");
    client.println(parseContentType(uri));
    client.println("Connection: close");
    client.println();
    
    while(file.available()) {
      char c = file.read();
      client.write(c);
      if(debugging){ 
        if(c=='\n'){ Serial.println(); }
        else { Serial.write(c); }
      }
    }//end while(file.available())
  } else { client.println("HTTP/1.1 404 Not Found\n"); }
}

String parseContentType(String uri) {
  String ext = uri.substring(uri.lastIndexOf('.'));
  // Return the appropriate content-type
       if(ext==".htm"){ return "text/html"; }
  else if(ext==".css"){ return "text/css"; }
  else { return "*/*"; }
}
