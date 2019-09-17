/* 
 *  Code for running a more sophisticated webserver on an Arduino Uno
 *  
 *  created 11 Sep 2019 
 *  by Tristan Armitage
 */

#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>

const boolean DEBUGGING = true;
const short MAX_METHOD_LENGTH = 10;
const short MAX_URI_LENGTH = 40;
short MethodLen;
short UriLen;

char RequestMethod[MAX_METHOD_LENGTH];
char RequestUri[MAX_URI_LENGTH];

char authorization[] = "YXJteXRhZzpwYXNzd29yZA==";

byte mac[]     = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte subnet[]  = { 255, 255, 255, 0 };
byte gateway[] = { 192, 168, 0, 1 };
IPAddress ip(192, 168, 0, 10);
EthernetServer server(80);


/* Functions for setting up the hardware peripherals, i.e. Ethernet and SD */
void setupEthernet() {
  Ethernet.begin(mac, ip, gateway, subnet);

  if(Ethernet.hardwareStatus() == EthernetNoHardware) {
    if(DEBUGGING){ Serial.println(F("No ethernet shield")); } //Send status if in debug mode
    while(true){ delay(1); } //lock in an infinite loop
  }
  if(Ethernet.linkStatus() == LinkOFF) {
    if(DEBUGGING){ Serial.println(F("No ethernet cable connected")); }
  }
}
void setupSD() {
  if(!SD.begin(4)) {
    if(DEBUGGING){ Serial.println(F("Could not initialize SD card")); }
    while(true){ delay(1); }
  }
}
void setupServer() {
  server.begin();
  if(DEBUGGING){ 
    Serial.println(F("Arduino Server started"));
    Serial.print(F("Server is at "));
    Serial.println(Ethernet.localIP());
  }
}


/* Normal setup function. Calls the specific setup functions implemented above */
void setup() {
  if(DEBUGGING){ Serial.begin(9600); }
  setupEthernet();
  setupSD();
  setupServer();
  MethodLen = MAX_METHOD_LENGTH;
  UriLen  = MAX_URI_LENGTH;
}

/* Normal loop function.
 * NOTE: You can pass the client to other functions (not pointer notation needed). */
void loop() {
  // Reset the request strings
  while(MethodLen>0){ MethodLen--; RequestMethod[MethodLen]=0; }
  while(UriLen>0) { UriLen--; RequestUri[UriLen]=0; }
  
  // Check for an available client
  EthernetClient client = server.available();
  if(client) {
    if(DEBUGGING){ Serial.println(F("---New Client---")); }
    boolean currentLineIsBlank = true;
    char c = client.read();
    /* Read the HTTP request, which should be the first line coming from the client
     * NOTE: A typical URI request should work fine, including folders and the initial '/' character */
    while(c!=' '){ RequestMethod[MethodLen]=c; MethodLen++; c=client.read(); }
    c = client.read();
    while(c!=' '){ RequestUri[UriLen]=c; UriLen++; c=client.read(); }
    if(RequestUri[UriLen-1]=='/'){ 
      RequestUri[UriLen] = 'i'; UriLen++;
      RequestUri[UriLen] = 'n'; UriLen++;
      RequestUri[UriLen] = 'd'; UriLen++;
      RequestUri[UriLen] = 'e'; UriLen++;
      RequestUri[UriLen] = 'x'; UriLen++;
      RequestUri[UriLen] = '.'; UriLen++;
      RequestUri[UriLen] = 'h'; UriLen++;
      RequestUri[UriLen] = 't'; UriLen++;
      RequestUri[UriLen] = 'm'; UriLen++;
    } 
    if(DEBUGGING){
      Serial.println(RequestMethod);
      Serial.println(RequestUri);
      Serial.println(parseContentType(RequestUri));
    }
    
    while(client.connected()) {
      if(client.available()) {
        char c = client.read();
        Serial.write(c);
        if(c=='\n'){ 
          if(currentLineIsBlank){ 
            if(DEBUGGING){ Serial.println(); }
                 if(strcmp(RequestMethod,"GET")==0){ processGetRequest(client,RequestUri); }
            else if(strcmp(RequestMethod,"PUT")==0){ processPutRequest(client,RequestUri); }
            else if(strcmp(RequestMethod,"HEAD")==0){ processHeadRequest(client,RequestUri); }
            else { client.println(F("HTTP/1.1 400 Bad Request\n")); }
            break;
          } 
          else{ currentLineIsBlank = true; }
        }
        else if(c!='\r'){ currentLineIsBlank=false; }
        
      }//end if(client.available())
    }//end while(client.connected())
    
    
    delay(1); // Give the web browser time to receive the data
    client.stop(); // Close the connection  
    if(DEBUGGING){ Serial.println(F("---Client Closed---")); }  
  }//end if(client)
}

void processHeadRequest(EthernetClient client, char* uri) {
  bool found = SD.exists(uri);
  if(found) {
    client.println( F("HTTP/1.1 200 OK") );
    client.print  ( F("Content-Type: ") );
    client.println(parseContentType(uri));
    client.println( F("Connection: close") );
    client.println();
  } else { client.print(F("HTTP/1.1 404 Not Found\n")); }
}

void processGetRequest(EthernetClient client, char* uri) {
  processHeadRequest(client, uri); //Create the HEAD response
  File file = SD.open(uri); //Print the contents
  while(file.available()) {
    char c = file.read();
    client.write(c);
    if(DEBUGGING){ 
      if(c=='\n'){ Serial.println(); }
      else { Serial.write(c); }
    }
  }//end while(file.available())
}

void processPutRequest(EthernetClient client, char* uri) {
  Serial.println(client.available());
  client.println(F("HTTP/1.1 100 Continue\n"));
  long t = millis();
  while(!client.available() || millis()-t<5000){ delay(1); }
  if(DEBUGGING){ Serial.println(F(" -Begin Body- ")); }
  char c = 0;
  while(client.available() && c!=EOF){
    c = client.read();
    if(DEBUGGING){ 
      if(c=='\n'){ Serial.println(); }
      else { Serial.write(c); }
    }
  }
  client.println(F("HTTP/1.1 200 OK\n"));
}

String parseContentType(char* uri) {
  short i = UriLen;
  short e = 0;
  char ext[4] = {0,0,0,0};
  while(uri[--i]!='.'){ ;; }
  while(uri[i]!=0){ ext[e++] = uri[++i]; }
  // Return the appropriate content-type
       if(strcmp(ext,"htm")==0){ return "text/html"; }
  else if(strcmp(ext,"css")==0){ return "text/css"; }
  else if(strcmp(ext,"js")==0) { return "text/javascript"; }
  else { return "*/*"; }
}
