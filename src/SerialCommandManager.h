
#ifndef __SerialCommandManager__
#define __SerialCommandManager__


#include <stdlib.h>
#include <Arduino.h>


#if (defined(ARDUINO) && ARDUINO >= 155) || defined(ESP8266)
 #define YIELD yield();
#else
 #define YIELD
#endif

const int MaximumParameterCount = 5;

typedef struct StringKeyValue {
   String key;
   String value;
} keyAndValue;

typedef void (*MessageReceivedCallback)(class SerialCommandManager* sender);

class SerialCommandManager
{
private:
	bool _readingMessage = false;
    bool _isParsingCommand = true;
    bool _isParsingParamName = true;
    unsigned long _lastCharTime = 0;
    String _incomingMessage;
	Stream* _serialPort;
	String _command;
	StringKeyValue _params[MaximumParameterCount];
	int _paramCount;
	String _rawMessage;
	unsigned long _serialTimeout;
	byte _maximumMessageSize;
	bool _messageTimeout;
	char _terminator;
	char _commandSeperator;
	char _paramSeperator;
	bool _isDebug;
	MessageReceivedCallback _messageReceivedCallback;
	bool processMessage();
	void sendMessage(String messageType, String message, String identifier);
public:
	SerialCommandManager(Stream* serialPort, MessageReceivedCallback commandReceived, char terminator, char commandSeperator, char paramSeperator, unsigned long timeoutMilliseconds, byte maximumMessageSize);
	void readCommands();
	bool isTimeout();
	String getCommand();
	StringKeyValue getArgs(int index);
	int getArgCount();
	String getRawMessage();
	void sendCommand(String header, String message, String identifier = "", StringKeyValue params[] = {}, int argLength = 0);
	void sendDebug(String message, String identifier);
	void sendError(String message, String identifier);
};

#endif
