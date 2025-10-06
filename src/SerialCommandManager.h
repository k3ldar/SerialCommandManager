#ifndef __SerialCommandManager__
#define __SerialCommandManager__

#include <Arduino.h>
#include <stdlib.h>

#if (defined(ARDUINO) && ARDUINO >= 155) || defined(ESP8266)
 #define YIELD yield();
#else
 #define YIELD
#endif

/**
 * @brief Represents a key/value parameter pair from a parsed serial message.
 */
typedef struct StringKeyValue {
   String key;
   String value;
} keyAndValue;

/**
 * @brief Legacy-style fallback callback when no registered command handler matches.
 */
typedef void (*MessageReceivedCallback)(class SerialCommandManager* sender);

/**
 * @brief Function signature for command handlers.
 * 
 * @param command The command string (e.g., "DEBUG").
 * @param params Pointer to an array of parsed key/value parameter pairs.
 * @param paramCount Number of parameters in the array.
 * @param context Optional user-provided pointer passed at registration.
 */
typedef void (*CommandHandlerFunc)(const String& command, StringKeyValue* params, int paramCount, void* context);

/**
 * @brief Represents an entry in the registered command handler list.
 */
typedef struct {
   String command;
   CommandHandlerFunc handler;
   void* context;
} CommandHandlerEntry;

/**
 * @class SerialCommandManager
 * @brief Manages structured serial communication with automatic parsing,
 * command handling, and message timeout control.
 */
class SerialCommandManager
{
private:
	Stream* _serialPort;                ///< Pointer to the serial port or stream.
	String _command;                    ///< Current parsed command.
	StringKeyValue* _params;            ///< Dynamic array of parsed parameters.
	int _paramCount;                    ///< Number of parsed parameters.
	String _rawMessage;                 ///< The raw unprocessed message string.
	unsigned long _serialTimeout;       ///< Message timeout in milliseconds.
	unsigned int _maximumMessageSize;   ///< Maximum allowable incoming message length.
	bool _messageComplete;              ///< Flag indicating message completion.
	bool _messageTimeout;               ///< Flag indicating message timeout.
	char _terminator;                   ///< Character used to terminate messages.
	char _commandSeperator;             ///< Character separating command parts.
	char _paramSeperator;               ///< Character separating key/value pairs.
	bool _isDebug;                      ///< Debug output toggle.
	MessageReceivedCallback _messageReceivedCallback; ///< Fallback callback.

	CommandHandlerEntry* _handlers;     ///< Dynamic list of registered handlers.
	int _handlerCount;                  ///< Number of registered handlers.

	bool processMessage();              ///< Handles internal commands like DEBUG.
	bool callHandler(const String& command); ///< Calls matching registered handler.
	void clearParams();                 ///< Frees memory used by parsed parameters.

public:
	/**
	 * @brief Constructs the SerialCommandManager.
	 * 
	 * @param serialPort Pointer to the stream (e.g., &Serial).
	 * @param commandReceived Optional fallback callback.
	 * @param terminator Message terminator character.
	 * @param commandSeperator Character used to separate commands/args.
	 * @param paramSeperator Character used to separate key/value pairs.
	 * @param timeoutMilliseconds Message timeout in ms.
	 * @param maxMessageSize Maximum allowable message size.
	 */
	SerialCommandManager(Stream* serialPort, MessageReceivedCallback commandReceived,
		char terminator, char commandSeperator, char paramSeperator,
		unsigned long timeoutMilliseconds, unsigned int maxMessageSize);

	/// Destructor – frees dynamic memory.
	~SerialCommandManager();

	/// Continuously reads serial input, parses messages, and triggers handlers.
	void readCommands();

	/// @return True if the last message completed successfully.
	bool isCompleteMessage();

	/// @return True if a message timeout occurred.
	bool isTimeout();

	/// @return The parsed command string.
	String getCommand();

	/// @return The number of parsed parameters.
	int getArgCount();

	/// @param index Parameter index (0-based).
	/// @return The specified parameter pair.
	StringKeyValue getArgs(int index);

	/// @return The raw incoming message string.
	String getRawMessage();

	/**
	 * @brief Sends a formatted command with optional parameters.
	 */
	void sendCommand(const String& header, const String& message, const String& identifier = "",
	                 StringKeyValue params[] = {}, int argLength = 0);

	/**
	 * @brief Sends a debug message if debug mode is enabled.
	 */
	void sendDebug(const String& message, const String& identifier);

	/**
	 * @brief Sends a message prefixed with a specific type.
	 */
	void sendMessage(const String& messageType, const String& message, const String& identifier);

	/**
	 * @brief Sends an error message (prefixed with "ERR").
	 */
	void sendError(const String& message, const String& identifier);

	/**
	 * @brief Registers a new command handler.
	 * @param command The command to handle (case-insensitive).
	 * @param handler Function to call when command is received.
	 * @param context Optional pointer for user data.
	 * @return True on success.
	 */
	bool registerHandler(const String& command, CommandHandlerFunc handler, void* context = nullptr);

	/**
	 * @brief Removes a previously registered command handler.
	 * @param command The command name to unregister.
	 * @return True if found and removed.
	 */
	bool unregisterHandler(const String& command);
};

#endif
