/* 
 *  Code for running a more sophisticated webserver on an Arduino Uno
 *  
 *  created  11 Sep 2019 
 *  modified 21 Nov 2019
 *  by Tristan Armitage  (armitageth@gmail.com)
 * 
 *  To understand how this code works, it is important to understand 
 *  how a client (e.g. a web browser) sends HTTP requests to a server. 
 *  
 *  In order to get information from a server, such as the contents of a 
 *  webpage, a client will send an HTTP request to that server, and the
 *  server will then respond appropriately. The code below allows an
 *  Arduino to create such a response just like any other server, but
 *  it does so based on the expectation of a proper HTTP request.
 *
 *  An HTTP request should be structured into a HEAD, which contains 
 *  information about the request, as well as a BODY, which contains any
 *  further content or data (although not all requests will include a BODY).
 *  The BODY doesn't require any particular internal structure, usually just
 *  the contents of a file or a set of data. The HEAD, on the other hand,
 *  will always start with a specific line containing, in order, the type
 *  of request, the Uniform-Resource-Identifier (URI, i.e. what content
 *  is being requested), and the HTTP version. The HEAD will then contain
 *  several "fields" providing more information, such as what browser is
 *  being used, any credentials the user might want to provide, and so on.
 *  At the end of the HEAD is an empty line, which separates it from the BODY.
 *
 *  An example of an HTTP request might look like this:
 *  (this example tries to update the index.html file on the server)
 *
 *      PUT /index.htm HTTP/1.1
 *      Host: 192.168.0.10
 *      Connection: keep-alive
 *      User-Agent: Mozilla/5.0
 *      Accept: text/html
 *      Authorization: Basic YXJteXRhZzpwYXNzd29yZA==
 *      
 *      <!DOCTYPE html>
 *      <html>
 *        <head>
 *          <title>Hello World</title>
 *        </head>
 *        <body>
 *          <h1>Hello, world!</h1>
 *        </body>
 *      </html>
 *  
 *  From this example we can see that the first line of the HEAD contains a
 *  request type (PUT), a URI (/index.htm), and an HTTP version.  After that,
 *  there are several header fields with further information, such as a basic
 *  HTTP Authorization.  The blank line indicates the end of the HEAD, and
 *  everything after that is the BODY of the request (in this case, a simple
 *  HTML page showing "Hello, world!"). Not all HTTP requests have a BODY.
 *
 *  You can search online for more information about HTTP requests and responses.
 *
 *  NOTES: 
 *  1. Arduino SD library only allows 8.3 filenames (aka SFN), meaning they 
 *      can have at most 8 characters, followed by a period '.', followed by a
 *      file extension of at most 3 characters (e.g. "filename.ext"). Importantly,
 *      this means the .html extension can NOT be used, so .htm is used instead.
 *  2. You can pass the client to other functions (no pointer notation needed). 
 *  3. Because the server can only handle one client at a time, the Keep-Alive
 *      header field is ignored and each request is always closed after response.
 */

#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>

/* These booleans control whether an output should be sent via Serial */
/* DISPLAYING is meant for simple output while DEBUGGING is more comprehensive */
boolean DISPLAYING = true;
boolean DEBUGGING  = false;
/* The DEBUGGING state is controlled by digital input at this pin */
const int debugPin = 5;

/* These constants are used to create and manipulate Strings of the given size */
const short MAX_METHOD_LENGTH = 10;
const short MAX_URI_LENGTH = 40;
const short MAX_HEAD_BUF_LENGTH = 100;
/* These variables are used to track the length of content in their respective Strings */
short MethodLen;
short UriLen;
short HeadBufLen;
/* The character arrays are created with a max length specified above */
char RequestMethod[MAX_METHOD_LENGTH];
char RequestUri[MAX_URI_LENGTH];
char HeaderBuffer[MAX_HEAD_BUF_LENGTH];

/* This is a base64 encoding of {username:password}, typical of simple HTTP authorization */ 
/* The character array stores the desired base64 encoding */
/* The boolean tracks whether the proper code has been received */
char Authorization[] = "YXJteXRhZzpwYXNzd29yZA==";
boolean Authorized;

/* These variables set the parameters that are used to set up the Ethernet server */
byte mac[]     = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte subnet[]  = { 255, 255, 255, 0 };
byte gateway[] = { 192, 168, 0, 1 };
IPAddress ip(192, 168, 0, 10);
EthernetServer server(80); /* Use port 80, the typical HTTP port */


/* Normal setup function. Calls the specific setup functions implemented below */
void 
setup() {
    pinMode(debugPin, INPUT);
    if(DISPLAYING||DEBUGGING){ Serial.begin(9600); }
    /* The following functions set up the SD card and Ethernet server */
    setupEthernet();
    setupSD();
    setupServer();
    /* The loop() will clear these Strings based on MethodLen and UriLen, so 
     * initially we set the lengths to max so the entire array will be reset */
    MethodLen = MAX_METHOD_LENGTH;
    UriLen = MAX_URI_LENGTH;
}

/* Normal loop function. This mostly focuses on recieving and parsing the
 * Request HEAD by reading until there is an empty line (which in HTTP separates
 * the HEAD from the BODY) and then calling the appropriate handler function */
void 
loop() {
    /* Reset the request strings, and set authentication state to false */
    clearRequestMethod();
    clearRequestUri();
    Authorized = false;

    /* Read the state of debugPin to decide whether detailed output is desired */
    DEBUGGING = digitalRead(debugPin); 

    /* Check for an available client */
    EthernetClient client = server.available();
    
    /* If we have a client connected, run the appropriate code */
    if (client) {
        if (DEBUGGING) {Serial.println(F("---New Client---"));}
        /* Because the HEAD is differentiated from the BODY by an empty line,
         * when we get to a newline character and the line was empty we want
         * to start handling the request.  To do this, we track whether the
         * line contained any characters by setting this boolean to false as soon as 
         * a non-return character (not 'newline' or 'carriage return') is encountered.
         * At the start of each new line it should be set back to true. */
        boolean currentLineIsBlank = true;
        /* The first line will be special, so we call a dedicated function for it */
        readRequestLine(client);
        /* Although we called a dedicated function, we print the results from here */
        if (DISPLAYING||DEBUGGING) {
            Serial.print(RequestMethod); Serial.print(" ");
            Serial.print(RequestUri); Serial.print(" ");
            if (DEBUGGING) {
                Serial.print(parseContentType(RequestUri));
            }
        }
        /* This is the main bulk of code, which receives the details of the request HEAD */
        while (client.connected()) {
            if (client.available()) {
                /* Read the request character by character */
                char c = client.read();
                if (DEBUGGING) {Serial.write(c);}
                /* if the character is 'newline', we must check if it ends the HEAD */
                if (c=='\n') {
                    /* If this is a blank line, we should handle the request */
                    /* NOTE: any BODY content still has not been read by the server */
                    if (currentLineIsBlank) {
                        handleRequest(client);
                        break;
                    } else { 
                        /* If the line wasn't blank, it had some parameters we should parse */
                        if (DEBUGGING) {Serial.println();}
                        parseHeaderLine(client);
                        currentLineIsBlank = true;
                    }
                } /* END if(c=='\n') */
                /* If the character isn't 'newline', and also isn't 'cairrage return'
                 * read it into the HeadBuf string (as long as there is room) */
                else if (c!='\r') { 
                    if (HeadBufLen<MAX_HEAD_BUF_LENGTH) {
                        HeaderBuffer[HeadBufLen] = c;
                        HeadBufLen++;
                    }
                    /* Because this is non-return character, the line isn't considered blank */
                    currentLineIsBlank=false; 
                }
            }/* END if(client.available()) */
        }/* END while (client.connected()) */
        
        /* Give the web browser time to receive the data */
        delay(1);
        /* Close the connection */
        client.stop();

        if (DEBUGGING) {Serial.println(F("---Client Closed---"));}
        if (DEBUGGING||DISPLAYING) {Serial.println();}  
    }/* END if(client) */
}

/*--- Functions for setting up the hardware peripherals ---*/
void 
setupEthernet() {
    /* Use the global parameters to initialize the Ethernet connection */
    Ethernet.begin(mac, ip, gateway, subnet);

    /* Handle the common errors that might occur with the Ethernet */
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        if (DEBUGGING) {
            Serial.println(F("No ethernet shield"));
        }
        /* If this error occurs, lock in an infinite loop */
        while (true) { delay(1); }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
        if (DEBUGGING) {
            Serial.println(F("No ethernet cable connected"));
        }
    }
}
void 
setupSD() {
    /* Try to initialize the SD card on pin 4 (default for Uno board) */
    if (!SD.begin(4)) {
        if (DEBUGGING) {
            Serial.println(F("Could not initialize SD card"));
        }
        /* If this error occurs, lock in an infinite loop */
        while (true) { delay(1); }
    }
}
void 
setupServer() {
    server.begin();
    if (DISPLAYING||DEBUGGING) { 
        Serial.println(F("Arduino Server started"));
        Serial.print  (F("Server is at "));
        Serial.println(Ethernet.localIP());
    }
}
/*--- Finish setup functions ---*/ 

/*--- Functions for clearing the Request strings ---*/
void
clearRequestMethod() {
    while (MethodLen>0) {
       MethodLen--; RequestMethod[MethodLen]=0;
   }
}
void
clearRequestUri() {
    while (UriLen>0) {
        UriLen--; RequestUri[UriLen]=0;
   }
}
/*--- Finish string clearing functions ---*/

/* Read the HTTP request, which should be the first line coming from the client
 * NOTE: A typical URI request should work fine, including folders and the initial '/' character */
void
readRequestLine(EthernetClient client) {
    /* The first part of this line should be the request type, e.g. GET, HEAD, POST, etc. */
    char c = client.read();
    while (c!=' ' && client.connected()) {
        RequestMethod[MethodLen]=c;
        MethodLen++;
        c=client.read();
    }
    /* The next part of this line should be the desired URI, i.e. the desired file or directory */
    c = client.read();
    while (c!=' ' && client.connected()) {
        RequestUri[UriLen]=c; 
        UriLen++; 
        c=client.read();
    }
    /* If no file is given within a directory, assume it should be index.htm */
    if (RequestUri[UriLen-1]=='/' && client.connected()) { 
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
    /* The last part of this line should be the HTTP version, but this code doesn't use that */
}

/* This function reads the content of HeaderBuffer, parsing the Field from the beginning,
 * then running code based on that. Currently on Authorization is used. */
void
parseHeaderLine(EthernetClient client) {
    /* Create a buffer to read the field into (size 40 arbitrarily chosen) */
    char headerField[40] = "";
    short hfLen=0;
    /* The field should end in a colon, so we read until that */
    while((HeaderBuffer[hfLen]!=':')&&(hfLen<40)&&(hfLen<HeadBufLen)) {
        headerField[hfLen] = HeaderBuffer[hfLen];
        hfLen++;
    }
    /* Based on the header field, we can run some specific code */
    if(strcmp(headerField,"Authorization")==0){ 
        /* We compare the value of this AUthorization field to our global Authorization string */
        /* NOTE: Here we jump 8 characters because the next characters would be ': Basic ' */
        if(strcmp(&HeaderBuffer[hfLen+8],Authorization)==0){ Authorized=true; }
    }
    /* Clear the HeaderBuffer array now that we are done with this header line */
    while(HeadBufLen > 0) {
        HeadBufLen--;
        HeaderBuffer[HeadBufLen] = 0;
    }
}

/* This function calls the appropriate function based on the request type */
void
handleRequest(EthernetClient client) {
    if (strcmp(RequestMethod,"GET")==0) {processGetRequest(client,RequestUri);}
    else if (strcmp(RequestMethod,"PUT")==0) {processPutRequest(client,RequestUri);}
    else if (strcmp(RequestMethod,"HEAD")==0) {processHeadRequest(client,RequestUri);}
    else if (strcmp(RequestMethod,"POST")==0) {processPostRequest(client,RequestUri);}
    /* If the request type is not one of the ones above, send an error response */
    else {client.println(F("HTTP/1.1 400 Bad Request\n"));}
    return;
}

/*--- Functions for handling particular HTTP requests ---*/
void 
processHeadRequest(EthernetClient client, char* uri) {
    /* Check if the requested URI exists */
    bool found = SD.exists(uri);
    if (found && client.connected()) {
        /* If the URI exists, send an OK response along with its type */
        client.println(F("HTTP/1.1 200 OK"));
        client.print  (F("Content-Type: "));
        client.println(parseContentType(uri));
        client.println(F("Connection: close"));
        client.println();
    } else {
        /* If the URI does not exist, send an error response */
        client.print(F("HTTP/1.1 404 Not Found\n"));
    }
}

void 
processGetRequest(EthernetClient client, char* uri) {
    /* Because HEAD is just like GET without the body, we first send a HEAD response */
    processHeadRequest(client, uri); 
    File file = SD.open(uri); 
    /* Directories should be handled differently.  Currently they are simply ignored */
    if (file.isDirectory()) {

    } else {
        /* For files, send the contents to the client */
        while(file.available() && client.connected()) {
            char c = file.read();
            client.write(c);
            if(DEBUGGING){ 
                if (c=='\n') {Serial.println();}
                else { Serial.write(c); }
            }
        }/* END while(file.available()) */
    }/* END else of if(file.isDirectory()) */
    file.close();
}

/* For PUT requests, it is assumed the desired URI is intended to be overwritten
 * therefore, proper authorization is required (this is handled in parseHeaderLine) */
void 
processPutRequest(EthernetClient client, char* uri) {
    /* See if the proper authorization had been provided during the request parsing */
    if (!Authorized){ 
        client.println(F("HTTP/1.1 401 Unauthorized\n")); 
        return;
    }
     /* Check if the requested URI already exists */
    if (SD.exists(uri)){
        File file = SD.open(uri);
        /* If it is a directory, don't allow the PUT request (i.e. no overwriting directories) */
        if (file.isDirectory()){
            file.close();
            client.println(F("HTTP/1.1 405 Method Not Allowed\n"));
            return;
        /* If it is a file, delete the existing version */
        } else { 
            file.close();
            SD.remove(uri); 
        }
        /* BUG: This should remove the directory of URI, but we already ignored directories ??? */
        SD.rmdir(uri);
    /* Create intermediate directories if they don't exist, otherwise SD.open() will fail to create the file */
    } else { 
        short lastSlash=UriLen-1;
        /* Starting from the end of the URI, find the last slash (which denotes folders rather than files) */
        while (lastSlash>0 && uri[lastSlash]!='/'){ lastSlash--; }
        char dir[lastSlash+1] = "";
        /* The rest of the URI before the last slash should only be names of folders */
        for (short i=0; i<lastSlash; i++) {
            dir[i] = uri[i];
        }
        /* If the folder doesn't exist already */
        /* NOTE: This should create all intermediate folders automatically */
        if (!SD.exists(dir)){ SD.mkdir(dir); }
    }/* END else of if(SD.exists(uri)) */

    /* Now that the filesystem has been preparaed, send a Continue response to receive the content */
    client.println(F("HTTP/1.1 100 Continue\n"));
    /* We record the current time to check for timeout */
    long t = millis();
    /* Prepare the the file to be written */
    File file = SD.open(uri, FILE_WRITE); 
    /* Wait for the content to be sent, timing out after 5 seconds */
    while (!client.available() && millis()-t<5000) { delay(1); } 
    if (DEBUGGING) { Serial.println(F(" -Begin Body- ")); }

    /* Write the content received from the client directly into the file */
    char c = 0;
    while (client.available() && c!=EOF) {
        c = client.read();
        file.write(c);
        if (DEBUGGING) { 
            if (c=='\n') { Serial.println(); }
            else { Serial.write(c); }
        }
    }
    file.close();
    client.println(F("HTTP/1.1 200 OK\n"));
}

/* POST requests are meant to append data to the end of a file */
void
processPostRequest(EthernetClient client, char* uri) {
    /* Check if the requested URI already exists */
    if (SD.exists(uri)){ 
        File file = SD.open(uri);
        /* If it is a directory, don't allow the POST request (i.e. no writing directories) */
        if (file.isDirectory()){ 
            file.close();
            client.println(F("HTTP/1.1 405 Method Not Allowed\n"));
            return;
        /* If it is a file, is simply needs to be closed */
        } else {
            file.close();
        }
    /* Create intermediate directories if they don't exist, otherwise SD.open() will fail to create the file */
    } else { 
        short lastSlash=UriLen-1;
        /* Starting from the end of the URI, find the last slash (which denotes folders rather than files) */
        while (lastSlash>0 && uri[lastSlash]!='/'){ lastSlash--; }
        char dir[lastSlash+1] = "";
        /* The rest of the URI before the last slash should only be names of folders */
        for (short i=0; i<lastSlash; i++) {
            dir[i] = uri[i];
        }
        /* If the folder doesn't exist already */
        /* NOTE: This should create all intermediate folders automatically */
        if (!SD.exists(dir)){ SD.mkdir(dir); }
    }/* END else of if(SD.exists(uri)) */

    /* Now that the filesystem has been preparaed, send a Continue response to receive the content */
    client.println(F("HTTP/1.1 100 Continue\n"));
    /* We record the current time to check for timeout */
    long t = millis();
    /* Prepare the the file to be written */
    File file = SD.open(uri, FILE_WRITE); 
    /* Wait for the content to be sent, timing out after 5 seconds */
    while (!client.available() && millis()-t<5000) { delay(1); } 
    if (DEBUGGING) { Serial.println(F(" -Begin Body- ")); }

    /* Write the content received from the client directly into the file */
    char c = 0;
    while (client.available() && c!=EOF) {
        c = client.read();
        file.write(c);
        if (DEBUGGING) { 
            if (c=='\n') { Serial.println(); }
            else { Serial.write(c); }
        }
    }
    file.close();
    client.println(F("HTTP/1.1 200 OK\n"));
    if (DEBUGGING||DISPLAYING) {
        Serial.println();
    }
}
/*--- Finish request handler functions ---*/

/* This function determines filetype from the file extension */
String 
parseContentType(char* uri) {
    short i = UriLen;
    short e = 0;
    /* Create an empty string */
    char ext[4] = {0,0,0,0}; 
    while (uri[--i]!='.' && i>0) { ;; }
    while (uri[i]!=0) { ext[e++] = uri[++i]; }
    /* Return the appropriate content-type */
         if(strcmp(ext,"htm")==0){ return "text/html"; }
    else if(strcmp(ext,"css")==0){ return "text/css"; }
    else if(strcmp(ext,"js")==0) { return "text/javascript"; }
    else if(strcmp(ext,"jpg")==0){ return "image/jpeg"; }
    else if(strcmp(ext,"svg")==0){ return "image/svg+xml"; }
    else { return "*/*"; }
}
