/*=============================================================================
 =======                            INCLUDES                            =======
 =============================================================================*/
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include "FS.h"

/*=============================================================================
 =======               DEFINES & MACROS FOR GENERAL PURPOSE             =======
 =============================================================================*/
#undef DEBUG

// access point
#define AP_NAME "geiger_counter"
#define AP_TIMEOUT 60

// Geiger Mueller tube parameterization (see datasheet of SBM-20)
// gamma sensivity for Radium224: 29cps = 1740cpm ≙ 1mR/h = 10μS/h ->  1cpm ≙ 0.0057μS/h
#define SBM20_FACTOR  ((double)0.0057)
#define NBR_GMTUBES   2
#define TUBE_FACTOR   (SBM20_FACTOR / (double)NBR_GMTUBES)

// NTP stuff
#define NTP_PACKET_SIZE        48
#define SEVENTYYEARS   2208988800UL

// file logging stuff
#define FILELOGRATE      10                             // minutes
#define MAXFILELINES     ((28 * 24 * 60) / FILELOGRATE) // 28 days of data, every 10 min one  element
#define MAXFILES         26                             // about 2 years of data
#define MAXCHARTELEMENTS 60                             // number of displayed elements on the graph

/*=============================================================================
 =======                       CONSTANTS  &  TYPES                      =======
 =============================================================================*/
typedef enum
{
	NTP_REQUEST,
	NTP_WAIT_RESPONSE,
	NTP_PARSE_RESPONSE,
	NTP_SLEEP
} ntp_states_e;

typedef enum
{
	PIEZO_START,
	PIEZO_ON,
	PIEZO_SLEEP
} piezo_states_e;

/*=============================================================================
 =======                VARIABLES & MESSAGES & RESSOURCEN               =======
 =============================================================================*/
// math stuff
double startMillis = 0.0;
double seconds = 0.0;
double minutes = 0.0;
double CPM = 0.0;
double uSv = 0.0;
double lastcSv = 0.0;
uint32 hitCount = 0ul;
uint32 minHolder = 0ul;
uint32 minuteHits = 0ul;

// filesystem stuff
uint32 lineCount = 0ul;
uint32 fileCount = 0ul;
String fName = "/geiger/gdata";
double lastcSvAvr = 0.0;

// graph stuff
String chartvalues[MAXCHARTELEMENTS];
uint32 chartvalues_cnt = 0ul;
boolean chartvalues_max = false;

// NTP client stuff
IPAddress timeServerIP; 
unsigned int localPort = 2390;
byte packetBuffer[NTP_PACKET_SIZE];
ntp_states_e ntp_state = NTP_REQUEST;
uint32 ntp_millis;

// piezo stuff
piezo_states_e piezo_state = PIEZO_SLEEP;
uint32 piezo_micros;

// WIFI stuff
WiFiUDP udp;
WiFiManager wifiManager;
WiFiServer server(80);
uint32 webRefresh = 60ul;    // how often the webpage refreshes

/*=============================================================================
 =======                              METHODS                           =======
 =============================================================================*/
unsigned long sendNTPpacket(IPAddress &address)
{
	// send an NTP request to the time server at the given address
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12]  = 49;
	packetBuffer[13]  = 0x4E;
	packetBuffer[14]  = 49;
	packetBuffer[15]  = 52;

	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	udp.beginPacket(address, 123); // NTP requests are to port 123
	udp.write(packetBuffer, NTP_PACKET_SIZE);
	udp.endPacket();
}

void GMpulse()
{
	seconds = ((double)millis() - startMillis) / 1000.0;
	minutes = seconds / 60.0;
	hitCount ++;
	minuteHits ++;

	CPM = ((double)hitCount) / minutes;
	uSv = CPM * TUBE_FACTOR;

	if (piezo_state == PIEZO_SLEEP)
	{
		piezo_state = PIEZO_START;
	}

#ifdef DEBUG
	Serial.print(" Hit Count: "); Serial.println(hitCount);
	Serial.print(" Time seconds: "); Serial.println(seconds);
	Serial.print(" Time minutes: "); Serial.println(minutes);
	Serial.print(" CPM: "); Serial.println(CPM);
	Serial.print(" uSv/hr: "); Serial.println(uSv);
	Serial.println(" ");
#endif
}

void resetVariables()
{
	hitCount = 0ul;
	startMillis = (double)millis();
	seconds = 0.0;
	minutes = 0.0;
	CPM = 0.0;
	uSv = 0.0;
	minHolder = 0ul;
	minuteHits = 0ul;
	lastcSv = 0.0;
	lastcSvAvr = 0.0;
	chartvalues_cnt = 0ul;
	chartvalues_max = false;
}

void setup() 
{
	// init serial logging
	Serial.begin(115200);

	// init file logging
	SPIFFS.begin();
	Dir dir = SPIFFS.openDir("/geiger");
	// determine latest logging file
	long timeval = 0;
	while (dir.next()) 
	{
		long t = dir.fileName().substring(13, 23).toInt();	
		if (t > timeval)
		{
			timeval = t; 
		}
		fileCount ++;
	}

	fName += timeval;
	fName += ".txt";
	File f = SPIFFS.open(fName, "r");
	if (!f) 
	{
		// the very first time no logging file is present -> format the filesystem
		Serial.println("Please wait 30 secs for SPIFFS to be formatted");
		SPIFFS.format();
		Serial.println("Spiffs formatted");
		fName = "/geiger/gdata1550000000.txt";
		f = SPIFFS.open(fName, "w");
		f.close();
		fileCount = 1;
		lineCount = 0;
		Serial.println("Data file created");
	}
	else
	{
		Serial.println("Data file exists");
		while (f.available()) 
		{
			// lets read line by line from the file
			String str = f.readStringUntil('\n');
			lineCount ++;
		}
		f.close();
	}

	// init WIFI
	wifiManager.setAPStaticIPConfig(IPAddress(192,168,1,1), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
	wifiManager.setTimeout(AP_TIMEOUT); // Timeout until WIFI config portal is turned off
	if (!wifiManager.autoConnect(AP_NAME))
	{
		Serial.println("WIFI not connected after timeout"); 
	}
	else
	{
		Serial.println("WIFI connected");    
		server.begin();
		Serial.println("WEB server started");
		udp.begin(localPort);
		Serial.println("UDP client started");
	}

	// switch (must be pullup input, D2 = GPIO4)
	pinMode(D2, INPUT_PULLUP);

	// piezo (output, D8 = GPIO15)
	pinMode(D8, OUTPUT);  
	digitalWrite(D8, LOW);

	// GMpulse (interrupt, D7 = GPIO13)
	pinMode(D7, INPUT);
	startMillis = (double)millis();
	attachInterrupt(D7, GMpulse, FALLING);

	yield();
}


void loop() 
{
	// piezo tick handling
	switch (piezo_state) 
	{
	case PIEZO_START:
		digitalWrite(D8, HIGH);
		piezo_micros = micros();
		piezo_state = PIEZO_ON;
		break;
	case PIEZO_ON:
		// piezo pulse duration is 1ms to get a nice tick
		if ((micros() - piezo_micros) > 1000)
		{
			piezo_state = PIEZO_SLEEP;
		}
		break;
	case PIEZO_SLEEP:
		digitalWrite(D8, LOW);
		break;
	default:
		break;
	}

	// NTP handling  
	if (WiFi.status() == WL_CONNECTED)
	{
		switch (ntp_state)
		{
		case NTP_REQUEST:
			WiFi.hostByName("de.pool.ntp.org", timeServerIP); //get a random server from the pool
			sendNTPpacket(timeServerIP); // send an NTP packet to a time server
			ntp_millis = millis();
			ntp_state = NTP_WAIT_RESPONSE;
			break;
		case NTP_WAIT_RESPONSE:
			if((millis() - ntp_millis) > 1000)
			{
				ntp_state = NTP_PARSE_RESPONSE;
			}
			break;
		case NTP_PARSE_RESPONSE:
			if (NTP_PACKET_SIZE <= udp.parsePacket())
			{
				unsigned long secsSince1900;
				unsigned long epoch;

				// We've received a packet, read the data from it
				udp.read(packetBuffer, NTP_PACKET_SIZE);

				//the timestamp starts at byte 40 of the received packet and is four bytes,
				secsSince1900  = (unsigned long)packetBuffer[40] << 24;
				secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
				secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
				secsSince1900 |= (unsigned long)packetBuffer[43];

				// now convert NTP time into everyday time.
				// Unix time starts on Jan 1 1970. In seconds, that's 2208988800.
				// subtract seventy years.
				epoch = secsSince1900 - SEVENTYYEARS;
				setTime(epoch);
				Serial.print("Unix time: ");
				Serial.println(epoch);
			}
			ntp_millis = millis();
			ntp_state = NTP_SLEEP;
			break;
		case NTP_SLEEP:
			// next NTP request in 10min
			if((millis() - ntp_millis) > 600000)
			{
				ntp_state = NTP_REQUEST;
			}
			break;
		default:
			break;
		}
	}

	// long term logging into filesystem
	if (lineCount == MAXFILELINES)
	{
		fileCount++;
		fName = "/geiger/gdata";
		fName += now();
		fName += ".txt";
		lineCount = 0;
	}

	if (fileCount <= MAXFILES)
	{
		if (minutes > ((double)(minHolder + 1)))
		{
			// every minute
			minHolder++;
			lastcSv = ((double)minuteHits) * TUBE_FACTOR;
			lastcSvAvr = lastcSvAvr + lastcSv;	  

			chartvalues[chartvalues_cnt] = ",['" + String(minHolder) + '\'' + ',' + String(minuteHits) + ',' + String(lastcSv) + ']';
			chartvalues_cnt ++;
			if (chartvalues_cnt == MAXCHARTELEMENTS)
			{
				chartvalues_max = true;
				chartvalues_cnt = 0;
			}

			if ((minHolder % FILELOGRATE) == 0)
			{
				lastcSvAvr = lastcSvAvr / ((double)FILELOGRATE);
				if (digitalRead(D2) != 0)
				{
					File f = SPIFFS.open(fName, "a");
					if (!f) 
					{
						Serial.println("file open for append failed");
					}
					else
					{
						Serial.println("====== Writing to SPIFFS file =========");
						f.print(now()); f.print(";"); f.print((int)lastcSvAvr);f.print(","); f.print(((int)(lastcSvAvr * 1000.0)) % 1000); f.print("\n");
						lineCount++;
						f.close();
					}
				}
				lastcSvAvr = 0.0;
			}

			//reset per minute counter
			minuteHits = 0;
		}
	}

	// check to for any web server requests. ie - browser requesting a page from the webserver
	WiFiClient client = server.available();
	if (!client)
	{
		return;
	}

	// wait until the client sends some data
	Serial.println("new client");

	// read the first line of the request
	String request = client.readStringUntil('\r');
	Serial.println(request);

	if (request.indexOf("/REFRESH") != -1) 
	{
		webRefresh = 60;
	}

	if (request.indexOf("/RESET") != -1)
	{  
		// reset session
		resetVariables();
		webRefresh = 0;
	}

	if (request.indexOf("/RESETWIFI") != -1)
	{  
		// reset WIFI settings
		wifiManager.resetSettings();
		delay(1000);
		// reset
		ESP.restart();    
	}

	client.flush();

	// return the response
	client.println("HTTP/1.1 200 OK");

	if (request.indexOf("/CLEAR") != -1) 
	{  
		// clear file contents
		webRefresh = 0;
		Serial.println("Clearing Data");

		Dir dir = SPIFFS.openDir("/geiger");
		while (dir.next())
		{
			if (fName != dir.fileName())
			{
				SPIFFS.remove(dir.fileName());
			}
		}

		client.println("Content-Type: text/html");
		client.println(""); //  do not forget this one
		client.println("<!DOCTYPE HTML>");
		client.println("<html>");
		client.println(" <head>");
		client.println("</head><body>");
		client.println("<h1>Data Files</h1>");
		client.println("<a href=\"/\">Back</a><br><br>");
		client.println("<br><br><div style\"color:red\">Files Cleared</div>");
		client.println("<br><a href=\"/CLEAR\"\"><button>Clear Data</button></a>");
		client.println(" WARNING! This will delete all data.");
		client.println("</body></html>");

		lineCount = 0;
		fileCount = 1;
		return;
	}

	// download file
	if (request.indexOf("/gdata") != -1) 
	{
		String fileDownloadName = request.substring(request.indexOf("/geiger"), request.indexOf("HTTP") - 1) ;

		client.println("Content-Type: text/plain");
		client.print("Content-Disposition: attachment;filename=");
		client.println(fileDownloadName);
		client.println("");

		File fi = SPIFFS.open(fileDownloadName, "r");
		if (!fi) 
		{
			client.println("file open for download failed ");
			client.println(fileDownloadName.substring(1));
		}
		else
		{
			Serial.println("====== Reading from SPIFFS file =======");

			while (fi.available()) 
			{
				// lets read line by line from the file
				String line = fi.readStringUntil('\n');
				client.println(line);
			}
			fi.close();
		}

		return;
	}

	client.println("Content-Type: text/html");
	client.println(""); //  do not forget this one
	client.println("<!DOCTYPE HTML>");
	client.println("<html>");
	client.println("<head>");

	if (request.indexOf("/DATA") != -1)
	{
		client.println("</head><body>");
		client.println("<h1>Data Files</h1>");
		client.println("<a href=\"/\">Back</a><br><br>");
		if (fileCount >= MAXFILES)
		{
			client.println("<div style=\"color:red\">File limit reached. Clear data to restart data logging.</div>");
		}
		client.println("<a href=\"/CLEAR\"\"><button>Clear Data</button></a>");
		client.println(" WARNING! This will delete all data files.<br><br>");

		Dir dir = SPIFFS.openDir("/geiger");
		while (dir.next())
		{
			client.print("<a href=\"");
			client.print(dir.fileName());
			client.print("\">");
			client.print(dir.fileName());
			client.println("</a><br>");
		}

		client.println("</body></html>");
		return;
	}

	if (webRefresh != 0)
	{
		client.print("<meta http-equiv=\"refresh\" content=\"");
		client.print(webRefresh);
		client.println("\">");
	}

	double CPMholder = CPM;
	double uSvholder = uSv;
	int hitHolder = hitCount;
	double minutesHolder = minutes;
	double lastcSvTmp;

	client.println("<script type=\"text/javascript\" src=\"https://www.gstatic.com/charts/loader.js\"></script> ");
	client.println("   <script type=\"text/javascript\"> ");
	client.println("    google.charts.load('current', {'packages':['corechart','gauge']}); ");
	client.println("    google.charts.setOnLoadCallback(drawChart); ");
	client.println("    google.charts.setOnLoadCallback(drawChartG); ");

	// draw low emission uSv gauge
	client.println("   function drawChartG() {");
	client.println("      var data = google.visualization.arrayToDataTable([ ");
	client.println("        ['Label', 'Value'], ");
  client.print  ("        ['\\u00B5Sv/h',  ");
 

	if (lastcSv != 0)
	{
		// minute by minute
		lastcSvTmp = lastcSv;
	}
	else
	{
		// shows the average
		lastcSvTmp = uSvholder;
	}

	client.print(lastcSvTmp);
	client.println(" ], ");
	client.println("       ]); ");
	// setup the google chart options here
	client.println("    var options = {");
	client.println("      width: 700, height: 600,");
	if (lastcSvTmp < 1.0)
	{
		client.println("      min: 0, max: 1,");
		client.println("      greenFrom: 0, greenTo: .25,");
		client.println("      yellowFrom: .25, yellowTo: 1,");
		client.println("      minorTicks: 10,");
		client.println("      majorTicks: [0.0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0]");
	}
	else if (lastcSvTmp < 10.0)
	{
		client.println("      min: 0, max: 10,");
		client.println("      greenFrom: 0, greenTo: .25,");
		client.println("      yellowFrom: .25, yellowTo: 10,");
		client.println("      minorTicks: 10,");
		client.println("      majorTicks: [0,1,2,3,4,5,6,7,8,9,10]");
	}
	else
	{
		client.println("      min: 0, max: 100,");
		client.println("      greenFrom: 0, greenTo: .25,");
		client.println("      yellowFrom: .25, yellowTo: 25,");
		client.println("      redFrom: 25, redTo: 100,");
		client.println("      minorTicks: 10,");
		client.println("      majorTicks: [0,10,20,30,40,50,60,70,80,90,100]");
	}
	client.println("    };");
	client.println("   var chart = new google.visualization.Gauge(document.getElementById('chart_div'));");
	client.println("  chart.draw(data, options);");
	client.println("  }");

	client.println("    function drawChart() { ");
	client.println("     var data = google.visualization.arrayToDataTable([ ");
	client.println("       ['Hit', 'CPM', '\\u00B5Sv/h'] ");

	if (chartvalues_cnt > 0)
	{
		if (chartvalues_max == true)
		{
			for (int i = chartvalues_cnt; i < MAXCHARTELEMENTS; i ++)
			{
				client.println(chartvalues[i]);
			}		  
			for (int i = 0; i < chartvalues_cnt; i ++)
			{
				client.println(chartvalues[i]);
			}		  
		}	  
		else
		{
			for (int i = 0; i < chartvalues_cnt; i ++)
			{
				client.println(chartvalues[i]);
			}		  
		}	
	}
	else 
	{
		// no data lines
		client.print(",['No Data - click Refresh in ");
		client.print(60 - seconds);
		client.println(" seconds',0,0]");
	}

	client.println("     ]); ");
	client.println("     var options = { ");
	client.println("        title: 'Geiger Activity', ");
	client.println("        curveType: 'function', ");
	client.println("  series: {");
	client.println("         0: {targetAxisIndex: 0},");
	client.println("         1: {targetAxisIndex: 1}");
	client.println("       },");
	client.println("  vAxes: { ");
	client.println("         // Adds titles to each axis. ");
	client.println("         0: {title: 'CPM'}, ");
	client.println("         1: {title: '\\u00B5Sv/h'} ");
	client.println("       }, ");
	client.println("  hAxes: { ");
	client.println("         // Adds titles to each axis. ");
	client.println("         0: {title: 'time elapsed (minutes)'}, ");
	client.println("         1: {title: ''} ");
	client.println("       }, ");
	client.println("         legend: { position: 'bottom' } ");
	client.println("       }; ");
	client.println("       var chart = new google.visualization.LineChart(document.getElementById('curve_chart')); ");
	client.println("       chart.draw(data, options); ");
	client.println("      } ");
	client.println("    </script> ");
	client.println("  </head>");
	client.println("  <body>");
	client.println("  <h1>Geiger Counter IoT</h1>");
	if (webRefresh != 0) 
	{
		client.print("<table><tr><td>This page will refresh every ");
		client.print(webRefresh);
		client.print(" seconds ");
	}
	else
	{
		client.print("<div><b style=\"color:red;\">REFRESH NOW</b> ");
	}
	client.println("<a href=\"/REFRESH\"\"><button>Refresh</button></a></td>");
	client.print("<td style=\"text-align:center; width:650px;\">");
	if (fileCount >= MAXFILES)
	{
		client.print("<div style=\"color:red\">File limit reached. Data logging stopped.<br>Click Data Files and Clear Data to restart data logging.</div>");
	}

	client.println("<a href=\"/DATA\"\">Data Files</a></td></tr></table>");
	client.println("<table><tr><td style=\"width:190px; vertical-align:top;\">");
	client.print("<b>Session Details</b>");
	client.println("<br>Hit Count: ");
	client.println(hitHolder);
	client.print("<br>Total Minutes: ");
	client.println(minutesHolder);
	client.print("<br>Avg CPM: <b>");
	client.println(CPMholder);
	client.print("</b><br>Avg &micro;Sv/hr: <b>");
	client.println(uSvholder);
	client.println("</b><br><a href=\"/RESET\"\"><button>Reset Session</button></a>");

	client.println("</td><td>");
	client.println("<div id=\"chart_div\" style=\"width: 250px;\"></div>");
	client.println("</td></tr></table>");

	client.println("<div id=\"curve_chart\" style=\"width: 1000px; height: 500px\"></div>");

	// show remaining data space in filesystem
	FSInfo fs_info;
	SPIFFS.info(fs_info);

	int fileTotalKB = (int)fs_info.totalBytes;
	int fileUsedKB = (int)fs_info.usedBytes;

	client.print("<div>fileTotalBytes: ");
	client.print(fileTotalKB);
	client.print("<br>fileUsedBytes: ");
	client.print(fileUsedKB);

	client.println("</b><br><a href=\"/RESETWIFI\"\"><button>Reset WIFI</button></a>");

	client.println("</body>");
	client.println("</html>");

	Serial.println("Client disonnected");
	Serial.println("");

	yield();
}

