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
#define MAX_METHOD_LENGTH 10
#define MAX_URI_LENGTH 40
short mthd_len;
short uri_len;
//boolean debugging=true;

char request_method[MAX_METHOD_LENGTH];
char request_uri[MAX_URI_LENGTH];

char password[25] = "YXJteXRhZzpwYXNzd29yZAo=";

byte mac[]     = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte subnet[]  = { 255, 255, 255, 0 };
byte gateway[] = { 192, 168, 0, 1 };
IPAddress ip(192, 168, 0, 10);
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
  mthd_len = MAX_METHOD_LENGTH;
  uri_len  = MAX_URI_LENGTH;
}

/* Normal loop function.
 * NOTE: You can pass the client to other functions (not pointer notation needed). */
void loop() {
  // Reset the request strings
  while(mthd_len>0){ mthd_len--; request_method[mthd_len]=0; }
  while(uri_len>0) { uri_len--; request_uri[uri_len]=0; }
  
  // Check for an available client
  EthernetClient client = server.available();
  if(client) {
    if(debugging){ Serial.println("---New Client---"); }
    boolean currentLineIsBlank = true;
    char c = client.read();
    /* Read the HTTP request, which should be the first line coming from the client
     * NOTE: A typical URI request should work fine, including folders and the initial '/' character */
    while(c!=' '){ request_method[mthd_len]=c; mthd_len++; c=client.read(); }
    c = client.read();
    while(c!=' '){ request_uri[uri_len]=c; uri_len++; c=client.read(); }
    if(request_uri[uri_len-1]=='/'){ 
      request_uri[uri_len] = 'i'; uri_len++;
      request_uri[uri_len] = 'n'; uri_len++;
      request_uri[uri_len] = 'd'; uri_len++;
      request_uri[uri_len] = 'e'; uri_len++;
      request_uri[uri_len] = 'x'; uri_len++;
      request_uri[uri_len] = '.'; uri_len++;
      request_uri[uri_len] = 'h'; uri_len++;
      request_uri[uri_len] = 't'; uri_len++;
      request_uri[uri_len] = 'm'; uri_len++;
    } 
    if(debugging){
      Serial.println(request_method);
      Serial.println(request_uri);
      Serial.println(parseContentType(request_uri));
    }
    
    while(client.connected()) {
      if(client.available()) {
        char c = client.read();
        Serial.write(c);
        if(c=='\n'){ 
          if(currentLineIsBlank){ 
            if(debugging){ Serial.println(); }
                 if(strcmp(request_method,"GET")==0){ processGetRequest(client,request_uri); }
            else if(strcmp(request_method,"PUT")==0){ processPutRequest(client,request_uri); }
            else if(strcmp(request_method,"HEAD")==0){ processHeadRequest(client,request_uri); }
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

void processHeadRequest(EthernetClient client, char* uri) {
  bool found = SD.exists(uri);
  if(found) {
    client.println("HTTP/1.1 200 OK");
    client.print  ("Content-Type: ");
    client.println(parseContentType(uri));
    client.println("Connection: close");
    client.println();
  } else { client.println("HTTP/1.1 404 Not Found\n"); }
}

void processGetRequest(EthernetClient client, char* uri) {
  processHeadRequest(client, uri); //Create the HEAD response
  File file = SD.open(uri); //Print the contents
  while(file.available()) {
    char c = file.read();
    client.write(c);
    if(debugging){ 
      if(c=='\n'){ Serial.println(); }
      else { Serial.write(c); }
    }
  }//end while(file.available())
}

void processPutRequest(EthernetClient client, char* uri) {
  
}

String parseContentType(char* uri) {
  short i = uri_len;
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
