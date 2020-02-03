#pragma once

typedef void (*ViceLogger)( void*, const char* text, size_t len );

void ViceSend( const char *string, int length );
void ViceOpen( char* address, int port );
void ViceSetMem( uint16_t addr, uint8_t* bytes, int length );
bool ViceAction();
bool ViceSyncing();
bool ViceRunning();
bool ViceConnected();
void ViceBreak();
bool ViceSync();
void ViceConnectShutdown();
bool ViceUpdatingSymbols();
void ViceSetUpdateSymbols(bool enable);
void ViceAddLogger( ViceLogger logger, void* user );
void ViceSendBytes(uint16_t addr, uint16_t bytes);

