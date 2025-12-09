#ifndef __SerialCommandManager__
#define __SerialCommandManager__


#include <stdlib.h>
#include <Arduino.h>


#if (defined(ARDUINO) && ARDUINO >= 155) || defined(ESP8266)
 #define YIELD yield();
#else
 #define YIELD
#endif

const uint8_t MaximumParameterCount = 5;

typedef struct StringKeyValue {
   String key;
   String value;
} keyAndValue;

/**
 * @brief Callback function type for message reception.
 * 
 * @param sender Pointer to the SerialCommandManager instance that received the message.
 */
typedef void (*MessageReceivedCallback)(class SerialCommandManager* sender);

/**
 * @brief Interface for handling serial commands.
 * 
 * This is an interface class that defines how command handlers should behave. It’s meant to be implemented by any class that wants to handle specific serial commands.
 *
 * Responsibilities:
 * Command Handling: Implements handleCommand() to define how to respond when a command is received.
 *
 * Command Support Declaration: Implements supportedCommands() to list which commands the handler can process.
 *
 * Command Matching: Includes a helper method supportsCommand() to check if a given command is supported.
 *
 * Use Case:
 * You’d create one or more classes that inherit from ISerialCommandHandler to handle different command types. For example, 
 * a MotorHandler might respond to "MOVE" or "STOP" commands.
*/
class ISerialCommandHandler {
public:
    /**
     * @brief Called when a command matching one of the supported commands arrives.
     * 
     * @param sender Pointer to the SerialCommandManager instance that received the command.
     * @param command The command string that was received.
     * @param params Array of key-value parameter pairs.
     * @param paramCount Number of parameters in the array.
	 * @return true if the command was handled successfully, false otherwise.
     */
    virtual bool handleCommand(SerialCommandManager* sender, const String command, const StringKeyValue params[], uint8_t paramCount) = 0;

    /**
     * @brief Returns a list of supported command tokens.
     * 
     * @param count Reference to store the number of supported commands.
     * @return const String* Array of supported commands (uppercase, trimmed).
     */
    virtual const String* supportedCommands(size_t& count) const = 0;

    /**
     * @brief Checks if this handler supports a specific command.
     * 
     * @param command The command string to check.
     * @return true if the command is supported, false otherwise.
     */
    virtual bool supportsCommand(const String& command) const {
        size_t count;
        const String* cmds = supportedCommands(count);
        for (size_t i = 0; i < count; ++i) {
            if (cmds[i] == command) return true;
        }
        return false;
    }

    virtual ~ISerialCommandHandler() {}
};

/**
 * @brief Manages serial command parsing and dispatching to registered handlers.
 * 
 * This is the core class that manages serial communication, parses incoming messages, and dispatches commands to the appropriate handlers.
 *
 * Responsibilities:
 * Message Reception: Reads from a Stream (like Serial) and parses incoming messages.
 *
 * Command Parsing: Extracts the command and its parameters from the message.
 *
 * Handler Dispatching: Calls the appropriate handler based on the command received.
 *
 * Timeout Handling: Detects if a message took too long to arrive.
 *
 * Message Sending: Can send debug, error, or command messages back over the serial port.
 *
 * Handler Registration: Accepts an array of ISerialCommandHandler objects to delegate command handling.
 *
 * Key Features:
 * Supports up to 5 parameters per command.
 *
 * Customizable message format using terminator, command separator, and parameter separator.
 *
 * Optional debug mode for verbose output.
 *
 * Callback mechanism (MessageReceivedCallback) for external notification when a message is received.
 */
class SerialCommandManager
{
    friend class DebugHandler;
private:
    ISerialCommandHandler** _handlerObjects;
    size_t _handlerCount = 0;
    bool _readingMessage = false;
    bool _isParsingCommand = true;
    bool _isParsingParamName = true;
    unsigned long _lastCharTime = 0;
    String _incomingMessage;
    Stream* _serialPort;
    String _command;
    StringKeyValue _params[MaximumParameterCount];
    uint8_t _paramCount;
    String _rawMessage;
    unsigned long _serialTimeout;
    uint8_t _maximumMessageSize;
    bool _messageTimeout;
    char _terminator;
    char _commandSeperator;
    char _paramSeperator;
    bool _isDebug;
    MessageReceivedCallback _messageReceivedCallback;

    /**
     * @brief Processes the incoming message and dispatches to handlers.
     * 
     * @return true if the message was processed successfully, false otherwise.
     */
    bool processMessage();

    /**
     * @brief Sends a message over the serial port.
     * 
     * @param messageType The type of message (e.g., "DEBUG", "ERROR").
     * @param message The message content.
     * @param identifier Optional identifier for the message.
     */
    void sendMessage(String messageType, String message, String identifier);

public:
    /**
     * @brief Constructs a SerialCommandManager instance.
     * 
     * @param serialPort Pointer to the Stream object for serial communication.
     * @param commandReceived Callback function for received commands.
     * @param terminator Character that terminates a command message.
     * @param commandSeperator Character that separates command from parameters.
     * @param paramSeperator Character that separates parameters.
     * @param timeoutMilliseconds Timeout for receiving a complete message.
     * @param maximumMessageSize Maximum allowed message size.
     */
    SerialCommandManager(Stream* serialPort, MessageReceivedCallback commandReceived, char terminator, 
        char commandSeperator, char paramSeperator, unsigned long timeoutMilliseconds, byte maximumMessageSize);

    /**
     * @brief Destructor for SerialCommandManager.
     */
    ~SerialCommandManager();

    /**
     * @brief Registers an array of command handler objects.
     * 
     * @param handlers Array of pointers to ISerialCommandHandler objects.
     * @param handlerCount Number of handlers in the array.
     */
    void registerHandlers(ISerialCommandHandler** handlers, size_t handlerCount);

    /**
     * @brief Reads and processes incoming serial commands.
     */
    void readCommands();

    /**
     * @brief Checks if the last message reception timed out.
     * 
     * @return true if a timeout occurred, false otherwise.
     */
    bool isTimeout();

    /**
     * @brief Gets the parsed command string from the last message.
     * 
     * @return The command string.
     */
    String getCommand();

    /**
     * @brief Gets a parsed key/value argument by index.
     * 
     * @param index Index of the argument to retrieve.
     * @return The key/value pair at the specified index.
     */
    StringKeyValue getArgs(uint8_t index);

    /**
     * @brief Gets the number of parsed arguments in the last message.
     * 
     * @return The argument count.
     */
    uint8_t getArgCount();

    /**
     * @brief Gets the raw message string as received.
     * 
     * @return The raw message string.
     */
    String getRawMessage();

    /**
     * @brief Sends a command message over the serial port.
     * 
     * @param header The command header string.
     * @param message The message content.
     * @param identifier Optional identifier for the message.
     * @param params Optional array of key/value parameters.
     * @param argLength Number of parameters in the array.
     */
    void sendCommand(String header, String message, String identifier = "", StringKeyValue* params = nullptr, uint8_t argLength = 0);

    /**
     * @brief Sends a debug message over the serial port.
     * 
     * @param message The debug message content.
     * @param identifier Optional identifier for the message.
     */
    void sendDebug(String message, String identifier);

    /**
     * @brief Sends an error message over the serial port.
     * 
     * @param message The error message content.
     * @param identifier Optional identifier for the message.
     */
    void sendError(String message, String identifier);
};

#endif
