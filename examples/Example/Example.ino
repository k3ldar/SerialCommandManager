#include <Arduino.h>
#include "SerialCommandManager.h"

// ------------------------------
// Example command handlers
// ------------------------------

// DEBUG command handler
void handleDebug(const String& command, StringKeyValue* params, int paramCount, void* context)
{
    // remove warning for unused params
    (void)command;
    (void)context;
    
    if (paramCount > 0 && params[0].key.equalsIgnoreCase("ON"))
        Serial.println("Debug mode ENABLED");
    else if (paramCount > 0 && params[0].key.equalsIgnoreCase("OFF"))
        Serial.println("Debug mode DISABLED");
}

// LED command handler
void handleLED(const String& command, StringKeyValue* params, int paramCount, void* context)
{
    // remove warning for unused params
    (void)command;
    (void)context;

    int pin = 13;
    bool state = false;

    for (int i = 0; i < paramCount; i++)
    {
        if (params[i].key.equalsIgnoreCase("pin")) pin = params[i].value.toInt();
        if (params[i].key.equalsIgnoreCase("state")) state = params[i].value.equalsIgnoreCase("ON");
    }

    pinMode(pin, OUTPUT);
    digitalWrite(pin, state ? HIGH : LOW);

    Serial.print("LED command processed: pin=");
    Serial.print(pin);
    Serial.print(", state=");
    Serial.println(state ? "ON" : "OFF");
}

// PING command handler
void handlePing(const String& command, StringKeyValue* params, int paramCount, void* context)
{
    // remove warning for unused params
    (void)command;
    (void)params;
    (void)paramCount;
    (void)context;

   Serial.println("PONG;");
}

// Fallback handler for unrecognized commands
void handleUnknown(SerialCommandManager* mgr)
{
    Serial.print("Unknown command received: ");
    Serial.println(mgr->getCommand());
    Serial.print("Raw message: ");
    Serial.println(mgr->getRawMessage());
}

// ------------------------------
// Create SerialCommandManager
// ------------------------------
SerialCommandManager commandMgr(
    &Serial,          // Serial port
    handleUnknown,    // Default fallback handler
    '\n',             // Message terminator
    ':',              // Command/parameter separator
    '=',              // Key/value separator
    500,              // Timeout in ms
    256               // Maximum message size
);

// ------------------------------
// Setup
// ------------------------------
void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println("SerialCommandManager Example Started");

    // Register command handlers
    commandMgr.registerHandler("DEBUG", handleDebug);
    commandMgr.registerHandler("LED", handleLED);
    commandMgr.registerHandler("PING", handlePing);

    Serial.println("Ready to receive commands...");
    Serial.println("Examples:");
    Serial.println("  DEBUG:ON;");
    Serial.println("  LED:pin=13,state=ON;");
    Serial.println("  PING;");
}

// ------------------------------
// Loop
// ------------------------------
void loop()
{
    // Continuously read and process serial input
    commandMgr.readCommands();

    // Example of sending debug message periodically
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 5000)
    {
        lastTime = millis();
        commandMgr.sendDebug("Heartbeat alive", "Loop");
    }
}
