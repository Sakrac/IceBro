// support for loading listing files
#include "struse/struse.h"
#include "HashTable.h"
#include "SourceDebug.h"
#include <malloc.h>
#include <vector>
#include <assert.h>

struct ListLineInfo
{
	strref m_line;
	uint8_t m_numBytes;
	uint8_t m_bytes[7]; // for validating line, not sure needed
};

struct ListSection
{
	strref m_name;
	strref m_type;
	HashTable<ListLineInfo> m_lines;
};

struct ListFile
{
	std::vector< ListSection* > m_sections;
	void* m_fileText;
	~ListFile() { for( auto &section : m_sections ) { delete section; } delete m_fileText; }
};

static ListFile* sListing = nullptr;

void ShutdownListing()
{
	if( sListing ) {
		delete sListing;
		sListing = nullptr;
	}
}

strref GetListing( uint16_t address, const uint8_t** bytes, uint8_t* numBytes )
{
	if( sListing ) {
		for( auto &section : sListing->m_sections ) {
			if( ListLineInfo* info = section->m_lines.Value( address ) )
			{
				if( numBytes && bytes ) {
					*numBytes = info->m_numBytes;
					*bytes = info->m_bytes;
				}
				return info->m_line;
			}
		}
	}
	return strref();
}

void LoadListing( const char* filename )
{
	FILE *f = nullptr;

	if( fopen_s( &f, filename, "rb" ) == 0 ) {

		ShutdownSourceDebug();
		ShutdownListing();

		fseek( f, 0, SEEK_END );
		size_t size = ftell( f );
		fseek( f, 0, SEEK_SET );

		void* data = malloc( size );
		fread( data, size, 1, f );
		fclose( f );
		strref file( (const char*)data, (int)size );
		ListSection* currSection = nullptr;

		ListFile* listfile = new ListFile;
		listfile->m_fileText = data;
		if( file[0]==0xef && file[1]==0xbb && file[2]==0xbf ) { file +=3; }	// bom if applied

		while( strref line = file.line() ) {
			if( line.get_word().same_str( "Section" ) ) {
				// file header has a list of sections that should be ignored
				line += 7; // len("section")
				line.skip_whitespace();
				strref SectName = line.get_word();
				line.skip( SectName.get_len() );
				line.skip_whitespace();
				if( line.get_first() == '(' ) {
					++line; line.skip_whitespace();
					int SectNum = line.atoi_skip();
					if( line.get_first() == ',' ) {
						++line; line.skip_whitespace();
						strref SectType = line.get_word();
						// check if section already listed
						bool sectionFound = false;
						for(auto &section : listfile->m_sections) {
							if( section->m_name.same_str( SectName ) ) {
								currSection = section;
								sectionFound = true;
								break;
							}
						}
						if( !sectionFound ) {
							ListSection* section = new ListSection;
							section->m_name = SectName;
							section->m_type = SectType;
							listfile->m_sections.push_back( section );
							currSection = listfile->m_sections[ listfile->m_sections.size() - 1 ];
						}
					}
				}
			} else if( currSection && line.get_first() == '$' ) {
				ListLineInfo info;
				info.m_numBytes = 0;
				info.m_line = line + 40;
				info.m_line.clip_trailing_whitespace();
				++line;
				// if address is already used in this section ignore the new one
				uint16_t addr = (uint16_t)line.ahextoui_skip();
				if( addr && !currSection->m_lines.Exists(addr) ) {	// 0 is not a valid hash key
					++line;
					while( strref::is_hex(line.get_first()) && strref::is_ws(line[2]) && info.m_numBytes < 7 ) {
						info.m_bytes[ info.m_numBytes++ ] = (uint8_t)line.ahextoui_skip();
						++line;
					}
					currSection->m_lines.Insert(addr, info);
					assert( currSection->m_lines.Value( addr ) );
				}
			}
		}

		if( sListing ) {
			delete sListing;
		}
		sListing = listfile;
	}
}

