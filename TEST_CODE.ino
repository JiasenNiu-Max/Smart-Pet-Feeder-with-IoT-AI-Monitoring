/**
 * Smart Pet Feeder - Integrated IoT Pet Feeding System
 * 
 * This project combines ESP32, Blynk IoT platform, and Telegram bot integration
 * to create a comprehensive automated pet feeding system with remote control,
 * weight monitoring, scheduled feeding, and real-time analytics.
 * 
 * MODIFIED: Corrected for 4-sided rotating gate where every 90° rotation dispenses food
 * 
 * Features:
 * - Remote control via Blynk mobile app
 * - Telegram bot integration for voice/text commands
 * - Real-time weight monitoring of pet bowl and food container
 * - Scheduled feeding with customizable times
 * - Manual feeding with user cancellation capability
 * - Data analytics with weight and feeding history charts
 * - Smart alerts for low food levels
 * - Easy calibration system
 * 
 * Hardware Requirements:
 * - ESP32 Development Board
 * - HX711 Weight Sensor Module
 * - Servo Motor (FS5115M or similar) with 4-sided rotating gate
 * - LED Indicator
 * - Food container with dispensing mechanism
 * - Pet bowl
 * 
 * Pin Configuration:
 * - Pin 5: Servo Motor (Signal)
 * - Pin 4: HX711 (DOUT)
 * - Pin 7: HX711 (SCK)
 * - Pin 2: LED Indicator
 */

// ========================== BLYNK CONFIGURATION ==========================
// WARNING: Replace these with your actual credentials before uploading
#define BLYNK_TEMPLATE_ID "TMPL6goH2FXrE"
#define BLYNK_TEMPLATE_NAME "IoT project"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN_HERE"
#define BLYNK_DEVICE_NAME "PetFeeder"
#define BLYNK_PRINT Serial   // Enable Blynk debug information output

// ========================== LIBRARY INCLUDES ==========================
#include <WiFi.h>                    // ESP32 WiFi connectivity
#include <WiFiClient.h>              // WiFi client for regular connections
#include <WiFiClientSecure.h>        // Secure WiFi client for Telegram
#include <BlynkSimpleEsp32.h>        // Blynk IoT platform integration
#include <ESP32Servo.h>              // Servo motor control library
#include <HX711.h>                   // Weight sensor communication
#include <TimeLib.h>                 // Time management functions
#include <WidgetRTC.h>               // Blynk real-time clock widget
#include <UniversalTelegramBot.h>    // Telegram bot integration

// ========================== NETWORK CREDENTIALS ==========================
// WARNING: Replace with your actual WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// WARNING: Replace with your actual Telegram bot token and chat ID
const char* botToken = "YOUR_TELEGRAM_BOT_TOKEN";
// ID of your chat or user (you can set this after first message if unknown)
String chat_id = "YOUR_TELEGRAM_CHAT_ID";

// ========================== HARDWARE PIN DEFINITIONS ==========================
const int SERVO_PIN = 5;        // Servo motor signal pin
const int HX711_DOUT_PIN = 4;   // HX711 weight sensor data pin
const int HX711_SCK_PIN = 7;    // HX711 weight sensor clock pin
const int LED_PIN = 2;          // LED indicator pin for visual feedback

// ========================== OBJECT INSTANTIATION ==========================
Servo gateServo;                         // Servo motor object for food dispensing
HX711 scale;                            // Weight sensor object
WidgetRTC rtc;                          // Blynk real-time clock widget
WiFiClientSecure secured_client;        // Secure client for Telegram communication
UniversalTelegramBot bot(botToken, secured_client);  // Telegram bot object

// ========================== GLOBAL CONFIGURATION VARIABLES ==========================
// Weight sensor calibration (adjust based on your specific setup)
float calibration_factor = -2220.2;     // Calibration factor for accurate weight readings
float currentWeight = 0.0;              // Current weight reading from sensor
float targetFeedAmount = 30.0;          // Default feeding amount in grams
bool feedingInProgress = false;         // Flag to prevent multiple simultaneous feedings

// Feeding schedule configuration (supports up to 7 scheduled feeding times)
int scheduledFeedings[7] = {0};         // Array to store scheduled feeding times
int numScheduledFeedings = 0;           // Current number of active scheduled feedings

// ========================== SERVO CONTROL CONFIGURATION ==========================
// Servo positions for food dispensing mechanism
int servoPositions[] = {0, 90, 0, 90};  // Four preset positions for rotation
int currentPositionIndex = 0;               // Current position index tracker

// ========================== FOOD CONTAINER MANAGEMENT ==========================
float containerCapacity = 500.0;       // Total food container capacity in grams
float containerInitialWeight = 0.0;     // Empty container baseline weight
float totalFedAmount = 0.0;             // Total amount of food dispensed since last refill

// ========================== ANALYTICS AND MONITORING ==========================
// Time tracking variables for data logging
unsigned long lastWeightLogTime = 0;    // Timestamp of last weight data point
unsigned long lastDailyStatsTime = 0;   // Timestamp of last daily statistics update
int feedCount = 0;                      // Number of feedings today
float dailyFeedTotal = 0.0;             // Total amount fed today
int previousDay = 0;                    // Previous day number for date change detection
float lastRecordedWeight = 0.0;         // Last recorded weight for comparison

// ========================== FEEDING HISTORY MANAGEMENT ==========================
#define MAX_FEED_HISTORY 10             // Maximum number of feeding records to store
float feedHistory[MAX_FEED_HISTORY];    // Array storing feeding amounts
unsigned long feedTimes[MAX_FEED_HISTORY]; // Array storing feeding timestamps
int feedHistoryIndex = 0;               // Current index in feeding history

// ========================== CONNECTION MANAGEMENT ==========================
unsigned long lastConnectionCheck = 0;  // Last time connection was checked
bool wasConnected = false;              // Previous Blynk connection status

// ========================== ENHANCED FEEDING CONTROL ==========================
bool userCancelledFeeding = false;     // Flag for user-initiated feeding cancellation
bool isOpen = false;                    // Flag for feeding gate open/close state

// ========================== TELEGRAM BOT CONFIGURATION ==========================
unsigned long lastTelegramCheck = 0;   // Last time Telegram messages were checked
const unsigned long telegramInterval = 3000; // Check Telegram messages every 3 seconds

// ========================== FUNCTION DECLARATIONS ==========================
void blinkLED(int times, int delayMs);
void checkDateChange();
void updateFoodRemaining();
void updateFeedHistory(float amount);
void feedPet(float amount);
void checkScheduledFeedings();
void sendStatusMsg(const String& msg);
void sendHeartbeat();
void updateAllCharts();
void handleTelegramMessages();

// ========================== LED VISUAL FEEDBACK FUNCTION ==========================
/**
 * Blinks the LED indicator for visual feedback
 * @param times - Number of times to blink
 * @param delayMs - Delay between blinks in milliseconds
 */
void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);   // Turn LED on
    delay(delayMs);
    digitalWrite(LED_PIN, LOW);    // Turn LED off
    delay(delayMs);
  }
}

// ========================== HEARTBEAT AND CONNECTION MAINTENANCE ==========================
/**
 * Sends periodic heartbeat data to maintain active Blynk connection
 * and keeps chart widgets active with regular data updates
 */
void sendHeartbeat() {
  // Send system uptime as heartbeat indicator
  Blynk.virtualWrite(V4, "Heartbeat: " + String(millis() / 1000) + "s");
  
  // Send current weight to keep weight chart active
  Blynk.virtualWrite(V9, currentWeight);
  
  // Periodically send zero-value data points to feeding chart to prevent timeout
  static unsigned long lastFeedHeartbeat = 0;
  if (millis() - lastFeedHeartbeat > 300000) { // Every 5 minutes
    Blynk.virtualWrite(V10, 0);
    lastFeedHeartbeat = millis();
  }
}

// ========================== CHART DATA UPDATE FUNCTION ==========================
/**
 * Forces update of all chart widgets with current data
 * Called after reconnection to ensure charts display correct information
 */
void updateAllCharts() {
  Serial.println("Force updating all chart data...");
  
  // Update weight history chart
  Blynk.virtualWrite(V9, currentWeight);
  Serial.println("Sent weight data to V9: " + String(currentWeight) + "g");
  
  // Update feeding history chart (send last feeding amount or 0)
  float lastFeedAmount = (feedHistoryIndex > 0) ? feedHistory[0] : 0;
  Blynk.virtualWrite(V10, lastFeedAmount);
  Serial.println("Sent feeding data to V10: " + String(lastFeedAmount) + "g");
  
  // Update daily feeding total chart
  Blynk.virtualWrite(V11, dailyFeedTotal);
  Serial.println("Sent daily feeding total to V11: " + String(dailyFeedTotal) + "g");
}

// ========================== TELEGRAM MESSAGE HANDLER ==========================
/**
 * Processes incoming Telegram messages and executes voice/text commands
 * Supports commands: "feed", "open", "close"
 */
void handleTelegramMessages() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  
  while (numNewMessages) {
    Serial.println("Processing Telegram messages...");
    
    for (int i = 0; i < numNewMessages; i++) {
      String text = bot.messages[i].text;
      chat_id = bot.messages[i].chat_id;
      
      Serial.println("Received command: " + text);
      
      // Process voice/text commands
      if (text == "feed") {
        // Trigger feeding sequence
        if (!feedingInProgress) {
          sendStatusMsg("Telegram feed command received");
          isOpen = true;
          feedPet(targetFeedAmount);
        } else {
          bot.sendMessage(chat_id, "Feeding already in progress. Please wait.", "");
        }
      } 
      else if (text == "open") {
        // Open feeding gate
        gateServo.write(90);
        isOpen = true;
        sendStatusMsg("Gate opened via Telegram");
        bot.sendMessage(chat_id, "Feeding gate opened", "");
        Serial.println(">> Open command executed");
      } 
      else if (text == "close") {
        // Close feeding gate
        gateServo.write(0);
        isOpen = false;
        sendStatusMsg("Gate closed via Telegram");
        bot.sendMessage(chat_id, "Feeding gate closed", "");
        Serial.println(">> Close command executed");
      }
      else if (text == "status") {
        // Send current system status
        String statusReport = "Pet Feeder Status:\n";
        statusReport += "Weight: " + String(currentWeight, 1) + "g\n";
        statusReport += "Food remaining: " + String((int)((containerCapacity - totalFedAmount) / containerCapacity * 100)) + "%\n";
        statusReport += "Today's feeds: " + String(feedCount) + " times\n";
        statusReport += "Daily total: " + String(dailyFeedTotal, 1) + "g";
        bot.sendMessage(chat_id, statusReport, "");
      }
      else {
        // Unknown command response
        bot.sendMessage(chat_id, "Unknown command. Available: feed, open, close, status", "");
      }
    }
    
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

// ========================== MAIN SETUP FUNCTION ==========================
void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Smart Pet Feeder Starting ===");
  
  // ========================== HARDWARE INITIALIZATION ==========================
  // Initialize LED indicator
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED initialized");
  
  // Initialize servo motor
  Serial.print("Initializing servo motor...");
  gateServo.attach(SERVO_PIN);
  gateServo.write(servoPositions[currentPositionIndex]);  // Set to initial position (0°)
  delay(500);  // Allow servo to reach position
  Serial.println("Done");
  
  // Initialize weight sensor
  Serial.print("Initializing weight sensor...");
  scale.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare();                // Zero out the scale
  containerInitialWeight = 0;  // Set baseline container weight
  Serial.println("Done");
  
  // ========================== NETWORK CONNECTION ==========================
  // Connect to WiFi network
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection FAILED!");
    Serial.println("Will retry connection in main loop...");
  }
  
  // ========================== BLYNK CONNECTION ==========================
  // Connect to Blynk IoT platform
  Serial.print("Connecting to Blynk...");
  Blynk.config(BLYNK_AUTH_TOKEN);  // Configure Blynk with auth token
  bool blynkConnected = Blynk.connect(10000);  // 10 second connection timeout
  
  if (blynkConnected) {
    Serial.println("Connected!");
  } else {
    Serial.println("Connection FAILED! Will retry in loop...");
  }
  
  // ========================== TELEGRAM BOT INITIALIZATION ==========================
  // Configure secure client for Telegram API
  secured_client.setInsecure();  // Use insecure mode for simplicity
  Serial.println("Telegram bot initialized");
  
  // ========================== TIME SYNCHRONIZATION ==========================
  // Initialize real-time clock widget
  rtc.begin();
  setSyncInterval(10 * 60);  // Sync time every 10 minutes
  Serial.println("RTC synchronized");
  
  // ========================== INITIAL DATA SETUP ==========================
  // Record initial weight reading
  currentWeight = scale.get_units(5);  // Average of 5 readings for accuracy
  if (currentWeight < 0) currentWeight = 0;  // Ensure non-negative values
  lastRecordedWeight = currentWeight;
  
  // Set initial date for daily statistics
  previousDay = day();
  
  // ========================== FEEDING HISTORY INITIALIZATION ==========================
  // Clear feeding history arrays
  for (int i = 0; i < MAX_FEED_HISTORY; i++) {
    feedHistory[i] = 0;
    feedTimes[i] = 0;
  }
  
  // ========================== BLYNK INTERFACE INITIALIZATION ==========================
  // Send initial values to Blynk dashboard
  if (Blynk.connected()) {
    Serial.println("Sending initial data to Blynk dashboard...");
    
    // Initialize weight display
    Blynk.virtualWrite(V2, currentWeight);
    
    // Initialize food remaining gauge (100% full)
    Blynk.virtualWrite(V7, 100);
    
    // Initialize chart widgets with initial data
    Blynk.virtualWrite(V9, currentWeight);  // Weight history chart
    Blynk.virtualWrite(V10, 0);             // Feed history chart
    Blynk.virtualWrite(V11, 0);             // Daily feed total chart
    
    delay(500);  // Ensure data transmission
    
    // Synchronize all widget states
    Blynk.syncAll();
    Serial.println("Initial dashboard data sent");
  }
  
  // ========================== STARTUP COMPLETION ==========================
  // Visual indication of successful startup
  blinkLED(3, 200);  // Blink LED 3 times
  sendStatusMsg("System started successfully - Ready for operation");
  Serial.println("=== Initialization Complete ===");
  Serial.println("System ready for operation!");
}

// ========================== MAIN LOOP FUNCTION ==========================
void loop() {
  // ========================== CONNECTION MANAGEMENT ==========================
  // Monitor and maintain WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost, attempting reconnection...");
    WiFi.begin(ssid, password);
    delay(5000);  // Wait for connection attempt
  }
  
  // Monitor and maintain Blynk connection
  if (!Blynk.connected()) {
    if (wasConnected) {
      Serial.println("Blynk connection lost, attempting reconnection...");
      wasConnected = false;
    }
    
    // Only attempt Blynk reconnection if WiFi is available
    if (WiFi.status() == WL_CONNECTED) {
      bool reconnected = Blynk.connect(5000);  // 5 second timeout
      if (reconnected) {
        Serial.println("Reconnected to Blynk successfully!");
        wasConnected = true;
        updateAllCharts();  // Refresh all chart data after reconnection
      }
    }
  } else {
    wasConnected = true;
    Blynk.run();  // Process Blynk events
    
    // Send periodic heartbeat to maintain connection
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 30000) {  // Every 30 seconds
      sendHeartbeat();
      lastHeartbeat = millis();
    }
  }
  
  // ========================== WEIGHT MONITORING ==========================
  // Regular weight sensor reading and data transmission
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {  // Update every second
    // Read current weight from sensor
    currentWeight = scale.get_units(5);  // Average of 5 readings
    if (currentWeight < 0) currentWeight = 0;  // Ensure non-negative
    
    // Send weight data to Blynk dashboard
    if (Blynk.connected()) {
      Blynk.virtualWrite(V2, currentWeight);
    }
    
    // Debug output
    Serial.print("Current weight: ");
    Serial.print(currentWeight, 1);
    Serial.println(" g");
    
    // Log weight data for history chart (every 30 seconds)
    if (millis() - lastWeightLogTime > 30000) {
      if (Blynk.connected()) {
        Blynk.virtualWrite(V9, currentWeight);
        Serial.println("Weight history recorded: " + String(currentWeight) + "g");
      }
      lastWeightLogTime = millis();
    }
    
    // Update calculated values
    updateFoodRemaining();  // Calculate food remaining percentage
    checkDateChange();      // Check if day changed for daily statistics reset
    
    // Send system status
    if (Blynk.connected()) {
      sendStatusMsg("active");
    }
    
    lastUpdate = millis();
  }
  
  // ========================== SCHEDULED FEEDING CHECK ==========================
  // Check for scheduled feeding times
  static unsigned long lastScheduleCheck = 0;
  if (millis() - lastScheduleCheck > 30000) {  // Check every 30 seconds
    checkScheduledFeedings();
    lastScheduleCheck = millis();
  }
  
  // ========================== TELEGRAM MESSAGE PROCESSING ==========================
  // Process incoming Telegram messages
  if (millis() - lastTelegramCheck > telegramInterval) {
    handleTelegramMessages();
    lastTelegramCheck = millis();
  }
}

// ========================== DATE CHANGE DETECTION ==========================
/**
 * Detects when the date changes and resets daily statistics
 * This ensures accurate daily feeding tracking
 */
void checkDateChange() {
  int currentDay = day();
  
  // Check if day has changed
  if (currentDay != previousDay) {
    // Log previous day's statistics
    String statsMsg = "Day changed - Previous day: " + String(feedCount) + 
                     " feeds, total " + String(dailyFeedTotal) + "g";
    Serial.println(statsMsg);
    sendStatusMsg(statsMsg);
    
    // Reset daily statistics for new day
    feedCount = 0;
    dailyFeedTotal = 0.0;
    previousDay = currentDay;
    
    // Reset daily chart data
    if (Blynk.connected()) {
      Blynk.virtualWrite(V11, 0);
    }
  }
}

// ========================== FOOD REMAINING CALCULATION ==========================
/**
 * Calculates and updates the food remaining percentage
 * Sends low food warnings when necessary
 */
void updateFoodRemaining() {
  // Calculate remaining food in container
  float foodRemaining = containerCapacity - totalFedAmount;
  if (foodRemaining < 0) foodRemaining = 0;
  
  // Calculate percentage remaining
  int percentRemaining = (int)((foodRemaining / containerCapacity) * 100);
  if (percentRemaining > 100) percentRemaining = 100;
  
  // Low food warning system
  static bool lowFoodWarningTriggered = false;
  if (percentRemaining < 20 && !lowFoodWarningTriggered) {
    String warningMsg = "LOW FOOD WARNING: Only " + String(percentRemaining) + "% remaining!";
    sendStatusMsg(warningMsg);
    // Send Telegram notification if available
    if (WiFi.status() == WL_CONNECTED && chat_id != "") {
      bot.sendMessage(chat_id, warningMsg, "");
    }
    lowFoodWarningTriggered = true;
  } else if (percentRemaining > 30) {
    lowFoodWarningTriggered = false;  // Reset warning when food is refilled
  }
  
  // Update Blynk food remaining gauge
  if (Blynk.connected()) {
    Blynk.virtualWrite(V7, percentRemaining);
  }
  
  // Debug information
  Serial.print("Food remaining: ");
  Serial.print(foodRemaining, 1);
  Serial.print("g (");
  Serial.print(percentRemaining);
  Serial.println("%)");
}

// ========================== FEEDING HISTORY MANAGEMENT ==========================
/**
 * Updates feeding history arrays and daily statistics
 * Sends data to Blynk charts for visualization
 * @param amount - Amount of food dispensed in grams
 */
void updateFeedHistory(float amount) {
  // Update total amount dispensed from container
  totalFedAmount += amount;
  if (totalFedAmount > containerCapacity) totalFedAmount = containerCapacity;
  
  // Shift existing history to make room for new entry
  for (int i = MAX_FEED_HISTORY - 1; i > 0; i--) {
    feedHistory[i] = feedHistory[i-1];
    feedTimes[i] = feedTimes[i-1];
  }
  
  // Add new feeding record
  feedHistory[0] = amount;
  feedTimes[0] = now();  // Current timestamp
  
  // Update daily statistics
  feedCount++;
  dailyFeedTotal += amount;
  
  // Send data to Blynk charts
  if (Blynk.connected()) {
    // Send feeding data multiple times to ensure chart recording
    for (int i = 0; i < 3; i++) {
      Blynk.virtualWrite(V10, amount);  // Feed history chart
      delay(100);
    }
    Blynk.virtualWrite(V11, dailyFeedTotal);  // Daily total chart
    
    Serial.println("Feeding data sent - Amount: " + String(amount) + "g, Daily total: " + String(dailyFeedTotal) + "g");
  } else {
    Serial.println("Blynk not connected - feeding data not recorded to charts");
  }
  
  // Update food remaining display
  updateFoodRemaining();
  
  // Send Telegram notification
  if (WiFi.status() == WL_CONNECTED && chat_id != "") {
    String feedMsg = "Fed " + String(amount, 1) + "g. Today's total: " + String(dailyFeedTotal, 1) + "g";
    bot.sendMessage(chat_id, feedMsg, "");
  }
}

// ========================== MAIN FEEDING FUNCTION ==========================
/**
 * Executes the pet feeding sequence with precise weight monitoring
 * CORRECTED FOR 4-SIDED ROTATING GATE: Every 90° rotation dispenses food
 * @param amount - Target total weight to achieve in bowl (grams)
 */
void feedPet(float amount) {
  // Prevent multiple simultaneous feeding operations
  if (feedingInProgress) {
    Serial.println("Feeding already in progress - ignoring request");
    return;
  }
  
  // Initialize feeding sequence
  feedingInProgress = true;
  userCancelledFeeding = false;
  Serial.println("=== Starting Feeding Sequence ===");
  sendStatusMsg("Starting feeding sequence...");
  
  // Record initial weight
  float initialWeight = scale.get_units(10);
  Serial.print("Initial bowl weight: ");
  Serial.print(initialWeight, 1);
  Serial.println("g");
  sendStatusMsg("Initial weight: " + String(initialWeight, 1) + "g");
  
  // ========================== PRE-FEEDING CHECK ==========================
  if (initialWeight >= amount * 0.9) {
    Serial.println("Bowl already contains sufficient food!");
    String statusMsg = "Target already achieved: " + String(initialWeight, 1) + "g >= " + String(amount, 1) + "g";
    sendStatusMsg(statusMsg);
    
    blinkLED(2, 100);
    feedingInProgress = false;
    isOpen = false;
    
    if (Blynk.connected()) {
      Blynk.virtualWrite(V1, 0);
    }
    
    updateFeedHistory(0.0);
    Serial.println("=== No Feeding Required ===");
    return;
  }
  
  // Calculate needed amount
  float neededAmount = amount - initialWeight;
  if (neededAmount < 0) neededAmount = 0;
  
  Serial.print("Food needed to reach target: ");
  Serial.print(neededAmount, 1);
  Serial.println("g");
  sendStatusMsg("Need to add: " + String(neededAmount, 1) + "g to reach " + String(amount, 1) + "g");
  
  // Visual feedback
  digitalWrite(LED_PIN, HIGH);
  
  // ========================== FEEDING LOOP ==========================
  unsigned long startTime = millis();
  bool timeoutOccurred = false;
  int rotationCount = 0;
  
  // Record current position (don't assume starting position)
  int currentAngle = servoPositions[currentPositionIndex];
  Serial.println("Starting from position: " + String(currentAngle) + " degrees");
  
  // Main feeding loop - each 90° rotation dispenses food
  while (millis() - startTime < 15000) {  // 15-second safety timeout
    
    // Check for user cancellation
    if (userCancelledFeeding) {
      Serial.println("Feeding cancelled by user");
      sendStatusMsg("Feeding cancelled by user");
      break;
    }
    
    // Check isOpen flag
    if (!isOpen && rotationCount > 0) {
      Serial.println("Feeding stopped - gate closed");
      sendStatusMsg("Feeding stopped - gate closed");
      break;
    }
    
    // ========================== SINGLE 90° ROTATION ==========================
    // Move to next position (every 90° dispenses food)
    currentPositionIndex = (currentPositionIndex + 1) % 4;  // Cycle through 0,1,2,3
    int nextPosition = servoPositions[currentPositionIndex];
    
    Serial.println("Rotation " + String(rotationCount + 1) + ": " + 
                   String(currentAngle) + " -> " + String(nextPosition) + " degrees");
    
    gateServo.write(nextPosition);
    rotationCount++;
    currentAngle = nextPosition;
    
    sendStatusMsg("Rotation " + String(rotationCount) + ": " + String(nextPosition) + " degrees (Food dispensing...)");
    
    // ========================== WAIT FOR FOOD TO DISPENSE ==========================
    // Give time for food to fall and weight to stabilize
    delay(1500);  // Increased delay for food to fully dispense and settle
    
    // ========================== WEIGHT CHECK AFTER EACH 90° ==========================
    float currentBowlWeight = scale.get_units(5);
    float totalDispensedAmount = currentBowlWeight - initialWeight;
    
    Serial.print("After rotation " + String(rotationCount) + ": Bowl weight = ");
    Serial.print(currentBowlWeight, 1);
    Serial.print("g (dispensed: ");
    Serial.print(totalDispensedAmount, 1);
    Serial.println("g)");
    
    // ========================== TARGET CHECK ==========================
    // Check if total bowl weight has reached target
    if (currentBowlWeight >= amount * 0.9) {
      Serial.println("Target total weight reached after " + String(rotationCount) + " rotations!");
      sendStatusMsg("Target achieved: " + String(currentBowlWeight, 1) + "g >= " + String(amount, 1) + "g");
      break;
    }
    
    // Alternative check: sufficient food dispensed
    if (totalDispensedAmount >= neededAmount * 0.9) {
      Serial.println("Sufficient food dispensed after " + String(rotationCount) + " rotations!");
      sendStatusMsg("Dispensed enough: " + String(totalDispensedAmount, 1) + "g");
      break;
    }
    
    // Progress update
    float progressPercent = (currentBowlWeight / amount) * 100;
    if (progressPercent > 100) progressPercent = 100;
    sendStatusMsg("Progress: " + String(progressPercent, 1) + "% (" + 
                  String(currentBowlWeight, 1) + "g/" + String(amount, 1) + "g)");
    
    // Safety check - prevent excessive rotations
    if (rotationCount >= 12) {  // Increased limit since each rotation is smaller
      timeoutOccurred = true;
      sendStatusMsg("WARNING: Maximum rotations reached (" + String(rotationCount) + ")");
      Serial.println("Maximum rotations reached - stopping for safety");
      break;
    }
  }
  
  // ========================== FEEDING COMPLETION ==========================
  if (millis() - startTime >= 15000) {
    timeoutOccurred = true;
    sendStatusMsg("WARNING: Feeding timeout occurred!");
    Serial.println("Feeding timeout - 15 seconds elapsed");
  }
  
  // Turn off LED
  digitalWrite(LED_PIN, LOW);
  
  // Final measurement
  delay(500);
  float finalWeight = scale.get_units(10);
  float actualDispensedAmount = finalWeight - initialWeight;
  if (actualDispensedAmount < 0) actualDispensedAmount = 0;
  
  // ========================== RESULT EVALUATION ==========================
  String statusMsg;
  bool targetAchieved = (finalWeight >= amount * 0.9);
  
  if (userCancelledFeeding) {
    statusMsg = "Cancelled after " + String(rotationCount) + " rotations: " + 
                String(finalWeight, 1) + "g total (" + String(actualDispensedAmount, 1) + "g added)";
  } else if (timeoutOccurred) {
    if (targetAchieved) {
      statusMsg = "Timeout but target reached: " + String(finalWeight, 1) + "g/" + String(amount, 1) + "g";
    } else {
      statusMsg = "Incomplete after " + String(rotationCount) + " rotations: " + 
                  String(finalWeight, 1) + "g/" + String(amount, 1) + "g";
    }
  } else {
    if (targetAchieved) {
      statusMsg = "Success in " + String(rotationCount) + " rotations: " + 
                  String(finalWeight, 1) + "g achieved (+" + String(actualDispensedAmount, 1) + "g)";
    } else {
      statusMsg = "Under-target after " + String(rotationCount) + " rotations: " + 
                  String(finalWeight, 1) + "g/" + String(amount, 1) + "g";
    }
  }
  
  sendStatusMsg(statusMsg);
  
  Serial.println("=== Feeding Complete ===");
  Serial.println("Total rotations: " + String(rotationCount));
  Serial.println("Final position: " + String(servoPositions[currentPositionIndex]) + " degrees");
  Serial.print("Final bowl weight: ");
  Serial.print(finalWeight, 1);
  Serial.print("g (target: ");
  Serial.print(amount, 1);
  Serial.print("g, dispensed: ");
  Serial.print(actualDispensedAmount, 1);
  Serial.println("g)");
  
  // Update feeding history
  if (actualDispensedAmount > 0.5) {
    updateFeedHistory(actualDispensedAmount);
  } else if (actualDispensedAmount <= 0.5 && initialWeight >= amount * 0.9) {
    updateFeedHistory(0.0);
  }
  
  // Reset feeding flags
  feedingInProgress = false;
  isOpen = false;
  
  if (Blynk.connected()) {
    Blynk.virtualWrite(V1, 0);
  }
  
  Serial.println("=== Feeding Sequence Complete ===");
}

// ========================== SCHEDULED FEEDING CHECKER ==========================
/**
 * Checks current time against scheduled feeding times
 * Automatically triggers feeding when scheduled time is reached
 */
void checkScheduledFeedings() {
  static int lastFedTime = -1;  // Prevent multiple feeds in same minute
  
  // Skip if no scheduled feedings are set
  if (numScheduledFeedings == 0) return;
  
  // Get current time in minutes since midnight
  int currentHour = hour();
  int currentMinute = minute();
  int currentTimeInMinutes = currentHour * 60 + currentMinute;
  
  // Check each scheduled feeding time
  for (int i = 0; i < numScheduledFeedings; i++) {
    int scheduledTime = scheduledFeedings[i];
    
    // Trigger feeding if time matches and hasn't fed this minute
    if (currentTimeInMinutes == scheduledTime && lastFedTime != currentTimeInMinutes) {
      Serial.println("Scheduled feeding time reached!");
      sendStatusMsg("Automatic feeding triggered");
      
      // Send Telegram notification
      if (WiFi.status() == WL_CONNECTED && chat_id != "") {
        bot.sendMessage(chat_id, "Scheduled feeding started", "");
      }
      
      isOpen = true;  // Enable feeding
      feedPet(targetFeedAmount);
      lastFedTime = currentTimeInMinutes;
      
      break;  // Only trigger one feeding per minute
    }
  }
}

// ========================== STATUS MESSAGE FUNCTION ==========================
/**
 * Sends status messages to both Blynk app and serial monitor
 * @param msg - Message string to send
 */
void sendStatusMsg(const String& msg) {
  // Send to Blynk terminal/label widget
  if (Blynk.connected()) {
    Blynk.virtualWrite(V4, msg);
  }
  
  // Output to serial monitor for debugging
  Serial.println("[STATUS] " + msg);
}

// ========================== BLYNK VIRTUAL PIN HANDLERS ==========================

/**
 * V1: Manual Feed Button Handler
 * Supports both feeding initiation and cancellation
 */
BLYNK_WRITE(V1) {
  int buttonState = param.asInt();
  
  if (buttonState == 1) {
    // Button pressed ON - start feeding if not already in progress
    if (!feedingInProgress) {
      sendStatusMsg("Manual feeding triggered from app");
      isOpen = true;
      feedPet(targetFeedAmount);
    } else {
      sendStatusMsg("Feeding already in progress");
    }
  } 
  else if (buttonState == 0) {
    // Button pressed OFF - cancel feeding if in progress
    if (feedingInProgress) {
      userCancelledFeeding = true;
      isOpen = false;
      sendStatusMsg("Feeding cancellation requested");
      Serial.println("User requested feeding cancellation via app");
    }
  }
}

/**
 * V3: Feed Amount Slider Handler
 * Sets the target feeding amount in grams
 */
BLYNK_WRITE(V3) {
  targetFeedAmount = param.asFloat();
  
  // Validate feeding amount within reasonable range
  if (targetFeedAmount < 5) targetFeedAmount = 5;      // Minimum 5g
  if (targetFeedAmount > 200) targetFeedAmount = 200;   // Maximum 200g
  
  Serial.print("Feed amount set to: ");
  Serial.print(targetFeedAmount);
  Serial.println("g");
  
  String statusMsg = "Feed amount set: " + String(targetFeedAmount) + "g";
  sendStatusMsg(statusMsg);
}

/**
 * V5: Calibration Button Handler
 * Performs weight sensor calibration (tare operation)
 */
BLYNK_WRITE(V5) {
  if (param.asInt() == 1) {
    // Perform scale tare operation
    Serial.println("Starting weight sensor calibration...");
    sendStatusMsg("Calibrating weight sensor...");
    
    scale.tare();  // Zero out the scale
    containerInitialWeight = 0;  // Reset container baseline
    
    Serial.println("Weight sensor calibrated successfully");
    sendStatusMsg("Weight sensor calibrated");
    
    // Visual feedback
    blinkLED(3, 100);
    
    // Read and display current weight
    currentWeight = scale.get_units(5);
    if (currentWeight < 0) currentWeight = 0;
    
    if (Blynk.connected()) {
      Blynk.virtualWrite(V2, currentWeight);
    }
    
    // Update food remaining calculation
    updateFoodRemaining();
  }
}

/**
 * V6: Scheduled Feeding Time Menu Handler
 * Sets scheduled feeding time using minutes since midnight
 */
BLYNK_WRITE(V6) {
  int timeInMinutes = param.asInt();
  numScheduledFeedings = 0;  // Reset existing schedules
  
  if (timeInMinutes > 0) {
    // Convert minutes to hours and minutes
    int feedHour = timeInMinutes / 60;
    int feedMinute = timeInMinutes % 60;
    
    // Store scheduled time
    scheduledFeedings[0] = timeInMinutes;
    numScheduledFeedings = 1;
    
    // Format time for display
    char timeBuffer[10];
    sprintf(timeBuffer, "%02d:%02d", feedHour, feedMinute);
    
    String statusMsg = "Schedule set: " + String(timeBuffer);
    sendStatusMsg(statusMsg);
    Serial.println("Scheduled feeding time set: " + String(timeBuffer));
    
    // Send Telegram confirmation
    if (WiFi.status() == WL_CONNECTED && chat_id != "") {
      bot.sendMessage(chat_id, "Feeding scheduled for " + String(timeBuffer), "");
    }
  } else {
    sendStatusMsg("No feeding schedule active");
    Serial.println("Feeding schedule cleared");
  }
}

/**
 * V12: Clear History Button Handler
 * Clears all feeding history and resets statistics
 */
BLYNK_WRITE(V12) {
  if (param.asInt() == 1) {
    Serial.println("Clearing feeding history and statistics...");
    sendStatusMsg("Clearing feeding history...");
    
    // Clear feeding history arrays
    for (int i = 0; i < MAX_FEED_HISTORY; i++) {
      feedHistory[i] = 0;
      feedTimes[i] = 0;
    }
    
    // Reset daily statistics
    feedCount = 0;
    dailyFeedTotal = 0.0;
    
    // Reset total fed amount
    totalFedAmount = 0.0;
    
    // Reset chart data
    if (Blynk.connected()) {
      Blynk.virtualWrite(V10, 0);  // Clear feed history chart
      Blynk.virtualWrite(V11, 0);  // Clear daily total chart
    }
    
    // Update food remaining display
    updateFoodRemaining();
    
    // Visual feedback
    blinkLED(3, 100);
    
    sendStatusMsg("History and statistics cleared");
    Serial.println("All feeding data cleared successfully");
  }
}

/**
 * V13: Reset Food Container Button Handler
 * Resets the food container to full capacity
 */
BLYNK_WRITE(V13) {
  if (param.asInt() == 1) {
    Serial.println("Resetting food container to full...");
    sendStatusMsg("Resetting food container...");
    
    // Reset total fed amount (marks container as full)
    totalFedAmount = 0.0;
    
    // Update food remaining display
    updateFoodRemaining();
    
    // Visual feedback
    blinkLED(3, 100);
    
    sendStatusMsg("Food container marked as refilled");
    Serial.println("Food container reset to full capacity");
    
    // Send Telegram notification
    if (WiFi.status() == WL_CONNECTED && chat_id != "") {
      bot.sendMessage(chat_id, "Food container refilled and reset", "");
    }
  }
}

// ========================== BLYNK CONNECTION EVENT HANDLER ==========================
/**
 * Called when Blynk connection is established
 * Synchronizes all widget states and refreshes data
 */
BLYNK_CONNECTED() {
  Serial.println("Successfully connected to Blynk server!");
  sendStatusMsg("Connected to Blynk server");
  
  // Initialize real-time clock
  rtc.begin();
  
  // Refresh all chart data after connection
  updateAllCharts();
  
  // Synchronize all widget states with current values
  Blynk.syncAll();
  
  Serial.println("All dashboard widgets synchronized");
}

// ========================== END OF CODE ==========================
