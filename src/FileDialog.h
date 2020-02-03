#pragma once

void GetStartFolder();
void ResetStartFolder();

void FileLoadThread();
bool IsFileDialogOpen();
bool IsFileLoadReady();
void FileLoadReadyAck();
const char* GetLoadFilename();
void LoadBinary( int filetype, int address, bool setPC, bool forceAddress, bool resetUndo );
void ReloadBinary();

void BinFileWriteConfig( UserData& config );
void BinFileReadConfig( strref config );

void ListingFileDialog();
void CheckLoadListing();

void KickDebugFileDialog();
void CheckLoadKickDebug();
void SymFileDialog();
void CheckSymFileLoad();
void LoadViceCommandFileDialog();
void CheckLoadViceCommand();

