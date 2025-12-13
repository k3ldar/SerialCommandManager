#include "SerialCommandManager.h"

// ============================================================================
    // Helper functions for char buffer operations (replacing String methods)
    // ============================================================================

    /**
     * @brief Safely copies a string into a buffer with length limit.
     */
    static void safeCopy(char* dest, const char* src, size_t maxLen) {
        if (!dest || !src || maxLen == 0) return;
        strncpy(dest, src, maxLen - 1);
        dest[maxLen - 1] = '\0'; // Ensure null termination
    }

    /**
     * @brief Appends a character to a buffer if there's room.
     * @return true if character was added, false if buffer full.
     */
    static bool appendChar(char* buffer, char c, size_t currentLen, size_t maxLen) {
        if (!buffer || currentLen >= maxLen) return false;
        buffer[currentLen] = c;
        buffer[currentLen + 1] = '\0';
        return true;
    }

    /**
     * @brief Trims whitespace from both ends of a string in-place.
     */
    static void trimInPlace(char* str) {
        if (!str) return;
        
        // Trim leading whitespace
        char* start = str;
        while (*start && (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')) {
            start++;
        }
        
        // Trim trailing whitespace
        char* end = start + strlen(start) - 1;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
            *end = '\0';
            end--;
        }
        
        // Move trimmed string to beginning if needed
        if (start != str) {
            memmove(str, start, strlen(start) + 1);
        }
    }

    /**
     * @brief Removes trailing terminators (CR/LF) from a string.
     */
    static void removeTrailingTerminators(char* str) {
        if (!str) return;
        
        size_t len = strlen(str);
        while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
            str[len - 1] = '\0';
            len--;
        }
    }

    /**
     * @brief Finds the first occurrence of a character in a string.
     * @return Index of character, or -1 if not found.
     */
    static int16_t findChar(const char* str, char c) {
        if (!str) return -1;
        
        const char* pos = strchr(str, c);
        return pos ? (pos - str) : -1;
    }

    /**
     * @brief Checks if a string ends with a specific character.
     */
    static bool endsWith(const char* str, char c) {
        if (!str) return false;
        size_t len = strlen(str);
        return len > 0 && str[len - 1] == c;
    }


//example to get memory
// MEM;
// DEBUG; -- returns the debug mode status
// DEBUG:ON; -- turns debug mode on
// DEBUG:OFF; -- turns debug mode off

// internal message handlers
class DebugHandler : public ISerialCommandHandler {
public:
    bool handleCommand(SerialCommandManager* sender, const char* command, const StringKeyValue params[], uint8_t paramCount) override
    {
        const char* token = nullptr;
        
        if (paramCount >= 1) {
            // Check if value is non-empty, otherwise use key
            if (params[0].value[0] != '\0')
                token = params[0].value;
            else if (params[0].key[0] != '\0')
                token = params[0].key;
        }
        
        if (token) {
            if (strcmp(token, "ON") == 0) 
                sender->_isDebug = true;
            else if (strcmp(token, "OFF") == 0) 
                sender->_isDebug = false;
        }

        sender->sendCommand(command, sender->_isDebug ? "ON" : "OFF");
        return true;
    }

    const char* const* supportedCommands(size_t& count) const override {
        static const char* cmds[] = { "DEBUG" };
        count = 1;
        return cmds;
    }
};
static DebugHandler s_debugHandler;


// serial command handler;

SerialCommandManager::SerialCommandManager(Stream* serialPort, MessageReceivedCallback commandReceived, 
    char terminator, char commandSeperator, char paramSeperator, unsigned long timeoutMilliseconds, 
    uint8_t maxCommandLength, uint8_t maxMessageLength)
{
    _serialPort = serialPort;
    _messageReceivedCallback = commandReceived;
    _terminator = terminator;
    _commandSeperator = commandSeperator;
    _paramSeperator = paramSeperator;
    _serialTimeout = timeoutMilliseconds;
    _maxCommandLength = maxCommandLength;
    _maxMessageLength = maxMessageLength;
    _isDebug = false;
    _paramCount = 0;
    _messageTimeout = false;

    // Allocate buffers
    _incomingMessage = new char[_maxMessageLength + 1];
    _rawMessage = new char[_maxMessageLength + 1];
    _command = new char[_maxCommandLength + 1];
    
    // Initialize buffers to empty strings
    _incomingMessage[0] = '\0';
    _rawMessage[0] = '\0';
    _command[0] = '\0';
    
    // Initialize parameter buffers
    for (uint8_t i = 0; i < MaximumParameterCount; ++i)
    {
        _params[i].key[0] = '\0';
        _params[i].value[0] = '\0';
    }
    
    // add handlers
    registerHandlers(nullptr, 0);
}

SerialCommandManager::~SerialCommandManager()
{
    delete[] _handlerObjects;
    
    // Clean up dynamically allocated buffers
    delete[] _incomingMessage;
    delete[] _rawMessage;
    delete[] _command;
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

const char* SerialCommandManager::getCommand()
{
    return _command;
}

const StringKeyValue* SerialCommandManager::getArgs(uint8_t index)
{
    if (index >= _paramCount)
        return nullptr;
    
    return &_params[index];
}

uint8_t SerialCommandManager::getArgCount()
{
    return _paramCount;
}

const char* SerialCommandManager::getRawMessage()
{
    return _rawMessage;
}
void SerialCommandManager::readCommands()
{
    // Check if any characters have arrived
    while (_serialPort->available() > 0)
    {
        char inChar = (char)_serialPort->read();
        _lastCharTime = millis();

        if (!_readingMessage)
        {
            _readingMessage = true;
            _messageTimeout = false;
            _isParsingCommand = true;
            _rawMessage[0] = '\0';           // Clear raw message
            _incomingMessage[0] = '\0';      // Clear incoming message
            _paramCount = 0;
        }

        // Append character to raw message
        size_t rawLen = strlen(_rawMessage);
        if (!appendChar(_rawMessage, inChar, rawLen, _maxMessageLength))
        {
            sendError("Raw buffer full", "SerialCommandManager");
            _readingMessage = false;
            return;
        }

        if (inChar == _terminator)
        {
            _readingMessage = false;

            // Strip terminator and any trailing newline/CR
            trimInPlace(_incomingMessage);
            if (endsWith(_incomingMessage, _terminator))
            {
                size_t len = strlen(_incomingMessage);
                if (len > 0) {
                    _incomingMessage[len - 1] = '\0';
                }
            }
            
            // Find command separator
            int16_t sepChar = findChar(_incomingMessage, _commandSeperator);
            
            if (sepChar > -1)
            {
                // Extract command (substring before separator)
                strncpy(_command, _incomingMessage, sepChar);
                _command[sepChar] = '\0';
            }
            else
            {
                // No separator, entire message is the command
                safeCopy(_command, _incomingMessage, _maxCommandLength);
            }
            
            trimInPlace(_command);

            if (!processMessage() && _messageReceivedCallback)
                _messageReceivedCallback(this);

            break;
        }
        else if (inChar == _commandSeperator)
        {
            if (_paramCount < MaximumParameterCount)
            {
                _paramCount++;
                _params[_paramCount - 1].key[0] = '\0';
                _params[_paramCount - 1].value[0] = '\0';
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
                size_t msgLen = strlen(_incomingMessage);
                if (!appendChar(_incomingMessage, inChar, msgLen, _maxMessageLength))
                {
                    sendError("Message buffer full", "SerialCommandManager");
                    _readingMessage = false;
                    return;
                }
            }
            else if (_paramCount > 0 && _paramCount <= MaximumParameterCount)
            {
                if (_isParsingParamName)
                {
                    size_t keyLen = strlen(_params[_paramCount - 1].key);
                    if (!appendChar(_params[_paramCount - 1].key, inChar, keyLen, DefaultMaxParamKeyLength))
                    {
                        sendError("Param key too long", "SerialCommandManager");
                        _readingMessage = false;
                        return;
                    }
                }
                else
                {
                    size_t valLen = strlen(_params[_paramCount - 1].value);
                    if (!appendChar(_params[_paramCount - 1].value, inChar, valLen, DefaultMaxParamValueLength))
                    {
                        sendError(F("Param value too long"), F("SerialCommandManager"));
                        _readingMessage = false;
                        return;
                    }
                }
            }
        }

        // Check message length
        if (strlen(_incomingMessage) > _maxMessageLength)
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

void SerialCommandManager::sendCommand(const char* header, const char* message, const char* identifier, const StringKeyValue* params, uint8_t argLength)
{
    if (!header || header[0] == '\0')
        return;

    // Normalize argLength and guard params pointer
    if (argLength > MaximumParameterCount)
        argLength = MaximumParameterCount;
    
    if (argLength > 0 && params == nullptr)
        argLength = 0;

    // Make a local copy of message to sanitize terminator/CRLF if necessary
    char msg[_maxMessageLength + 1];
    if (message)
    {
        safeCopy(msg, message, _maxMessageLength);
        removeTrailingTerminators(msg);
    }
    else
    {
        msg[0] = '\0';
    }

    _serialPort->print(header);
    
    // Only print separator if we have message content or parameters
    if (msg[0] != '\0' || argLength > 0)
    {
        _serialPort->print(_commandSeperator);
    }

    if (msg[0] != '\0')
    {
        _serialPort->print(msg);

        if (argLength > 0)
            _serialPort->print(_commandSeperator);
    }

    for (uint8_t i = 0; i < argLength; ++i)
    {
        if (!params)
            break;

        _serialPort->print(params[i].key);
        _serialPort->print(_paramSeperator);
        _serialPort->print(params[i].value);

        if (i != argLength - 1)
            _serialPort->print(_commandSeperator);
    }

    if (identifier && identifier[0] != '\0')
    {
        _serialPort->print(": (");
        _serialPort->print(identifier);
        _serialPort->print(")");
    }

    // Only print the terminator if message doesn't already end with it
    if (!endsWith(msg, _terminator))
        _serialPort->print(_terminator);
}


bool SerialCommandManager::processMessage()
{
    if (_rawMessage[0] == '\0')
        return true;

    sendDebug(_rawMessage, "SerialComdMgr-RawMessage:");

    for (size_t i = 0; i < _handlerCount; ++i)
    {
        if (_handlerObjects[i]->supportsCommand(_command))
        {
            if (_handlerObjects[i]->handleCommand(this, _command, _params, _paramCount))
                return true;
        }
    }

    return false;
}

void SerialCommandManager::sendMessage(const char* messageType, const char* message, const char* identifier)
{
    if (!message || message[0] == '\0')
        return;

    if (strcmp(messageType, "DEBUG") == 0 && !_isDebug)
        return;

    _serialPort->print(messageType);
    _serialPort->print(F(":"));
    _serialPort->print(message);
    
    if (identifier && identifier[0] != '\0')
    {
        _serialPort->print(": (");
        _serialPort->print(identifier);
        _serialPort->print(")");
    }
    
    if (!endsWith(message, _terminator))
        _serialPort->print(_terminator);
}

void SerialCommandManager::sendError(const char* message, const char* identifier)
{
    sendMessage("ERR", message, identifier);
}

void SerialCommandManager::sendError(const char* message, const __FlashStringHelper* identifier) {
    // Convert Flash string identifier to temporary C-string
    char identifierBuffer[DefaultMaxParamKeyLength + 1];

    // Copy Flash string to RAM buffer
    strncpy_P(identifierBuffer, (const char*)identifier, DefaultMaxParamKeyLength);
    identifierBuffer[DefaultMaxParamKeyLength] = '\0';

    sendError(message, identifierBuffer);
}

void SerialCommandManager::sendError(const __FlashStringHelper* message, const __FlashStringHelper* identifier) {
    // Convert Flash strings to temporary C-strings
    char messageBuffer[_maxMessageLength];
    char identifierBuffer[DefaultMaxParamKeyLength + 1];
    
    // Copy Flash string to RAM buffer
    strncpy_P(messageBuffer, (const char*)message, _maxMessageLength - 1);
    messageBuffer[_maxMessageLength - 1] = '\0';
    
    // Handle optional identifier
    if (identifier != nullptr) {
        strncpy_P(identifierBuffer, (const char*)identifier, DefaultMaxParamKeyLength);
        identifierBuffer[DefaultMaxParamKeyLength] = '\0';
        sendError(messageBuffer, identifierBuffer);
    } else {
        sendError(messageBuffer, "");
    }
}

void SerialCommandManager::sendDebug(const char* message, const char* identifier)
{
    sendMessage("DEBUG", message, identifier);
}

void SerialCommandManager::sendDebug(const __FlashStringHelper* message, const __FlashStringHelper* identifier) {
    // Convert Flash strings to temporary C-strings
    char messageBuffer[_maxMessageLength];
    char identifierBuffer[DefaultMaxParamKeyLength + 1];

    // Copy Flash string to RAM buffer
    strncpy_P(messageBuffer, (const char*)message, _maxMessageLength - 1);
    messageBuffer[_maxMessageLength - 1] = '\0';

    // Handle optional identifier
    if (identifier != nullptr) {
        strncpy_P(identifierBuffer, (const char*)identifier, DefaultMaxParamKeyLength);
        identifierBuffer[DefaultMaxParamKeyLength] = '\0';
        sendDebug(messageBuffer, identifierBuffer);
    }
    else {
        sendDebug(messageBuffer, "");
    }
}

void SerialCommandManager::sendDebug(const char* message, const __FlashStringHelper* identifier) {
    // Convert Flash string identifier to temporary C-string
    char identifierBuffer[DefaultMaxParamKeyLength + 1];

    // Copy Flash string to RAM buffer
    strncpy_P(identifierBuffer, (const char*)identifier, DefaultMaxParamKeyLength);
    identifierBuffer[DefaultMaxParamKeyLength] = '\0';

    sendDebug(message, identifierBuffer);
}
