#pragma once

strref GetListing( uint16_t address, const uint8_t** bytes, uint8_t* numBytes );
void LoadListing( const char* filename );
void ShutdownListing();


