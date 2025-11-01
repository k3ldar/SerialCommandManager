#pragma once
#include <Arduino.h>
#include <SerialCommandManager.h>

/**
 * @brief Small helper base class that centralizes ACK formatting used by command handlers.
 *
 * Handlers can inherit from `BaseCommandHandler` to get protected `sendAckOk` / `sendAckErr`
 * helpers that delegate to `SerialCommandManager::sendCommand(...)` while keeping the
 * communications layer abstract.
 *
 * This follows the typical Arduino library comment style (Doxygen-compatible) so the
 * documentation can be generated or read inline in sketches.
 */
class BaseCommandHandler : public ISerialCommandHandler
{
protected:
    /**
     * @brief Send an acknowledgement indicating the command completed successfully.
     *
     * @param sender Pointer to the `SerialCommandManager` that will perform the send.
     * @param cmd The command header/string to include in the ACK.
     * @param param Optional key/value parameter to include with the ACK (may be `nullptr`).
     */
    void sendAckOk(SerialCommandManager* sender, const String& cmd, const StringKeyValue* param = nullptr);

    /**
     * @brief Send an acknowledgement indicating the command failed.
     *
     * @param sender Pointer to the `SerialCommandManager` that will perform the send.
     * @param cmd The command header/string to include in the ACK.
     * @param err A human-readable error string describing the failure.
     * @param param Optional key/value parameter to include with the error ACK (may be `nullptr`).
     */
    void sendAckErr(SerialCommandManager* sender, const String& cmd, const String& err, const StringKeyValue* param = nullptr);
};