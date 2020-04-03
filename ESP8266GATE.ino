/* ESP8266 ESP8266GATE */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266SSDP.h>
#include <WebSocketsServer.h> // https://github.com/Links2004/arduinoWebSockets

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdate;
WebSocketsServer webSocket(81);

enum _cycle { none, protection, startfirst, startsecond, stopfirst, stopsecond, autoclose };
enum _move { closeright, closeleftright, openrightpartial, openrightpartialautoclose, openright, openrightautoclose, openrightleft, openrightleftautoclose };
enum _mode { singlepartial, single, both, firstrun };
enum _status { closed, closing, opening, open };

/* CONFIG */
const uint16_t protectionTimer = 1000UL; // DELAY TO PROTECT THE RELAYS
const uint16_t detainTimer = 3000UL;  // DELAY BETWEEN BOTH WINGS
const uint16_t wingPartialTimer = 9000UL;  // PARTIAL OPEN MODE 
const uint16_t wingTimer = 15000UL; // FULL OPEN MODE
const uint8_t onboardLed = 16; // ONBOARD LED USED WHEN GATE IS MOVING
const uint8_t onboardLed2 = 2; // ONBOARD LED 2 USED FOR SMARTCONFIG
const uint8_t smartButton = 0; // ONBOARD PUSHBUTTON TO PUT DEVICE INTO SMARTCONFIG
const uint8_t gateButton = 14; // PUSHBUTTON TO ACTIVE GATE
const uint8_t openLeftRelay = 4; // RELAY TO OPEN LEFT WING
const uint8_t closeLeftRelay = 5; // RELAY TO CLOSE LEFT WING
const uint8_t openRightRelay = 12; // RELAY TO OPEN RIGHT WING
const uint8_t closeRightRelay = 13; // RELAY TO CLOSE RIGHT WING
const char* userPassword = "user123"; // CONTROL PAGE PASSWORD
const char* adminPassword = "admin456"; // FIRMWARE PAGE PASSWORD
const uint8_t gateMode = _mode::singlepartial; // MODE WHEN USING GATEBUTTON (singlepartial / single / both)
const uint32_t gateClose = 300000UL; // AUTOCLOSE TIME GATEBUTTON (0UL = disabled)
const uint32_t gateDebounce = 1000UL; // DEBOUNCE DELAY FOR GATEBUTTON
/* END */

struct _millis
{
    uint32_t run;
    uint32_t protection;
    uint32_t first;
    uint32_t detain;
    uint32_t second;
    uint32_t autoclose;
} millisStruct;

uint8_t left = _status::open;
uint8_t right = _status::open;
uint8_t cycle = _cycle::none;
uint8_t move = _move::closeleftright;
uint8_t mode = _mode::firstrun;
uint32_t close = 0UL;

const char html[] PROGMEM = R"=====(
<!DOCTYPE html>
<style>
  html {
    text-align: center;
    color: black;
    font-size: 72px;
  }

  h1 {
    font-size: 132px;
    text-decoration: overline underline;
    font-weight: bold;
  }

  input[type=button], select {
    color: white;
    background-color: gray;
    border: 1px solid black;
    border-radius: 25px;
    font-size: 72px;
    width: 800px;
    text-align-last: center;
  }

  input[type=button]:hover, input[type=button]:disabled, select:disabled {
	  background-color: black;
  }

  p {
    font-weight: bold;
  }
</style>

<body>
  <h1>ESP8266GATE</h1>
  <img id="rightImg" style="width:200px"><img id="leftImg" style="width:200px"><br />
  <p id="updateP">Connecting!</p>
  <input type="button" id="runButton" value="Open" onclick="onclick_runButton()" disabled><br /><br />
  <select id="modeSelect" disabled>
    <option value="0">Right partial wing</option>
    <option value="1">Right wing</option>
    <option value="2">Both wings</option>
  </select><br /><br />
  <select id="timeSelect" disabled>
    <option value="0UL">Manuel</option>
    <option value="60000UL">1 minute</option>
    <option value="300000UL">5 minutes</option>
    <option value="900000UL">15 minutes</option>
    <option value="1800000UL">30 minutes</option>
    <option value="3600000UL">1 hour</option>
    <option value="7200000UL">2 hours</option>
  </select>
<script>
  var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/', ['arduino']);
  var interval;
	var counter;
	var runButton = document.getElementById('runButton');
  var leftImg = document.getElementById('leftImg');
  var rightImg = document.getElementById('rightImg');
  var modeSelect = document.getElementById('modeSelect');
  var timeSelect = document.getElementById('timeSelect');
  var updateP = document.getElementById('updateP');
     
	webSocket.onopen = function () {
    updateP.innerText = 'Connected!';
  };
			
  webSocket.onerror = function(e) {
    updateP.innerText = 'webSocket ' + e.data + ' error!';
    runButton.disabled = true;
    modeSelect.disabled = true;
    timeSelect.disabled = true;
  };

  webSocket.onclose = function () {
    updateP.innerText = 'Disconnected!';
    runButton.disabled = true;
    modeSelect.disabled = true;
    timeSelect.disabled = true;
  };
			
  webSocket.onmessage = function(e) {
	  console.log(e.data);
		[cycle, move, mode, left, right, close] = e.data.split(":");
    updateMovement(cycle, move, mode, left, right, close);
  };

  function updateMovement(cycle, move, mode, left, right, close) {
    if (cycle === '0') {
		  updateP.innerText = 'Awaiting instructions!';
    }
    else if (cycle === '1' || cycle === '2' || cycle === '3' || (cycle === '4' && (move === '1' || move === '6' || move === '7'))) {
		  updateP.innerText = 'Moving...';
		}
		else if (cycle === '4' && (move === '3' || move === '5') || (cycle === '5' && move === '7')) {
		  interval = setInterval(closeTimer, 1000);
			counter = close;
    }
		else if (cycle === '6') {
		  clearInterval(interval);
			counter = 0;
		}

		if (mode === '3') {
		  runButton.value = 'CLOSE (FIRSTRUN)';
			if (cycle === '0') {
			  runButton.disabled = false;
			}
			else {
			  runButton.disabled = true;
			}
		}
		else if (move === '0' || move === '1') {
		  runButton.value = 'OPEN';
			runButton.disabled = false;
			modeSelect.disabled = false;
      timeSelect.disabled = false;
    }
		else {
		  runButton.value = 'CLOSE';
			runButton.disabled = false;
			modeSelect.disabled = true;
      timeSelect.disabled = true;
		}

    switch (left) {
      case '0': leftImg.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAABZSURBVEhL7dMhDgAhDETRDldYi+T+J0Ji9wxdU0VIJgEqNplnqPtiAtzdzgCIa6XEm0YBCm9tce56Ro9rRRtQ+SNn/+QLgcnU08iUApQClAKUApQC1N8DZh/bgQw9WZ7eNwAAAABJRU5ErkJggg=='; break;
      case '1': leftImg.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAC+SURBVEhL7ZVLDsMgDEShR2mu089Zk/Y6yVXoRCCCqJGJbXZ5m3j1BmtQ8CEEp8N7nyaKW/oO4wpgGR5gcIum5RmH7f2NQ4l2g2wH9/mRpgJVQGlvIQyAuscOJAEtNdnB6ZJb9vX1iUP15zi3AWv/p3eDfrVkA8HBM3yAxg6YAKUdNDsQq7s60B88QwQY2kEdYGsHRwdWaroD84Nn9oBxdkDfImBiB0QA1FZ2sAeUOkN1xODRr1C9BwKuAAbnfpqtUTp0EAG5AAAAAElFTkSuQmCC';	break;
      case '2': leftImg.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAADASURBVEhL7ZUxDsMgDEWhR2jWdmvvf6Bka9fkCvRHIIKokYlttrwlnt7H+ij4EILT4b1PE8UtfYdxBbAMDzC4RdvzHYfpu8ShRLtBtoP18UpTgSqgtLcQBkDdYweSgJaa7OB0yS37/TPHofpznNuAtf/Tu0G/WrKB4OAZPkBjB0yA0g6aHYjVXR3oD54hAgztoA6wtYOjAys13YH5wTN7wDg7oG8RMLEDIgBqKzvYA0qdoTpi8OhXqN4DAVcAg3M/aWVROp67J7YAAAAASUVORK5CYII=';	break;
      case '3': leftImg.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAABRSURBVEhL7dSxCYBAEAXRXVuxHrFXuXpsZYVjQ2ECNRDmJfezCRYuqyqeycxed5Z+P2MAGUAG0AtfxXpsvaZzH70mb4AMIAPIADKADKC/ByIutVwJP5S4jc8AAAAASUVORK5CYII='; break;
    }
				
		switch (right) {
      case '0': rightImg.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAACgSURBVEhLrdKxDUMxDEPBDJIy+2+WGX6aAwsDSmHx4IoQXuXXf8+a0MTVgtDE1YLQxNWC0MTVgtDk+/4sn9DkuL54QpPj+uIJTY7riyc0Oa4vntDEV1gQCnOPbph7dMPcoxvmHt0w9+iGuUc3zD26Ye7RDXOPbph7dMPcoxvmHt0w9+iGuUc3zD26Ye7RDXOPbph7dMPcoxvmHt0wtzzPD00Iqp4+kZkGAAAAAElFTkSuQmCC'; break;
      case '1': rightImg.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAC2SURBVEhL7ZRBDsMgDARpn5J+J+1fk76nbyGWjKyK4Cy2yY25xCdGyzo80iU55zJ5eZbvbUwB5HZBe4uWbeXh9/ny4KYhkNOZoANf0Wt/l8kFTiD4ooAOKhyOWiBvg3YzVofagXaQtRI1gRCMgrcoGKXrT444ep8KtwN3UGGtpDeBYI1iFhAmh0dA9DvMHVTASpwJBBglKiCuHQMEhOYgxggIcjQ1wwTMv4Pn6BZBBic4MwWAlA4Pk0+RA4HIUwAAAABJRU5ErkJggg=='; break;
      case '2': rightImg.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAAC4SURBVEhL7ZQxDsMgDEVpj5C52dr7Hyhj5vYKxJKRVRGcj22y8ZZ44unzHR7pkpxzmbw8y/c2pgByu6C9Rd/Xm4dl33hw0xDI6UzQga/ot37K5AInEHxRQAcVDkctkLdBuxmrQ+1AO8haiZpACEbBWxSM0vUnRxy9T4XbgTuosFbSm0CwRjELCJPDIyD6HeYOKmAlzgQCjBIVENeOAQJCcxBjBAQ5mpphAubfwXN0iyCDE5yZAkBKB+euT1lbuaVEAAAAAElFTkSuQmCC'; break;
      case '3': rightImg.src = 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAAXNSR0IArs4c6QAAAARnQU1BAACxjwv8YQUAAAAJcEhZcwAADsMAAA7DAcdvqGQAAABQSURBVEhLY2TAC/7//w9lkQuYoDTNwKgFBMGoBQTBqAUEAXpRobTRB8oCg7t+m6EscsFoHBAEoxYQBKMWEASjFhAEoxYQBKMWEARD3QIGBgDgAQY5iqdB2QAAAABJRU5ErkJggg=='; break;
    }
  }
			
	function closeTimer() { 
	  counter = Math.floor(counter - 1000);
		var h = Math.floor(counter / 3600000);
		var m = Math.floor((counter % 3600000) / 60000);
		var s = Math.floor(((counter % 360000) % 60000) / 1000);
		updateP.innerText = 'Closing in ' + h + 'h ' + m + 'm ' + s + 's!';
	}

  function onclick_runButton() {
    webSocket.send(modeSelect[modeSelect.selectedIndex].value + ':' + timeSelect[timeSelect.selectedIndex].value);
  }
</script>
</body>
</html>
)=====";

void setup()
{
    Serial.begin(115200);
    delay(10);
    Serial.print(F("ESP8266GATE BOOTING UP..."));
    pinMode(smartButton, INPUT_PULLUP);
    pinMode(gateButton, INPUT_PULLUP);
    pinMode(onboardLed, OUTPUT);
    pinMode(onboardLed2, OUTPUT);
    pinMode(openLeftRelay, OUTPUT);
    pinMode(closeLeftRelay, OUTPUT);
    pinMode(openRightRelay, OUTPUT);
    pinMode(closeRightRelay, OUTPUT);
    digitalWrite(openLeftRelay, HIGH);
    digitalWrite(closeLeftRelay, HIGH);
    digitalWrite(openRightRelay, HIGH);
    digitalWrite(closeRightRelay, HIGH);

    httpServer.on("/", []() {
        if (!httpServer.authenticate("user", userPassword)) {
            return httpServer.requestAuthentication(HTTPAuthMethod::DIGEST_AUTH, "ESP8266GATE", "Authentication Failed!");
        }
        httpServer.send_P(200, "text/html", html);
    });

    httpServer.on("/description.xml", HTTP_GET, []() {
        SSDP.schema(httpServer.client());
    });

    httpServer.begin();
    httpUpdate.setup(&httpServer, "/firmware", "admin", adminPassword);

    MDNS.begin("ESP8266GATE");
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("Websocket", "tcp", 81);

    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName("ESP8266GATE");
    SSDP.setSerialNumber(ESP.getChipId());
    SSDP.setURL("/");
    SSDP.setModelName("ESP8266GATE");
    SSDP.setModelNumber("v1");
    SSDP.setModelURL("https://github.com/VDBKevin");
    SSDP.setManufacturer("VDBKevin");
    SSDP.setManufacturerURL("https://github.com/VDBKevin");
    SSDP.setDeviceType("upnp:rootdevice");
    SSDP.begin();

    WiFi.hostname("ESP8266GATE");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.println(F("DONE!"));
}

void loop()
{
    if (digitalRead(smartButton) == LOW) {
      smartConfig();
    }

    if (digitalRead(gateButton) == LOW) {
      uint32_t current = millis();
      while(digitalRead(gateButton) == LOW);
      if (millis() -  current >= gateDebounce) {
        if (mode != _mode::firstrun) {
          mode = gateMode;
          close = gateClose;
        }
        Serial.println(F("GATEBUTTON TRIGGER"));
        millisStruct.run = millis();
      }
    }

    MDNS.update();
    webSocket.loop();
    httpServer.handleClient();
    controlHandler();
}

void controlHandler()
{
    uint32_t current = millis();

    if (millisStruct.run != 0UL) {
        millisStruct.run = 0UL;

        if (mode == _mode::firstrun && cycle == _cycle::none) {// FORCE GATE TO BE CLOSED FIRST AFTER A POWERLOSS. THIS CAN NOT BE INTERRUPTED
            move = _move::closeleftright;
            Serial.println(F("CLOSELEFTRIGHT...FIRSTRUN"));
        }
        else if (mode != _mode::firstrun && (cycle == _cycle::none || cycle == _cycle::autoclose)) { // A FRESH RUN
            if (mode == _mode::singlepartial) {
                if (right == _status::opening || right == _status::open) {
                    if (left == _status::opening || left == _status::open) { // LEFT WING IS OPEN(ING). FORCE TO CLOSE BOTH
                        move = _move::closeleftright;
                        Serial.println(F("CLOSELEFTRIGHT...FORCED"));
                    }
                    else {
                        move = _move::closeright;
                        Serial.println(F("CLOSERIGHT"));
                    }
                }
                else {
                    if (close == 0UL) {
                        move = _move::openrightpartial;
                        Serial.println(F("OPENRIGHTPARTIAL"));
                    }
                    else {
                        move = _move::openrightpartialautoclose;
                        Serial.println(F("OPENRIGHTPARTIALAUTOCLOSE"));
                    }
                }
            }
            else if (mode == _mode::single) {
                if (right == _status::opening || right == _status::open) {
                    if (left == _status::opening || left == _status::open) { // LEFT WING IS OPEN(ING). FORCE TO CLOSE BOTH
                        move = _move::closeleftright;
                        Serial.println(F("CLOSELEFTRIGHT...FORCED"));
                    }
                    else {
                        move = _move::closeright;
                        Serial.println(F("CLOSERIGHT"));
                    }
                }
                else {
                    if (close == 0UL) {
                        move = _move::openright;
                        Serial.println(F("OPENRIGHT"));
                    }
                    else {
                        move = _move::openrightautoclose;
                        Serial.println(F("OPENRIGHTAUTOCLOSE"));
                    }
                }
            }
            if (mode == _mode::both) {
                if (left == _status::opening || left == _status::open && (right == _status::opening || right == _status::open)) {
                    move = _move::closeleftright;
                    Serial.println(F("CLOSELEFTRIGHT"));
                }
                else {
                    if (close == 0UL) {
                        move = _move::openrightleft;
                        Serial.println(F("OPENRIGHTLEFT"));
                    }
                    else {
                        move = _move::openrightleftautoclose;
                        Serial.println(F("OPENRIGHTLEFTAUTOCLOSE"));
                    }
                }
            }
        }
        else if (mode != _mode::firstrun && cycle != _cycle::none) { // RUN INTERRUPTED. REVERSE IT
            if (move == _move::closeright) {
                if (close == 0UL)
                {
                    if (mode == _mode::singlepartial) {
                        move = _move::openrightpartial;
                        Serial.println(F("ABORTED...OPENRIGHTPARTIAL"));
                    }
                    else if (mode == _mode::single) {
                        move = _move::openright;
                        Serial.println(F("ABORTED...OPENRIGHT"));
                    }
                }
                else {
                    if (mode == _mode::singlepartial) {
                        move = _move::openrightpartialautoclose;
                        Serial.println(F("ABORTED...OPENRIGHTPARTIALAUTOCLOSE"));
                    }
                    else if (mode == _mode::single) {
                        move = _move::openrightautoclose;
                        Serial.println(F("ABORTED...OPENRIGHTAUTOCLOSE"));
                    }
                }
            }
            else if (move == _move::closeleftright) {
                if (close == 0UL) {
                    move = _move::openrightleft;
                    Serial.println(F("ABORTED...OPENRIGHTLEFT"));
                }
                else {
                    move = _move::openrightleftautoclose;
                    Serial.println(F("ABORTED...OPENRIGHTLEFTAUTOCLOSE"));
                }
            }
            else if (move == _move::openrightpartial || move == _move::openrightpartialautoclose || move == _move::openright || move == _move::openrightautoclose) {
                move = _move::closeright;
                Serial.println(F("ABORTED...CLOSERIGHT"));
            }
            else if (move == _move::openrightleft || move == _move::openrightleftautoclose) {
                move = _move::closeleftright;
                Serial.println(F("ABORTED...CLOSELEFTRIGHT"));
            }
        }

        if (move == _move::closeright || move == _move::openright) {
            millisStruct.protection = current + protectionTimer;
            millisStruct.first = millisStruct.protection + wingTimer;
            millisStruct.detain = 0UL;
            millisStruct.second = 0UL;
            millisStruct.autoclose = 0UL;
        }
        else if (move == _move::closeleftright || move == _move::openrightleft) {
            millisStruct.protection = current + protectionTimer;
            millisStruct.first = millisStruct.protection + wingTimer;
            millisStruct.detain = millisStruct.protection + detainTimer;
            millisStruct.second = millisStruct.detain + wingTimer;
            millisStruct.autoclose = 0UL;
        }
        else if (move == _move::openrightpartial) {
            millisStruct.protection = current + protectionTimer;
            millisStruct.first = millisStruct.protection + wingPartialTimer;
            millisStruct.detain = 0UL;
            millisStruct.second = 0UL;
            millisStruct.autoclose = 0UL;
        }
        else if (move == _move::openrightpartialautoclose) {
            millisStruct.protection = current + protectionTimer;
            millisStruct.first = millisStruct.protection + wingPartialTimer;
            millisStruct.detain = 0UL;
            millisStruct.second = 0UL;
            millisStruct.autoclose = millisStruct.first + close;
        }
        else if (move == _move::openrightautoclose) {
            millisStruct.protection = current + protectionTimer;
            millisStruct.first = millisStruct.protection + wingTimer;
            millisStruct.detain = 0UL;
            millisStruct.second = 0UL;
            millisStruct.autoclose = millisStruct.first + close;
        }
        else if (move == _move::openrightleftautoclose) {
            millisStruct.protection = current + protectionTimer;
            millisStruct.first = millisStruct.protection + wingTimer;
            millisStruct.detain = millisStruct.protection + detainTimer;
            millisStruct.second = millisStruct.detain + wingTimer;
            millisStruct.autoclose = millisStruct.second + close;
        }

        digitalWrite(closeLeftRelay, HIGH);
        digitalWrite(openLeftRelay, HIGH);
        digitalWrite(closeRightRelay, HIGH);
        digitalWrite(openRightRelay, HIGH);
        digitalWrite(onboardLed, LOW);
        Serial.println(F("PROTECTING THE RELAYS"));
        cycle = _cycle::protection;
        webSocketUpdate();
    }
    else if (millisStruct.protection != 0UL && current >= millisStruct.protection) {
        millisStruct.protection = 0UL;
        if (move == _move::closeright) {
            digitalWrite(closeRightRelay, LOW);
            Serial.println(F("CLOSING RIGHT WING"));
            right = _status::closing;
        }
        else if (move == _move::closeleftright) {
            digitalWrite(closeLeftRelay, LOW);
            Serial.println(F("ClOSING LEFT WING"));
            left = _status::closing;
        }
        else if (move == _move::openrightpartial || move == _move::openrightpartialautoclose || move == _move::openright || move == _move::openrightautoclose || move == _move::openrightleft || move == _move::openrightleftautoclose) {
            digitalWrite(openRightRelay, LOW);
            Serial.println(F("OPENING RIGHT WING"));
            right = _status::opening;
        }
        cycle = _cycle::startfirst;
        webSocketUpdate();
    }
    else if (millisStruct.detain != 0UL && current >= millisStruct.detain) {
        millisStruct.detain = 0UL;
        if (move == _move::closeleftright) {
            digitalWrite(closeRightRelay, LOW);
            Serial.println(F("CLOSING RIGHT WING"));
            right = _status::closing;
        }
        else if (move == _move::openrightleft || move == _move::openrightleftautoclose) {
            digitalWrite(openLeftRelay, LOW);
            Serial.println(F("OPENING LEFT WING"));
            left = _status::opening;
        }
        cycle = _cycle::startsecond;
        webSocketUpdate();
    }
    else if (millisStruct.first != 0UL && current >= millisStruct.first) {
        millisStruct.first = 0UL;
        if (move == _move::closeright) {
            digitalWrite(closeRightRelay, HIGH);
            digitalWrite(onboardLed, HIGH);
            Serial.println(F("FINISCHED!"));
            right = _status::closed;
            cycle = _cycle::none;
        }
        else if (move == _move::closeleftright) {
            digitalWrite(closeLeftRelay, HIGH);
            Serial.println(F("STOP LEFT WING"));
            left = _status::closed;
            cycle = _cycle::stopfirst;
        }
        else if (move == _move::openrightpartial || move == _move::openright) {
            digitalWrite(openRightRelay, HIGH);
            digitalWrite(onboardLed, HIGH);
            Serial.println(F("FINISCHED!"));
            right = _status::open;
            cycle = _cycle::none;
        }
        else if (move == _move::openrightpartialautoclose || move == _move::openrightautoclose || move == _move::openrightleft || move == _move::openrightleftautoclose) {
            digitalWrite(openRightRelay, HIGH);
            Serial.println(F("STOP RIGHT WING"));
            right = _status::open;
            cycle = _cycle::stopfirst;
        }
        webSocketUpdate();
    }
    else if (millisStruct.second != 0UL && current >= millisStruct.second) {
        millisStruct.second = 0UL;
        if (move == _move::closeleftright) {
            if (mode == _mode::firstrun) {
                mode = _mode::both;
            }
            digitalWrite(closeRightRelay, HIGH);
            digitalWrite(onboardLed, HIGH);
            Serial.println(F("FINISCHED!"));
            right = _status::closed;
            cycle = _cycle::none;
        }
        else if (move == _move::openrightleft) {
            digitalWrite(openLeftRelay, HIGH);
            digitalWrite(onboardLed, HIGH);
            Serial.println(F("FINISCHED!"));
            left = _status::open;
            cycle = _cycle::none;
        }
        else if (move == _move::openrightleftautoclose) {
            digitalWrite(openLeftRelay, HIGH);
            Serial.println(F("STOP RIGHT WING"));
            left = _status::open;
            cycle = _cycle::stopsecond;
        }
        webSocketUpdate();
    }
    else if (millisStruct.autoclose != 0UL && current >= millisStruct.autoclose) {
        millisStruct.autoclose = 0UL;
        if (move == _move::openrightpartialautoclose || move == _move::openrightautoclose) {
            move = _move::closeright;
        }
        else if (move == _move::openrightleftautoclose) {
            move = _move::closeleftright;
        }
        digitalWrite(onboardLed, HIGH);
        Serial.println(F("AUTO CLOSING"));
        millisStruct.run = millis();
        cycle = _cycle::autoclose;
        webSocketUpdate();
    }
}

void webSocketUpdate()
{
    char c[31];
    snprintf_P(c, sizeof(c), "%u:%u:%u:%u:%u:%lu", cycle, move, mode, left, right, close);
    webSocket.broadcastTXT(c);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length)
{
    switch (type) {
    case WStype_DISCONNECTED:
        Serial.printf("WEBSOCKET DISCONNECTED %u\n", num);
        break;
    case WStype_CONNECTED:
        Serial.printf("WEBSOCKED CONNECTED %u %s %s\n", num, webSocket.remoteIP(num).toString().c_str(), payload);
        webSocketUpdate();
        break;
    case WStype_TEXT:
        uint8_t a;
        uint32_t b;
        if (sscanf((const char*)payload, "%u:%luUL", &a, &b) == 2) {
            if (mode != _mode::firstrun) {
                mode = a;
                close = b;
            }
            Serial.println(F("WEBSOCKET TRIGGER"));
            millisStruct.run = millis();
        }
        else {
            Serial.printf("WEBSOCKET RECIEVED INVALID DATA \"%d\" FROM %u\n", payload, num);
        }
    }
}

void smartConfig()
{
    WiFi.beginSmartConfig();
    Serial.print(F("SMARTCONFIG"));

    while (!WiFi.smartConfigDone()) {
        digitalWrite(onboardLed2, LOW);
        delay(500);
        Serial.print(".");
        digitalWrite(onboardLed2, HIGH);
        delay(500);
    }
    Serial.println(F("DONE!"));
    Serial.print(F("CONNECTING TO WIFI"));

    while (WiFi.status() != wl_status_t::WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(F("CONNECTED!"));
}
