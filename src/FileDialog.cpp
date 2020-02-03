#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <windows.h>
#include <commdlg.h>
#include "stdafx.h"
#include "struse/struse.h"
#include "machine.h"
#include "Sym.h"
#include "Breakpoints.h"
#include "Config.h"
#include "Views.h"
#include "Listing.h"
#include "SourceDebug.h"
#include "ViceConnect.h"
#include "platform.h"

#define FILE_LOAD_THREAD_STACK 8192

IBThread hThreadFileDialog = 0;
IBThread hThreadLoadBinary = 0;
static bool sFileDialogOpen = false;
static bool sFileLoadReady = false;
static bool sListLoadReady = false;
static bool sKickDebugLoadReady = false;
static bool sSymFileLoadReady = false;
static bool sViceCommandLoadReady = false;
static char sFileNameOpen[ MAX_PATH ] = {};
static char sCurrentDir[ MAX_PATH ] = {};

static int binFiletype;
static int binAddress;
static bool binLoadSetPC;
static bool binForceAddress;
static bool binLoadResetUndo;
static char binLoadFilename[MAX_PATH];


bool IsFileDialogOpen() { return sFileDialogOpen; }
bool IsFileLoadReady() { return sFileLoadReady; }
void FileLoadReadyAck() { sFileLoadReady = false; }
const char* GetLoadFilename() { return sFileNameOpen; }

void GetStartFolder()
{
#ifdef _WIN32
	if( GetCurrentDirectory( sizeof( sCurrentDir ), sCurrentDir ) != 0 ) {
		return;
	}
#endif
	sCurrentDir[ 0 ] = 0;
}

void ResetStartFolder()
{
	if( sCurrentDir[ 0 ] ) {
#ifdef _WIN32
		SetCurrentDirectory( sCurrentDir );
#endif
	}
}

struct FileDialogParams {
	const char* filter;
	char* filename;
	int filename_size;
	bool* ready;
};

static FileDialogParams sLoadProgramParams = { "All\0*.*\0Prg\0*.prg\0Bin\0*.bin\0", sFileNameOpen, sizeof(sFileNameOpen), &sFileLoadReady };
static FileDialogParams sLoadListingParams = { "List\0*.lst\0", sFileNameOpen, sizeof(sFileNameOpen), &sListLoadReady };
static FileDialogParams sLoadKickDbgParams = { "C64Debugger\0*.dbg\0", sFileNameOpen, sizeof(sFileNameOpen), &sKickDebugLoadReady };
static FileDialogParams sLoadSymbolsParams = { "Symbols\0*.sym\0", sFileNameOpen, sizeof(sFileNameOpen), &sSymFileLoadReady };
static FileDialogParams sLoadViceCmdParams = { "Vice Commands\0*.vs\0", sFileNameOpen, sizeof(sFileNameOpen), &sViceCommandLoadReady };

IBThreadRet FileDialogThreadRun(void *param)
{
	FileDialogParams* params = (FileDialogParams*)param;

#ifdef _WIN32
	OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAME);
	//	ofn.hInstance = GetPrgInstance();
	ofn.lpstrFile = params->filename;
	ofn.nMaxFile = params->filename_size;
	ofn.lpstrFilter = params->filter;
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileName(&ofn) != TRUE) {
		DWORD err = GetLastError();
		sFileDialogOpen = false;
		return 0;
	}
	sFileDialogOpen = false;
	*params->ready = true;
#else
	// FILE DIALOG NOT IMPLEMENTED
#endif
	return 0;
}


void FileLoadThread()
{
	// can't load if already running
	if( IsCPURunning() || sFileDialogOpen )
		return;

	sFileLoadReady = false;
	sFileDialogOpen = true;

	IBCreateThread(&hThreadFileDialog, FILE_LOAD_THREAD_STACK, FileDialogThreadRun, &sLoadProgramParams);
}

void BinFileWriteConfig( UserData& config )
{
	strown<32> arg;
	config.AddValue( strref( "binFiletype" ), binFiletype );
	arg.copy( "$" );
	arg.append_num( binAddress, 4, 16 );
	config.AddValue( strref( "binAddress" ), arg.c_str() );
	config.AddValue( strref( "binLoadSetPC" ), config.OnOff( binLoadSetPC ) );
	config.AddValue( strref( "binForceAddress" ), config.OnOff( binForceAddress ) );
	config.AddValue( strref( "binLoadResetUndo" ), config.OnOff( binLoadResetUndo ) );
	config.AddValue( strref( "binLoadFilename" ), binLoadFilename );
}

void BinFileReadConfig( strref config )
{
	ConfigParse conf( config );
	while( !conf.Empty() ) {
		strref name, value;
		ConfigParseType type = conf.Next( &name, &value );
		if( name.same_str( "binFiletype" ) && type == CPT_Value ) {
			binFiletype = (int)value.atoi();
		} else if( name.same_str("binAddress") && type == CPT_Value ) {
			if( value.grab_char( '$' ) ) {
				binAddress = (uint16_t)value.ahextoui();
			}
		} else if( name.same_str("binLoadSetPC") && type == CPT_Value ) {
			binLoadSetPC = !value.same_str( "Off" );
		} else if( name.same_str("binForceAddress") && type == CPT_Value ) {
			binForceAddress = !value.same_str( "Off" );
		} else if( name.same_str("binLoadResetUndo") && type == CPT_Value ) {
			binLoadResetUndo = !value.same_str( "Off" );
		} else if( name.same_str("binLoadFilename") && type == CPT_Value ) {
			strovl name( binLoadFilename, MAX_PATH );
			name.copy( value );
		}
	}
}


IBThreadRet LoadBinaryThread(void* params)
{
	(void)params;
	FILE *f = nullptr;

	if( fopen_s( &f, binLoadFilename, "rb" ) == 0 ) {
		// assume .prg for now
		fseek( f, 0, SEEK_END );
		size_t size = ftell( f );
		fseek( f, 0, SEEK_SET );

		if( binFiletype == 0 || binFiletype == 1 ) {
			int8_t addr8[ 2 ];
			fread( addr8, 1, 2, f );
			size_t addr = addr8[ 0 ] + (uint16_t( addr8[ 1 ] ) << 8);
			size -= 2;
			if( binFiletype == 1 ) {
				fread( addr8, 1, 2, f );
				size = addr8[ 0 ] + (uint16_t( addr8[ 1 ] ) << 8);
			}
			if( !binForceAddress )
				binAddress = (int)addr;
		}

		size_t read = size < size_t( 0x10000 - binAddress ) ?
			size : size_t( 0x10000 - binAddress );

		uint8_t *trg = Get6502Mem( (uint16_t)binAddress );
		fread( trg, read, 1, f );
		fclose( f );

		GetRegs().T = 0;
		if( binLoadSetPC ) {
			// refinement:
			// if load address == $0801 && file type == 0 && [$0805] == $9e =>
			// { start address = strref($0806).atoi() }
			uint16_t startAddr = binAddress;
			if( binAddress == 0x0801 && binFiletype == 0 && Get6502Byte( 0x0805 ) == 0x9e ) {
				startAddr = (uint16_t)strref( (const char*)Get6502Mem( 0x0806 ) ).atoi();
			}
			GetRegs().PC = startAddr;
			FocusPC();
		}

		if( binLoadResetUndo ) {
			ResetUndoBuffer();
		}

		ReadSymbolsForBinary( binLoadFilename );
		ResetStartFolder();
	}
	hThreadLoadBinary = nullptr;
	return 0;
}


void LoadBinary( int filetype, int address, bool setPC, bool forceAddress, bool resetUndo )
{
	binFiletype = filetype;
	binAddress = address;
	binLoadSetPC = setPC;
	binForceAddress = forceAddress;
	binLoadResetUndo = resetUndo;
	memcpy( binLoadFilename, sFileNameOpen, MAX_PATH );

	IBCreateThread(&hThreadLoadBinary, FILE_LOAD_THREAD_STACK, LoadBinaryThread, nullptr);
}

void ReloadBinary()
{
	IBCreateThread(&hThreadLoadBinary, FILE_LOAD_THREAD_STACK, LoadBinaryThread, nullptr);
}

void ListingFileDialog()
{
	sListLoadReady = false;
	sFileDialogOpen = true;

	IBCreateThread(&hThreadFileDialog, FILE_LOAD_THREAD_STACK, FileDialogThreadRun, &sLoadListingParams);
}

void CheckLoadListing()
{
	if( sListLoadReady ) {
		LoadListing( sFileNameOpen );
		sListLoadReady = false;
		ResetStartFolder();
	}
}

void KickDebugFileDialog()
{
	sKickDebugLoadReady = false;
	sFileDialogOpen = true;

	IBCreateThread(&hThreadFileDialog, FILE_LOAD_THREAD_STACK, FileDialogThreadRun, &sLoadKickDbgParams);
}

void CheckLoadKickDebug()
{
	if (sKickDebugLoadReady) {
		ViceSetUpdateSymbols(false);
		ReadC64DbgSrc(sFileNameOpen);
		sKickDebugLoadReady = false;
		ResetStartFolder();
	}
}

void SymFileDialog()
{
	sSymFileLoadReady = false;
	sFileDialogOpen = true;

	IBCreateThread(&hThreadFileDialog, FILE_LOAD_THREAD_STACK, FileDialogThreadRun, &sLoadSymbolsParams);
}

void CheckSymFileLoad()
{
	if (sSymFileLoadReady) {
		ViceSetUpdateSymbols(false);
		ReadSymbols(sFileNameOpen);
		sSymFileLoadReady = false;
		ResetStartFolder();
	}
}

void LoadViceCommandFileDialog()
{
	sViceCommandLoadReady = false;
	sFileDialogOpen = true;

	IBCreateThread(&hThreadFileDialog, FILE_LOAD_THREAD_STACK, FileDialogThreadRun, &sLoadViceCmdParams);
}

void CheckLoadViceCommand()
{
	if (sViceCommandLoadReady) {
		ReadViceCommandFile(sFileNameOpen);
		sViceCommandLoadReady = false;
		ResetStartFolder();
	}
}
