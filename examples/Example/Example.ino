#include <Arduino.h>
#include "SerialCommandManager.h"

// ------------------------------
// Example command handlers
// ------------------------------
#include "SerialCommandManager.h"

// example commands would be
// MOVE:direction=REVERSE;speed=180;
class MotorHandler : public ISerialCommandHandler {
private:
    const String _supportedCommands[1] = {"MOVE"};
    int _pinDirection;
    int _pinSpeed;

public:
    MotorHandler(int directionPin, int speedPin)
        : _pinDirection(directionPin), _pinSpeed(speedPin) {
        pinMode(_pinDirection, OUTPUT);
        pinMode(_pinSpeed, OUTPUT);
    }

    void handleCommand(SerialCommandManager* sender, const String command, const StringKeyValue params[], int paramCount) override {
        Serial.println(sender->getRawMessage());
        (void)command;
        String direction = "FORWARD";
        int speed = 0;

        for (int i = 0; i < paramCount; ++i) {
            if (params[i].key == "direction") {
                direction = params[i].value;
            } else if (params[i].key == "speed") {
                speed = params[i].value.toInt();
            }
        }

        digitalWrite(_pinDirection, direction == "FORWARD" ? HIGH : LOW);
        analogWrite(_pinSpeed, constrain(speed, 0, 255));

        sender->sendDebug("Motor moved " + direction + " at speed " + String(speed), "MotorHandler");
    }

    const String* supportedCommands(size_t& count) const override {
        count = 1;
        return _supportedCommands;
    }
};


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
MotorHandler motorHandler(5, 6); // Direction on pin 5, speed on pin 6

SerialCommandManager commandManager(&Serial, nullptr, '\n', ':', ';', 1000, 64);

// ------------------------------
// Setup
// ------------------------------
void setup()
{
    ISerialCommandHandler* handlers[] = { &motorHandler };
    commandManager.registerHandlers(handlers, 1);

    Serial.begin(115200);
    delay(500);
    Serial.println("SerialCommandManager Example Started");


    Serial.println("Ready to receive commands...");
    Serial.println("Examples:");
    Serial.println("  DEBUG:ON;");
    Serial.println("  DEBUG:OFF");
    Serial.println("  MOVE:direction=REVERSE;speed=180;");
}

// ------------------------------
// Loop
// ------------------------------
void loop()
{
    // Continuously read and process serial input
    commandManager.readCommands();

    // Example of sending debug message periodically
    static unsigned long lastTime = 0;
    if (millis() - lastTime > 5000)
    {
        lastTime = millis();
        commandManager.sendDebug("Heartbeat alive", "Loop");
    }
}
