// Manages breakpoint window and synchronizing vice in monitor mode

#include <stdint.h>
#include <vector>
#include "struse/struse.h"
#include "ViceConnect.h"
#include "Breakpoints.h"
#include "machine.h"

#define MAX_VICEBP 256
//typedef std::vector<ViceBP> ViceBPList;



static ViceBP viceBreaks[ MAX_VICEBP ];
static size_t numViceBreaks = 0;

void ResetViceBP()
{
	numViceBreaks = 0;
}

const ViceBP* GetBreakpoints() {
	if( numViceBreaks ) {
		return &viceBreaks[ 0 ];
	}
	return nullptr;
}

int GetNumBreakpoints() {
	return (int)numViceBreaks;
}

int GetViceBPIndex( uint16_t addr, ViceBPType type )
{
	for( size_t i = 0, n = numViceBreaks; i < n; ++i ) {
		if( viceBreaks[ i ].address == addr ) {
			if( viceBreaks[ i ].type == (unsigned int)type ) { return int(viceBreaks[i].viceIndex); }
		}
	}
	return -1;
}

bool HasViceBP( uint16_t addr, ViceBPType type )
{
	for( size_t i = 0, n = numViceBreaks; i < n; ++i ) {
		if( addr == viceBreaks[ i ].address && type == viceBreaks[i].type ) {
			return true;
		}
	}
	return false;
}

void SetViceBP( uint16_t address, uint16_t end, int index, bool add, ViceBPType type, bool disabled, bool updateFromVice )
{
	if( add ) {
		for( size_t i = 0, n = numViceBreaks; i < n; ++i ) {
			if( index != -1 && viceBreaks[ i ].viceIndex == -1 && viceBreaks[ i ].address == address &&
				type == viceBreaks[ i ].type ) {
				viceBreaks[ i ].viceIndex = index;
				viceBreaks[ i ].disabled = disabled;
				return;
			}
			if( index == viceBreaks[ i ].viceIndex ) {
				if( index == -1 && !(address == viceBreaks[ i ].address &&
					type == viceBreaks[ i ].type ) ) {
					continue;
				}
				viceBreaks[ i ].address = address;
				viceBreaks[ i ].end = end;
				viceBreaks[ i ].type = type;
				viceBreaks[ i ].disabled = disabled;
				if( type == VBP_Break ) {
					if( disabled ) { ClearPCBreakpoint( address ); }
					else { SetPCBreakpoint( address ); }
				}
				return;
			}
		}
		ViceBP bp = { address, end, index, type };
		viceBreaks[ numViceBreaks++ ] = bp;
		if( type == VBP_Break ) {
			if( disabled ) { ClearPCBreakpoint( address ); }
			else { SetPCBreakpoint( address ); }
		}
		if( !updateFromVice && ViceConnected() ) {
			strown<128> command;
			command.sprintf( "BREAK $%04x", address );
			ViceSend( command.c_str(), command.get_len() );
		}
	} else {
		for( size_t i = 0; i < numViceBreaks; ++i ) {
			if( index == viceBreaks[i].viceIndex && address == viceBreaks[i].address ) {
				if( type == viceBreaks[i].type ) {

					if( type == VBP_Break ) { ClearPCBreakpoint( address ); }

					if( ViceConnected() ) {
						strown<128> command;
						command.sprintf( "DEL %d", viceBreaks[i].viceIndex );
					}
					for( size_t j = i + 1; j < numViceBreaks; ++j ) {
						viceBreaks[ j - 1 ] = viceBreaks[ j ];
					}
					--numViceBreaks;
					return;
				}
			}
		}
	}
}