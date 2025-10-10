
#include "SerialCommandManager.h"



//example to get memory
// MEM;
// DEBUG; -- returns the debug mode status
// DEBUG:ON; -- turns debug mode on
// DEBUG:OFF; -- turns debug mode off

SerialCommandManager::SerialCommandManager(Stream* serialPort, MessageReceivedCallback commandReceived, char terminator, char commandSeperator, char paramSeperator, unsigned long timeoutMilliseconds, byte maximumMessageSize)
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
    if (msgReserve > 0) {
        _incomingMessage.reserve(msgReserve);
        _rawMessage.reserve(msgReserve);
        _command.reserve(64); // typical command length
    }

    // Reserve for parameter key/value strings
    size_t perParam = _maximumMessageSize > 0 ? (_maximumMessageSize / MaximumParameterCount) + 4 : 32;
    for (int i = 0; i < MaximumParameterCount; ++i) {
        _params[i].key.reserve(perParam);
        _params[i].value.reserve(perParam);
    }}

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
            else if (_paramCount > 0)
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

    // 2. Check timeout (only if we were in middle of a message)
    if (_readingMessage && (millis() - _lastCharTime > _serialTimeout))
    {
        sendError("Timeout", "SerialCommandManager");
        _messageTimeout = true;
        _readingMessage = false;
        return;
    }
}

void SerialCommandManager::sendCommand(String header, String message, String identifier, StringKeyValue params[], int argLength)
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
	
	_serialPort->print(header);
	_serialPort->print(_commandSeperator);
	
	if (message.length() > 0)
	{
		_serialPort->print(message);
		
		if (argLength > 0)
			_serialPort->print(_commandSeperator);
	}
	
	
	for (int i = 0; i < argLength; i++)
	{
		_serialPort->print(params[i].key);
		_serialPort->print(_paramSeperator);
		_serialPort->print(params[i].value);
		
		if ((argLength -1) != i)
			_serialPort->print(_commandSeperator);
	}
	
	if (identifier != "")
		_serialPort->print(": (" + identifier + ")");

	_serialPort->print(_terminator);
}

bool SerialCommandManager::processMessage()
{
    sendDebug(_rawMessage, "SerialComdMgr-RawMessage:");

    if (_rawMessage.length() == 0)
        return true;

    if (_command == "DEBUG")
    {
        if (_paramCount == 1)
        {
            String token = _params[0].value;
            if (token.length() == 0) token = _params[0].key;
            token.trim();
            token.toUpperCase();
            _isDebug = (token == "ON");
        }

        sendCommand("DEBUG", _isDebug ? "ON" : "OFF");
        return true;
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