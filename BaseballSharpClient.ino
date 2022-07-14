#include <WiFi.h>

#include <Adafruit_GFX.h>
#include <P3RGB64x32MatrixPanel.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/TomThumb.h>
#include <Fonts/Picopixel.h>
#include <time.h>
#include <ArduinoOTA.h>


#include <HTTPClient.h>
#include <ArduinoJson.h>


#define TZ (-5*60*60) 
#define UPDATE_SECONDS 10

// constructor with default pin wiring
P3RGB64x32MatrixPanel matrix;
int Team_ID = 0;
int Tz = TZ;

#include "Configurator.h"
#include "SoxLogo.h"
// #define SERVER_IP "http://192.168.1.93:5050"
#define SERVER_IP "http://soxmon.azurewebsites.net"


// use this constructor for custom pin wiring instead of the default above
// these pins are an example, you may modify this according to your needs
//P3RGB64x32MatrixPanel matrix(25, 26, 27, 21, 22, 23, 15, 32, 33, 12, 16, 17, 18);




void setup() {
  
  Serial.begin(115200);
  Serial.println("BaseballSharpClient.ino. July 2022. C.Crawford");

  pinMode(TRIGGER_PIN, INPUT_PULLUP);

 // SPIFFS.format();

  GetConfigFromFile();
  Serial.print("Cur teamId: "); Serial.println(teamId);
  char buf[5];
  char* ptr;
  Team_ID = strtol(teamId, &ptr, 10);
  Serial.print("Cur TZ: "); Serial.println(tz);
  Serial.print("Cur Server Address: "); Serial.println(serverAddress);
  
  SetupWifiManager();

wm.setConfigPortalBlocking(true);

  
//  if(wm.autoConnect("AutoConnectAP")) Serial.println("Connected!");
//  else Serial.println("Could not connect.");

  while(!wm.autoConnect("ScoreboardAutoConnectAP"))
  {
    Serial.println("Could not connect...");
    delay(3000);
  }

  Serial.println("Connected!");

//OTA SETUP
 
  ArduinoOTA.setHostname("BaseballSharpClient32");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      matrix.stop();
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();













// wm.setConfigPortalBlocking(false);
  
/*
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
*/

  configTime(TZ, 0, "0.pool.ntp.org", "1.pool.ntp.org"); // enable NTP
  Serial.println("Back from NTP.");

  checkButton();
  
  matrix.begin();                           // setup the LED matrix
}

DynamicJsonDocument doc(3200);

void showError(char* message)
{
  Serial.println(message);
  matrix.fillScreen(0);

  matrix.setTextColor(matrix.color444(15, 15, 15));
  matrix.setFont(&TomThumb);
  matrix.setCursor(1, 6);
  matrix.printf(message);
  matrix.setCursor(1, 20);
  matrix.printf("IP:");

  matrix.printf(WiFi.localIP().toString().c_str());
  matrix.swapBuffer();
  
}

void showMessage(char* message)
{
  Serial.print("ShowMessage:");Serial.println(message);
  matrix.fillScreen(0);

  matrix.setTextColor(matrix.color444(15, 15, 15));
  matrix.setFont(&TomThumb);
  matrix.setCursor(0, 6);
  matrix.printf(message);
  matrix.swapBuffer();
  
}

void ShowSoxLogo()
{
  for(int i = 100; i<200; i++)
  {
  matrix.fillScreen(i);
  matrix.setTextColor(matrix.color444(0,0,0));
  matrix.setCursor(6,6);
  matrix.printf("%i",i);
  matrix.swapBuffer();
  delay(20);
  }
//  matrix.drawRGBBitmap(7,0,(const uint16_t *)soxlogo,49,32);
//  matrix.drawRGBBitmap(0,0,(const uint16_t *)AngelsIcon,32,32);
//  matrix.drawRGBBitmap(32,16,(const uint16_t *)Angels16,16,16);
//  matrix.swapBuffer();
}

int mode=6;
bool liveGameMode = false; 
bool leagueIdle = false;  // League idle is when no games are in progress.
time_t nextGame;          // Time when league goes off idle.


void loop()
{

  static time_t last_t;
  int lastGame, nextUnplayedGame, gameInProgress;
  static int retryCount =0;
  static unsigned long lastMillis = 0;
  static time_t nextUpdateTime = 0;
  static int lastPrimaryTeam = Team_ID;
  bool refreshData = false;

  ArduinoOTA.handle();

  if(wm_nonblocking) wm.process(); // avoid delays() in loop when non-blocking and other long running code  
  if (checkButton()) showError("Started Portal");

  //if (WiFi.status() != WL_CONNECTED)
  //  ESP.restart();
  

  time_t t = time(NULL);
  if (last_t + UPDATE_SECONDS >= t) 
  {     
    // Scroll display if we're not changing pages.
    if (mode == 2 && (millis() - lastMillis) > 60) 
    {
      ScrollStatus();
      lastMillis = millis();
    }
    return;
  }
  
  last_t = t;
  struct tm *lt = localtime(&t);  

  if (t >= nextUpdateTime || Team_ID != lastPrimaryTeam)
  {
    time_t secondsUntilUpdate = GetNextUpdateSeconds();
    nextUpdateTime = t + secondsUntilUpdate;
    refreshData = true;
    lastPrimaryTeam = Team_ID; // Refresh if the primary team changed in the UI.
    Serial.println("Time to update");
  }
  else
  {
    refreshData = false;
    Serial.print("Next update: "); Serial.println(nextUpdateTime - t);
  }

  Serial.print("Mode: "); Serial.println(mode);
  Serial.print("Refresh: "); Serial.println(refreshData);
  

  // TODO: Get us out of Mode 0 after a while.
  static int offset = 0;
    
  switch (mode) {
    case 0: // Broken mode
      Serial.print("In broken mode. Server address: ");
      Serial.println(serverAddress);
      if(retryCount++ > 10){
        mode=1;
        retryCount = 0;
      }
      
      break;    
    case 1: // Previous game result
      lastGame = GetLastGame(Team_ID);
      ShowPrevBoxScore(lastGame, true);
//      ShowFullScreenBoxScore(lastGame);
      mode++;
      break;

    case 2: // Game in progress or about to start
      // ShowFullScreen returns true if we're in a game. stop the rotation.
      gameInProgress = GetCurrentGame(Team_ID);
      liveGameMode = (gameInProgress != 0);
      if (liveGameMode && ShowLiveBoxScore(gameInProgress));
      else mode++;
      break;

    case 3: // Current standings
      ShowFullScreenStandings(Team_ID, refreshData);
      mode++;
      break;
    case 4: // Next game
      ShowNextGameInfo(Team_ID);
        mode++;
      break;
    case 5: 
      char dtStr[32];
      strftime(dtStr, sizeof dtStr, "%Y-%m-%d",lt);
      if(!ShowLeagueGames(dtStr, offset)) //Returns number of rows remaining.
      {
        offset = 0;
        mode++;
      }
      else offset += 4;
      break;
    case 6: //
      ShowSoxLogo();
      mode=1;
      break;
    default:
      mode=1;

  }
  
  return;
 
}

int GetDocFromServer(char* query)
{
  HTTPClient http;
  Serial.print(query);
  http.begin(query);
  int httpResponseCode = http.GET();
  Serial.print("Back from Get: "); Serial.println(httpResponseCode);

  if(httpResponseCode != 200) 
  {
    showError("Couldn't Find Server");
    if(httpResponseCode == -1) 
    {
      Serial.print("The server isn't available or is misconfigured.");
      mode = 0;
    }
    return 1;
  }
  
  String response = http.getString();
  http.end();
  
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
      Serial.println(error.f_str());
      showError("No data back");
      return 1; 
  }

  return 0;
}

int GetJsonFromServer(char* query, DynamicJsonDocument* docPtr)
{
  HTTPClient http;
  Serial.print(query);
  http.begin(query);
  int httpResponseCode = http.GET();
  Serial.print("Back from Get: "); Serial.println(httpResponseCode);

  if(httpResponseCode != 200) 
  {
    showError("Couldn't Find Server");
    if(httpResponseCode == -1) 
    {
      Serial.print("The server isn't available or is misconfigured.");
      mode = 0;
    }
    return 1;
  }
  
  String response = http.getString();
  http.end();
  
  DeserializationError error = deserializeJson(*docPtr, response);
  if (error) {
      Serial.println(error.f_str());
      showError("No data back");
      return 1; 
  }

  return 0;
}

int GetIntFromServer(char* query)
{
  HTTPClient http;
  Serial.print("GIFS: ");
  Serial.println(query);
  http.begin(query);
  int httpResponseCode = http.GET();
  Serial.print("Back from Get: "); Serial.println(httpResponseCode);
  if(httpResponseCode != 200) 
  {
    char errMsg[80];
    sprintf(errMsg, "Srvr error: %d",httpResponseCode);
    showError(errMsg);
    if(httpResponseCode == -1) 
    {
      Serial.print("The server isn't available or is misconfigured.");
      mode = 0;
    }
    return 0;
  }
  
   String response = http.getString();
   Serial.println(response);  
   http.end();
 
   return response.toInt();
 
}

String statusText = "";

bool ScrollStatus()
{
  // if (statusText = "") return false;
  
  static int i = 63;
  int msgLen = statusText.length();
  matrix.setTextWrap(false);
  matrix.setCursor(i,31);
  matrix.writeFillRect(0,25,64,7,0);
  matrix.setTextColor(matrix.color444(15, 15, 15));

  char statusBuf[400];
  statusText.toCharArray(statusBuf, 399);
  matrix.printf(statusBuf);
  i--;
  if(i<-(msgLen*4)) i=63;
  return true;
}

int GetNextUpdateSeconds()
{
  char buf[80];
  sprintf(buf,"%s/api/TimeForNextUpdate",serverAddress);
  int secondsToUpdate = GetIntFromServer(buf);
  Serial.print("Back from TimeForNextUpdate"); Serial.println(secondsToUpdate);
  return secondsToUpdate;
}

int GetLastGame(int teamId)
{
  char buf[80];
  sprintf(buf,"%s/api/GetLastGameId/%d",serverAddress, teamId);
  int gameId = GetIntFromServer(buf);
  Serial.print("Back from GetLastGame "); Serial.println(gameId);
  return gameId;
}

int GetNextGame(int teamId)
{
  char buf[80];
  sprintf(buf,"%s/api/GetNextGameId/%d",serverAddress, teamId);
  int gameId = GetIntFromServer(buf);
  Serial.print("Back from GetNextGame "); Serial.println(gameId);
  return gameId;
}

int GetNextUnplayedGame(int teamId)
{
  char buf[80];
  sprintf(buf,"%s/api/GetNextScheduledGame/%d",serverAddress, teamId);
  int gameId = GetIntFromServer(buf);
  Serial.print("Back from Unplayed Game"); Serial.println(gameId);
  return gameId;
}

int GetCurrentGame(int teamId)
{
  // Service returns 0 if no current game, else return the gamePk
  char buf[80];
  sprintf(buf,"%s/api/GetGameInProgress/%d",serverAddress, teamId);
  int gameId = GetIntFromServer(buf);
  Serial.print("Back from GetCurrentGame "); Serial.println(gameId);
  return gameId;
}


//
// LEAGUE SCOREBOARD
// returns number of games still to show.
//

int ShowLeagueGames(char * dateString, int offset)
{
  static DynamicJsonDocument resultsDoc(3200);
  int retVal = 0;
  static bool gamesInProgress=false;
  static bool gamesAllDone=true;
  static time_t nextGameStart = LONG_MAX; //
  char buf[80];
  sprintf(buf, "%s/api/GetGamesForDate/%s", serverAddress, dateString);
  // GetDocFromServer(buf);
  // If we're on a later page, no need to make another call.
  // if we don't have cached results, get them.
  if(offset == 0 || resultsDoc.isNull()) 
  {  //Save a call or two. TODO: Add games in progress check.
    GetJsonFromServer(buf, &resultsDoc);
  }

  JsonArray arr = resultsDoc.as<JsonArray>();

  matrix.fillScreen(0);
  matrix.setTextColor(matrix.color444(15, 15, 15));
  matrix.setFont(&TomThumb);
  
  int index=0;
  int row = 1;
  int col = 0;
  int colOffset = 32;
    
  for(JsonVariant value : arr) 
  {
    if(index++<offset) continue; //Burn the rows leading to offset.
    if(row>=5) {
      retVal++;
      continue;
    }
    
    const char* awayA = value["awayAbbr"];
    const char* homeA = value["homeAbbr"];
    const char* gameStatus = value["gameStatus"];
    const char* inning = value["inning"];
    int score;
    bool homeWinning = false;
    bool awayWinning = false;
    time_t gameTime = value["gameTimeUnix"];

    // Check to see if scoreboard is live.
    // If there's a game in progress it's live.
    // If no games are live, restart at the time of the first game.
    // If all games are F, no need to update for rest of day.
    if(!strncmp(gameStatus, "I", 1))
    {
      gamesInProgress = true;
    }
    
    if(strncmp(gameStatus, "F", 1)) // If all the games aren't final
    {
      gamesAllDone = false;
    }
    // When's the next game to start.
    if((!strncmp(gameStatus,"P",1) || !strncmp(gameStatus,"S",1)) && gameTime<nextGameStart)
    {
      nextGameStart = gameTime;
    }

    
    
    Serial.print(awayA); Serial.println(homeA);

    if (!strncmp(gameStatus, "F",1) || !strncmp(gameStatus, "I",1))
    {
      if(value["awayTeamRuns"]>value["homeTeamRuns"]) awayWinning = true;
      if(value["awayTeamRuns"]<value["homeTeamRuns"]) homeWinning = true;
    }
    
    if(awayWinning) matrix.setTextColor(matrix.color444(0,15,0));
    else if(homeWinning) matrix.setTextColor(matrix.color444(15,0,0));
    else matrix.setTextColor(matrix.color444(15,15,15));
    matrix.setCursor(0+(col*colOffset), row*6);
    matrix.printf(awayA);

    if(homeWinning) matrix.setTextColor(matrix.color444(0,15,0));
    else if(awayWinning) matrix.setTextColor(matrix.color444(15,0,0));
    else matrix.setTextColor(matrix.color444(15,15,15));
    matrix.setCursor(0+(col*colOffset), (row+1)*6);
    matrix.printf(homeA);

    matrix.setTextColor(matrix.color444(15,15,15));

    int shiftInning = 0;
    if (value["homeTeamRuns"] != nullptr && strncmp(gameStatus, "P", 1)) //Pre-game status has runs, but I don't like to show them.
    {
      matrix.setCursor(15+(col*colOffset),row*6);
      score=value["awayTeamRuns"];
      itoa(score, buf,10);
      matrix.printf(buf);
      matrix.setCursor(15+(col*colOffset),(row+1)*6);
      score=value["homeTeamRuns"];
      itoa(score, buf,10);
      matrix.printf(buf);
      if (value["awayTeamRuns"] > 9 || value["homeTeamRuns"] > 9) shiftInning = 1;
    }

    
    matrix.setCursor(20+(col*colOffset)+(shiftInning*3),(row*6)+3);
    matrix.printf(inning);
    if(col) {
      col=0;
      row+=3;
    }
    else {
      col++;
    }
  }
//  matrix.swapBuffer();

//  sprintf(buf, "%i-%i",value["wins"].as<int>(), value["losses"].as<int>());
  return retVal;
    
}

void ShowNextGameInfo(int teamId)
{
  //  Check to see if next game has changed based on date of next game. refresh as needed.
  // 

  static DynamicJsonDocument doc(512);  // From the json assistant. 
  time_t gameTimeUnix;
  char buf[80];
  sprintf(buf,"%s/api/GetUpcomingGameInfoForTeam/%d",serverAddress, teamId);
  
  if(!doc.isNull()) 
  {
    Serial.println("Document has content.");
    gameTimeUnix = doc["gameTimeUnix"];
    time_t curTime;
    time(&curTime);
    if (curTime > gameTimeUnix) 
    {
      Serial.println("Need update to next game.");
      GetJsonFromServer(buf, &doc);
    }
    else
    {
      Serial.println("Reuse old data.");
      Serial.print("refresh in: ");Serial.print(gameTimeUnix - curTime);
    }
  }
  else 
  {
    Serial.println("First Fetch.");
    GetJsonFromServer(buf, &doc);
  }
  
  const char* awayA = doc["awayAbbr"];
  const char* homeA = doc["homeAbbr"];
  const char* statusB = doc["statusBlurb"];
  const char* gameStatus = doc["gameStatus"];
  const char* homePitcher = doc["homePitcher"];
  const char* awayPitcher = doc["awayPitcher"];
  const char* gameTime = doc["gameTime"];
  gameTimeUnix = doc["gameTimeUnix"];

    
  matrix.fillScreen(0);
  matrix.setTextColor(matrix.color444(15, 15, 15));
  matrix.setFont();
  matrix.setCursor(1, 1);
  matrix.printf("%s @ %s", awayA, homeA);
  
  matrix.setFont(&TomThumb);
  
  matrix.setCursor(3, 16);
  matrix.setTextColor(matrix.color444(1, 15, 1));

  static const char* const wd[7] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
  struct tm *tm; 
  tm = localtime(&gameTimeUnix);
  
  matrix.printf("%s at %d:%02d", wd[tm->tm_wday], tm->tm_hour % 12, tm->tm_min);
  
  matrix.setCursor(1, 25);
  matrix.setTextColor(matrix.color444(0, 15, 8));
  matrix.printf(homePitcher);
  matrix.setCursor(1, 31);
  matrix.setTextColor(matrix.color444(0, 15, 8));
  matrix.printf(awayPitcher);
  
}

void ShowFullScreenStandings(int teamId, bool forceUpdate)
{
  static DynamicJsonDocument doc(1024);
  char buf[80];
  sprintf(buf,"%s/api/GetStandings/%d",serverAddress, teamId);
  // GetDocFromServer(buf);

  if(doc.isNull() || forceUpdate)
  {
    Serial.println("Fetch Standings");
    GetJsonFromServer(buf, &doc);
  }
  else
  {
    Serial.println("Reusing old results");
  }
  
  JsonArray arr = doc.as<JsonArray>();

  matrix.fillScreen(0);
  matrix.setTextColor(matrix.color444(15, 15, 15));
  matrix.setFont(&TomThumb);
  
  int row = 0;

  for(JsonVariant value : arr) 
  {
    if (value["teamId"] == teamId) matrix.setTextColor(matrix.color444(14,0,0));
//    if (!strncmp(value["teamName"].as<char*>(), "CWS", 3)) matrix.setTextColor(matrix.color444(15,0,0));
    else matrix.setTextColor(matrix.color444(15,15,12));
    matrix.setCursor(1, ++row*6);
    matrix.printf(value["divisionRank"].as<char*>());
    matrix.printf(".");
    matrix.setCursor(9, row*6);
    matrix.printf(value["teamName"].as<char*>());
    matrix.setCursor(23, row*6);
    sprintf(buf, "%i-%i",value["wins"].as<int>(), value["losses"].as<int>());
    matrix.printf(buf);
    matrix.setCursor(46, row*6);
    matrix.printf(value["gamesBack"].as<char*>());
  }
}

bool ShowPrevBoxScore(int gamePk, bool forceRefresh)
{
  bool retVal = false;
  char buf[80];
  static DynamicJsonDocument doc(2048);
  
  sprintf(buf,"%s/api/GetBoxScore/%d",serverAddress, gamePk);

  if(doc.isNull() || forceRefresh)
  {
    Serial.println("Fetch BoxScore");
    GetJsonFromServer(buf, &doc);
  }
  else
  {
    Serial.println("Reusing old results");
  }

  DisplayBoxScore(doc, gamePk);
}

bool ShowLiveBoxScore(int gamePk)
{
  bool retVal = false;
  char buf[80];
  static DynamicJsonDocument doc(2048);
  
  sprintf(buf,"%s/api/GetBoxScore/%d",serverAddress, gamePk); 
  GetJsonFromServer(buf, &doc); 
  DisplayBoxScore(doc, gamePk);
}

bool DisplayBoxScore(const JsonDocument& doc, int gamePk)
//bool ShowFullScreenBoxScore(int gamePk) //Return true if we should hold rotation.
{
  bool retVal = false;
  char buf[80];
  //sprintf(buf,"%s/api/GetBoxScore/%d",serverAddress, gamePk);
  //GetDocFromServer(buf);

  const char* awayA = doc["awayAbbr"];
  const char* homeA = doc["homeAbbr"];
  const char* statusB = doc["statusBlurb"];
  const char* awayL = doc["awayLineScore"] | "";
  const char* homeL = doc["homeLineScore"] | "";
  const char* gameStatus = doc["gameStatus"];
  const char* pitcher = doc["pitcher"];
  const char* batter = doc["batter"];
  const char* inningHalf = doc["inningHalf"] | "";
  const char* inningState = doc["inningState"] | "X";

  const char* lastComment = doc["lastComment"];
  
  bool manOnFirst = doc["manOnFirst"];
  bool manOnSecond = doc["manOnSecond"];
  bool manOnThird = doc["manOnThird"];
  
  int awayR = doc["awayteamRunsGame"];
  int homeR = doc["hometeamRunsGame"];
  int awayH = doc["awayteamHitsGame"];
  int homeH = doc["hometeamHitsGame"];
  int awayE = doc["awayteamErrorsGame"];
  int homeE = doc["hometeamErrorsGame"];
  
  char ibuf[3];

  matrix.setTextWrap(false);
  sprintf(buf, "%s @ %s %s", awayA, homeA, statusB);

  matrix.fillScreen(0);

  matrix.setTextColor(matrix.color444(15, 15, 15));
  matrix.setFont(&TomThumb);
  matrix.setCursor(0, 6);

  Serial.println(buf);
  matrix.printf(buf);
  
  matrix.setCursor(0, 16);
  if(!strncmp(gameStatus, "I", 1) && !strncmp(inningHalf, "T", 1)) matrix.setTextColor(matrix.color444(15,12,12));
  else matrix.setTextColor(matrix.color444(15, 4, 4));
  matrix.printf(awayA);
  matrix.setCursor(14,16);
  matrix.printf(awayL);

  matrix.setCursor(0, 22);
  if(!strncmp(gameStatus, "I", 1) && !strncmp(inningHalf, "B", 1)) matrix.setTextColor(matrix.color444(15,12,12));
  else matrix.setTextColor(matrix.color444(15, 4, 4));
  matrix.printf(homeA);

  matrix.setCursor(14,22);
  matrix.printf(homeL);


  // Only print runs, hits, errors if the game is in progress or done. This is getting sloppy.
  
  if(!strncmp(gameStatus, "I",1) || !strncmp(gameStatus, "F",1) || !strncmp(gameStatus, "O",1))
  {
    matrix.setCursor((awayR>9)?49:53, 16);
    matrix.setTextColor(matrix.color444(15,0,0));
    itoa(awayR, ibuf, 10);
    matrix.printf(ibuf);
    matrix.setTextColor(matrix.color444(15,12,12));
    matrix.setCursor((awayH>9)?57:61, 16);
    itoa(awayH, ibuf, 10);
    matrix.printf(ibuf);

    matrix.setTextColor(matrix.color444(15,0,0));
    matrix.setCursor((homeR>9)?49:53,22);
    itoa(homeR, ibuf, 10);
    matrix.printf(ibuf);

    matrix.setTextColor(matrix.color444(15,12,12));
    matrix.setCursor((homeH>9)?57:61, 22);
    itoa(homeH, ibuf, 10);
    matrix.printf(ibuf);
    
  }

  Serial.println(gameStatus);

  if(!strncmp(gameStatus, "I",1))
  {
    if(strncmp(inningState, "E", 1) && strncmp(inningState, "M", 1)) //Not the end or Middle of inning.
    {
      retVal = true; //Only true if in progress and not in a break. TODO figure out if in a break between innings.
    }
    char *ln = strrchr(pitcher, ' ') + 1;
    
    matrix.setTextColor(matrix.color444(7,7,7));
    matrix.setCursor(0,31);

    // matrix.printf(lastComment);
    statusText = lastComment;
    Serial.println(statusText);
/*
    matrix.printf("P:%s",ln);
    char *ab = strrchr(batter, ' ') + 1;
    matrix.printf(" AB:%s", ab);
*/
    
    //Show BaseRunners
    static int emptyBase = matrix.color444(3,8,15);
    static int onBase = matrix.color444(15,0,0);
    
    matrix.drawPixel(61,5,emptyBase);
    matrix.drawPixel(63,3,manOnFirst ? onBase : emptyBase);
    matrix.drawPixel(61,1,manOnSecond ? onBase : emptyBase);
    matrix.drawPixel(59,3,manOnThird ? onBase : emptyBase);

  }
  else if (!strncmp(gameStatus, "F", 1))
  {
    Serial.println("game final");
  }
  matrix.swapBuffer(); // display the image written to the buffer
  
  return retVal;
}
