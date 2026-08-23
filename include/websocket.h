#pragma once

#include "constants.h"

#ifdef SIMULATOR
#include <string>
// The simulator has no websocket. Plugins that push state to the web UI still
// need this to link; the shim discards the message.
void sendWSMessage(std::string &message);
#endif

#ifdef ENABLE_SERVER
#include <ESPAsyncWebServer.h>

#include "storage.h"

void onWsEvent(AsyncWebSocket *server,
               AsyncWebSocketClient *client,
               AwsEventType type,
               void *arg,
               uint8_t *data,
               size_t len);
void sendInfo();
void sendWSMessage(String &message);
void initWebsocketServer(AsyncWebServer &server);
void cleanUpClients();

#ifdef WS_MAX_QUEUED_MESSAGES
#undef WS_MAX_QUEUED_MESSAGES
#define WS_MAX_QUEUED_MESSAGES 64
#endif

#endif
