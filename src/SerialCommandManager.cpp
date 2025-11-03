#include "SerialCommandManager.h"



//example to get memory
// MEM;
// DEBUG; -- returns the debug mode status
// DEBUG:ON; -- turns debug mode on
// DEBUG:OFF; -- turns debug mode off




// internal message handlers
class DebugHandler : public ISerialCommandHandler {
public:
    void handleCommand(SerialCommandManager* sender, const String command, const StringKeyValue params[], int paramCount) override
    {
        String token;
        if (paramCount >= 1) {
            token = params[0].value;
            if (token.length() == 0) token = params[0].key;
        }
        token.trim();
        if (token == "ON") sender->_isDebug = true;
        else if (token == "OFF") sender->_isDebug = false;

        sender->sendCommand(command, sender->_isDebug ? "ON" : "OFF");
    }

    const String* supportedCommands(size_t& count) const override {
        static const String cmds[] = { "DEBUG" };
        count = 1;
        return cmds;
    }
};
static DebugHandler s_debugHandler;


// serial command handler;

SerialCommandManager::SerialCommandManager(Stream* serialPort, MessageReceivedCallback commandReceived, char terminator, char commandSeperator, 
    char paramSeperator, unsigned long timeoutMilliseconds, byte maximumMessageSize)
{
	_serialPort = serialPort;
	_messageReceivedCallback = commandReceived;
	_terminator = terminator;
	_commandSeperator = commandSeperator;
	_paramSeperator = paramSeperator;
	_serialTimeout = timeoutMilliseconds;
	_maximumMessageSize = maximumMessageSize;
	_isDebug = false;

    // Reserve for frequently appended buffers
    // Reserve a bit more than maximum to avoid edge reallocs
    size_t msgReserve = (size_t)_maximumMessageSize + 8;
    if (msgReserve > 0)
    {
        _incomingMessage.reserve(msgReserve);
        _rawMessage.reserve(msgReserve);
        _command.reserve(64); // typical command length
    }

    // Reserve for parameter key/value strings
    size_t perParam = _maximumMessageSize > 0 ? (_maximumMessageSize / MaximumParameterCount) + 4 : 32;
    for (int i = 0; i < MaximumParameterCount; ++i) 
    {
        _params[i].key.reserve(perParam);
        _params[i].value.reserve(perParam);
    }
	
	// add handlers
    registerHandlers(nullptr, 0);
}

SerialCommandManager::~SerialCommandManager()
{
	delete[] _handlerObjects;
}

void SerialCommandManager::registerHandlers(ISerialCommandHandler** handlers, size_t handlerCount)
{
    if (_handlerObjects)
    {
        delete[] _handlerObjects;
        _handlerObjects = nullptr;
        _handlerCount = 0;
	}

    size_t internalHandlers = 1;
    _handlerCount = handlerCount + internalHandlers;
    _handlerObjects = new ISerialCommandHandler * [_handlerCount];

    // internal debug handler
    _handlerObjects[0] = &s_debugHandler;

    for (size_t i = 1; i < _handlerCount; i++)
    {
        _handlerObjects[i] = handlers[i - internalHandlers];
    }
}

bool SerialCommandManager::isTimeout()
{
	return _messageTimeout;
}

String SerialCommandManager::getCommand()
{
	return _command;
}

StringKeyValue SerialCommandManager::getArgs(int index)
{
	if (index < 0 || index >= _paramCount)
		return { "", "" };
	
	return _params[index];
}

int SerialCommandManager::getArgCount()
{
	return _paramCount;
}

String SerialCommandManager::getRawMessage()
{
	return _rawMessage;
}
void SerialCommandManager::readCommands()
{
    // 1. Check if any characters have arrived
    while (_serialPort->available() > 0)
    {
        char inChar = (char)_serialPort->read();
        _lastCharTime = millis();

        if (!_readingMessage)
        {
            _readingMessage = true;
            _messageTimeout = false;
			_isParsingCommand = true;
            _rawMessage = "";
            _incomingMessage = "";
            _paramCount = 0;
        }

        _rawMessage += inChar;

        if (inChar == _terminator)
        {
            _readingMessage = false;

            // Strip terminator and any trailing newline/CR
            _incomingMessage.trim();
            if (_incomingMessage.endsWith(String(_terminator)))
			{
                _incomingMessage.remove(_incomingMessage.length() - 1);
            }
			
			int sepChar = _incomingMessage.indexOf(_commandSeperator);
			
			if (sepChar > -1)
				_command = _incomingMessage.substring(0, sepChar);
			else
				_command = _incomingMessage;
			
			_command.trim();

			if (!processMessage() && _messageReceivedCallback)
				_messageReceivedCallback(this);

           break;
        }
        else if (inChar == _commandSeperator)
        {
            if (_paramCount < MaximumParameterCount)
            {
                _paramCount++;
                _params[_paramCount - 1].key = "";
                _params[_paramCount - 1].value = "";
            }
            _isParsingCommand = false;
            _isParsingParamName = true;
        }
        else if (inChar == _paramSeperator)
        {
            _isParsingParamName = false;
        }
        else
        {
            if (_isParsingCommand)
            {
                _incomingMessage += inChar;
            }
            else if (_paramCount > 0 && _paramCount <= MaximumParameterCount)
            {
                if (_isParsingParamName)
                    _params[_paramCount - 1].key += inChar;
                else
                    _params[_paramCount - 1].value += inChar;
            }
        }

        if (_incomingMessage.length() > _maximumMessageSize)
        {
            sendError("Too Long", "SerialCommandManager");
            _readingMessage = false;
            return;
        }
    }

    if (_readingMessage && (millis() - _lastCharTime > _serialTimeout))
    {
        sendError("Timeout", "SerialCommandManager");
        _messageTimeout = true;
        _readingMessage = false;
        return;
    }
}

void SerialCommandManager::sendCommand(String header, String message, String identifier, StringKeyValue* params, int argLength)
{
    if (header.length() == 0)
        return;

    // Normalize argLength and guard params pointer
    if (argLength < 0)
		argLength = 0;
	
    if (argLength > MaximumParameterCount)
		argLength = MaximumParameterCount;
	
    if (argLength > 0 && params == nullptr)
		argLength = 0;

    // Make a local copy of message to sanitize terminator/CRLF if necessary
    String msg = message;
    while (msg.length() > 0 && (msg.endsWith("\n") || msg.endsWith("\r")))
        msg.remove(msg.length() - 1);

    _serialPort->print(header);
    
    // Only print separator if we have message content or parameters
    if (msg.length() > 0 || argLength > 0)
    {
        _serialPort->print(_commandSeperator);
    }

    if (msg.length() > 0)
    {
        _serialPort->print(msg);

        if (argLength > 0)
            _serialPort->print(_commandSeperator);
    }

    for (int i = 0; i < argLength; ++i)
    {
        if (!params)
			break;

        _serialPort->print(params[i].key);
        _serialPort->print(_paramSeperator);
        _serialPort->print(params[i].value);

        if (i != argLength - 1)
            _serialPort->print(_commandSeperator);
    }

    if (identifier.length() > 0)
        _serialPort->print(": (" + identifier + ")");

    // Only print the terminator if message doesn't already end with it
    if (!msg.endsWith(String(_terminator)))
        _serialPort->print(_terminator);
}


bool SerialCommandManager::processMessage()
{
    if (_rawMessage.length() == 0)
        return true;

    sendDebug(_rawMessage, "SerialComdMgr-RawMessage:");

    for (size_t i = 0; i < _handlerCount; ++i)
    {
        if (_handlerObjects[i]->supportsCommand(_command))
        {
            _handlerObjects[i]->handleCommand(this, _command, _params, _paramCount);
            return true;
        }
    }

    return false;
}

void SerialCommandManager::sendDebug(String message, String identifier)
{
	sendMessage("DEBUG", message, identifier);
}

void SerialCommandManager::sendMessage(String messageType, String message, String identifier)
{
	if (message.length() == 0)
		return;

	if (messageType == "DEBUG" && !_isDebug)
		return;

	_serialPort->print(messageType);
	_serialPort->print(F(":"));
	_serialPort->print(message);
	
	if (identifier.length() > 0)
		_serialPort->print(": (" + identifier + ")");
	
	if (!message.endsWith(String(_terminator)))
		_serialPort->print(_terminator);
}

void SerialCommandManager::sendError(String message, String identifier)
{
	sendMessage("ERR", message, identifier);
}
