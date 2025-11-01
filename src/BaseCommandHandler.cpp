#include "BaseCommandHandler.h"

void BaseCommandHandler::sendAckOk(SerialCommandManager* sender, const String& cmd, const StringKeyValue* param)
{
    if (!sender)
        return;

    // Format acknowledgement as specified in Commands.md:
    // ACK:<command>=<result>
    // If no explicit msg provided, use "ok" (lowercase per examples)

    String payload = cmd + "=ok";

    if (param == nullptr)
        sender->sendCommand("ACK", payload);
    else
        sender->sendCommand("ACK", payload, "", const_cast<StringKeyValue*>(param), 1);
}

void BaseCommandHandler::sendAckErr(SerialCommandManager* sender, const String& cmd, const String& err, const StringKeyValue* param)
{
    if (!sender)
        return;

    // Errors are also reported via ACK with the error text after '=' per Commands.md examples:
    // ACK:<command>=<error message>
    String payload = cmd + "=" + err;

    if (param == nullptr)
        sender->sendCommand("ACK", payload);
    else
        sender->sendCommand("ACK", payload, "", const_cast<StringKeyValue*>(param), 1);
}
