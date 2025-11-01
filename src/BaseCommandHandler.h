#pragma once
#include <Arduino.h>
#include <SerialCommandManager.h>

/*
  Small helper base class that centralizes ACK formatting used by command handlers.
  Handlers can inherit this to get protected sendOk/sendErr helpers that delegate
  to SerialCommandManager::sendCommand(...) while keeping the comms layer abstract.
*/
class BaseCommandHandler : public ISerialCommandHandler
{
protected:
    void sendAckOk(SerialCommandManager* sender, const String& cmd, const StringKeyValue* param = nullptr);
    void sendAckErr(SerialCommandManager* sender, const String& cmd, const String& err, const StringKeyValue* param = nullptr);
};