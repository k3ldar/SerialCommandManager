#include "SerialCommandManager.h"

SerialCommandManager::SerialCommandManager(Stream* serialPort, MessageReceivedCallback commandReceived,
	char terminator, char commandSeperator, char paramSeperator,
	unsigned long timeoutMilliseconds, unsigned int maxMessageSize)
{
	_serialPort = serialPort;
	_messageReceivedCallback = commandReceived;
	_terminator = terminator;
	_commandSeperator = commandSeperator;
	_paramSeperator = paramSeperator;
	_serialTimeout = timeoutMilliseconds;
	_maximumMessageSize = maxMessageSize;
	_isDebug = false;
	_messageComplete = false;
	_messageTimeout = false;
	_paramCount = 0;
	_params = nullptr;
	_handlers = nullptr;
	_handlerCount = 0;
}

SerialCommandManager::~SerialCommandManager()
{
	clearParams();
	if (_handlers)
	{
		free(_handlers);
		_handlers = nullptr;
	}
}

bool SerialCommandManager::isCompleteMessage() { return _messageComplete; }
bool SerialCommandManager::isTimeout() { return _messageTimeout; }
String SerialCommandManager::getCommand() { return _command; }
int SerialCommandManager::getArgCount() { return _paramCount; }
String SerialCommandManager::getRawMessage() { return _rawMessage; }

StringKeyValue SerialCommandManager::getArgs(int index)
{
	if (index < 0 || index >= _paramCount)
		return {"", ""};
	return _params[index];
}

/**
 * @brief Frees all allocated parameter memory and resets count.
 */
void SerialCommandManager::clearParams()
{
	if (_params)
	{
		free(_params);
		_params = nullptr;
	}
	_paramCount = 0;
}

/**
 * @brief Reads characters from the serial port, builds and parses messages.
 */
void SerialCommandManager::readCommands()
{
	if (_serialPort->available() > 0)
	{
		String incomingMessage;
		incomingMessage.reserve(_maximumMessageSize);
		_messageTimeout = false;
		_messageComplete = false;
		_rawMessage = "";
		unsigned long startMillis = millis();
		bool isCommand = true;
		bool isParamName = true;
		clearParams();

		while (!_messageComplete)
		{
			if (_serialPort->available() > 0)
			{
				char inChar = (char)_serialPort->read();
				_rawMessage += inChar;

				if (inChar == _terminator)
				{
					_messageComplete = true;
					break;
				}
				else if (inChar == _commandSeperator)
				{
					isCommand = false;
					isParamName = true;
					_paramCount++;
					_params = (StringKeyValue*)realloc(_params, sizeof(StringKeyValue) * _paramCount);
					_params[_paramCount - 1].key = "";
					_params[_paramCount - 1].value = "";
				}
				else if (inChar == _paramSeperator)
				{
					isParamName = false;
				}
				else
				{
					if (isCommand)
						incomingMessage += inChar;
					else
					{
						if (_paramCount == 0)
						{
							_paramCount++;
							_params = (StringKeyValue*)realloc(_params, sizeof(StringKeyValue) * _paramCount);
							_params[0].key = "";
							_params[0].value = "";
						}
						if (isParamName)
							_params[_paramCount - 1].key += inChar;
						else
							_params[_paramCount - 1].value += inChar;
					}
				}

				if (incomingMessage.length() > _maximumMessageSize)
				{
					_serialPort->println(F("ERR:Too Long"));
					return;
				}
			}

			if (millis() - startMillis > _serialTimeout)
			{
				_serialPort->println(F("ERR:Timeout"));
				_messageTimeout = true;
				return;
			}
		}

		int sepChar = incomingMessage.indexOf(_commandSeperator);
		if (sepChar > -1)
			_command = incomingMessage.substring(0, sepChar);
		else
			_command = incomingMessage;

		if (!processMessage() && _messageReceivedCallback)
		{
			if (!callHandler(_command))
				_messageReceivedCallback(this);
		}
	}
	YIELD;
}

/**
 * @brief Handles built-in DEBUG command.
 * @return True if handled internally.
 */
bool SerialCommandManager::processMessage()
{
	sendDebug(_rawMessage, "SerialComdMgr");

	if (_command.equalsIgnoreCase("DEBUG"))
	{
		if (_paramCount == 1)
			_isDebug = _params[0].key.equalsIgnoreCase("ON");

		sendCommand("DEBUG", _isDebug ? "ON" : "OFF");
		return true;
	}
	return false;
}

/**
 * @brief Invokes a registered handler if the command matches.
 */
bool SerialCommandManager::callHandler(const String& command)
{
	for (int i = 0; i < _handlerCount; i++)
	{
		if (_handlers[i].command.equalsIgnoreCase(command))
		{
			if (_handlers[i].handler)
				_handlers[i].handler(command, _params, _paramCount, _handlers[i].context);
			return true;
		}
	}
	return false;
}

/**
 * @brief Registers a handler dynamically.
 */
bool SerialCommandManager::registerHandler(const String& command, CommandHandlerFunc handler, void* context)
{
	if (!handler || command.length() == 0)
		return false;

	_handlers = (CommandHandlerEntry*)realloc(_handlers, sizeof(CommandHandlerEntry) * (_handlerCount + 1));
	if (!_handlers)
		return false;

	_handlers[_handlerCount].command = command;
	_handlers[_handlerCount].handler = handler;
	_handlers[_handlerCount].context = context;
	_handlerCount++;
	return true;
}

/**
 * @brief Removes a handler from the registry.
 */
bool SerialCommandManager::unregisterHandler(const String& command)
{
	for (int i = 0; i < _handlerCount; i++)
	{
		if (_handlers[i].command.equalsIgnoreCase(command))
		{
			for (int j = i; j < _handlerCount - 1; j++)
				_handlers[j] = _handlers[j + 1];

			_handlerCount--;
			_handlers = (CommandHandlerEntry*)realloc(_handlers, sizeof(CommandHandlerEntry) * _handlerCount);
			return true;
		}
	}
	return false;
}

/**
 * @brief Sends a command with optional parameters.
 */
void SerialCommandManager::sendCommand(const String& header, const String& message, const String& identifier,
                                       StringKeyValue params[], int argLength)
{
	if (!header.length())
		return;

	_serialPort->print(header);
	_serialPort->print(_commandSeperator);

	if (message.length())
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
		if (i < argLength - 1)
			_serialPort->print(_commandSeperator);
	}

	if (identifier.length())
		_serialPort->print(": (" + identifier + ")");

	_serialPort->println(_terminator);
}

/**
 * @brief Sends a debug message if debug mode is enabled.
 */
void SerialCommandManager::sendDebug(const String& message, const String& identifier)
{
	sendMessage("DEBUG", message, identifier);
}

/**
 * @brief Sends a message of a specific type.
 */
void SerialCommandManager::sendMessage(const String& messageType, const String& message, const String& identifier)
{
	if (!message.length() || !_isDebug)
		return;

	_serialPort->print(messageType);
	_serialPort->print(":");
	_serialPort->print(message);

	if (identifier.length())
		_serialPort->print(": (" + identifier + ")");

	if (!message.endsWith(String(_terminator)))
		_serialPort->println(_terminator);
}

/**
 * @brief Sends an error message.
 */
void SerialCommandManager::sendError(const String& message, const String& identifier)
{
	sendMessage("ERR", message, identifier);
}
