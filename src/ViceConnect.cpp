// start a telnet connection with a local instance of VICE
#include "stdafx.h"
#include "winsock2.h"
#include <ws2tcpip.h>
#include <inttypes.h>
#include <stdio.h>
#include "machine.h"
#include "Expressions.h"
#include "Sym.h"
#include "ViceConnect.h"
#include <vector>
#include "Breakpoints.h"
#include "struse\struse.h"
#include "platform.h"


class ViceConnect {
	struct sendCmdRecord {
		char* buf;
		int length;
	};

public:
	enum { RECEIVE_SIZE = 4096 };

	ViceConnect() : activeConnection(false), threadHandle(IBThread_Clear), logFunc(nullptr), cmd_mutex(IBMutex_Clear), syncing(false),
	syncRequest(false), viceUpdatesSymbols(true) {}

	void connectionThread();

	bool openConnection(char* address, int port);
	bool connect(char* address = "127.0.0.1", int port = 6510);
	void sendCmd(const char* msg, int len);
	void modMem(uint16_t addr, uint8_t * bytes, int len);
	void close();

	bool open(char* address, int port);

	void addLogger(ViceLogger logger, void * user)
	{
		logFunc = logger;
		logUser = user;
	}

	sockaddr_in addr;
	SOCKET s;
	IBThread threadHandle;

	ViceLogger logFunc;
	void* logUser;

	// commands to send (moving send into connectionThread)
	std::vector<sendCmdRecord> commands;
	IBMutex cmd_mutex;

	bool activeConnection;
	bool closeRequest;
	bool viceRunning;
	bool monitorOn;
	bool stopRequest;
	bool syncing;
	bool syncRequest;
	bool viceUpdatesSymbols;
	bool viceReloadSymbols;
};

static ViceConnect sVice;

static const char* sViceRunning = "\n<VICE started>\n";
static const char* sViceStopped = "<VICE stopped>\n";
static const char* sViceConnected = "<VICE connected>\n";
static const char* sViceDisconnected = "<VICE disconnected>\n";
static const char* sViceLost = "<VICE connection lost>\n";
static const int sViceLostLen = (const int)strlen(sViceLost);
static const char* sViceRun = "x\n";
static const char* sViceDel = "del\n";
static char sViceExit[64];
static int sViceExitLen;

void ViceSend(const char *string, int length)
{
	if (sVice.activeConnection) {
		if (length&&(string[0]=='x'||string[0]=='X'||string[0]=='g'||string[0]=='G')) {
			memcpy(sViceExit, string, length);
			sViceExitLen = length;
			sVice.viceRunning = true;
		} else if (length && _strnicmp(string, "quit", 4) == 0) {
			sVice.closeRequest = true;
		}
		//send(sVice.s, string, length, NULL);
		sVice.sendCmd(string, length);
	}
}

void ViceSendBytes(uint16_t addr, uint16_t bytes)
{
	while (bytes) {
		strown<256> command(">");
		command.append_num(addr, 4, 16);
		while (bytes && command.left() > 6) {
			command.append(' ').append_num(Get6502Byte(addr++), 2, 16);
			--bytes;
		}
		command.append('\n');
		ViceSend(command.get(), command.len());
	}
}

void ViceOpen(char * address, int port)
{
	if (!sVice.activeConnection) {
		sVice.connect(address, port);
	}
}

void ViceSetMem(uint16_t addr, uint8_t * bytes, int length)
{
	sVice.modMem(addr, bytes, length);
}

bool ViceAction()
{
	if (sVice.activeConnection) {
		if (sVice.monitorOn) {
			ViceSend(sViceRun, (int)strlen(sViceRun));
			return true;
		}
		sVice.close();
		if (sVice.logFunc) {
			sVice.logFunc(sVice.logUser, sViceDisconnected, sizeof(sViceDisconnected)-1);
		}
		//		if (CMainFrame *pFrame = theApp.GetMainFrame()) {
		//			pFrame->VicePrint(sViceDisconnected, (int)strlen(sViceDisconnected));
		//		}
		return false;
	}
	return sVice.connect();
}

bool ViceSyncing()
{
	return sVice.activeConnection && sVice.syncing;
}

bool ViceRunning()
{
	return sVice.activeConnection&&!sVice.monitorOn;
}

bool ViceConnected()
{
	return sVice.activeConnection;
}

void ViceBreak()
{
	sVice.stopRequest = true;
}

bool ViceSync()
{
	if (sVice.activeConnection) {
		sVice.syncRequest = true;
		return true;
	}
	return false;
}

bool ViceUpdatingSymbols() { return sVice.viceUpdatesSymbols;  }

void ViceSetUpdateSymbols(bool enable) { sVice.viceUpdatesSymbols = enable; }

void ViceConnectShutdown()
{
	if (sVice.activeConnection) {
		if (!sVice.viceRunning) {
			ViceSend("x\12", 2);
			while (!sVice.viceRunning && sVice.activeConnection) {
				Sleep(100);
			}
		}
		sVice.close();
	}
}

void ViceAddLogger(ViceLogger logger, void * user)
{
	sVice.addLogger(logger, user);
}

IBThreadRet ViceConnectThread(void* data)
{
	((ViceConnect*)data)->connectionThread();
	return 0;
}

bool ViceConnect::connect(char* address, int port)
{
	monitorOn = false;
	closeRequest = false;
	activeConnection = false;
	if (openConnection(address, port)) {
		if (cmd_mutex==IBMutex_Clear) {
			IBMutexInit(&cmd_mutex, "Vice connect mutex");
		}
		IBCreateThread(&threadHandle, 16384, ViceConnectThread, this);
	}
	return false;
}

void ViceConnect::sendCmd(const char* msg, int len)
{
	bool pad = msg[len-1]!=0x0a;
	if (pad) { len++; }
	char* copy = (char*)malloc(len);
	memcpy(copy, msg, len);
	copy[len-1] = 0x0a;
	sendCmdRecord rec = { copy, len };

	IBMutexLock(&cmd_mutex);
	commands.push_back(rec);
	IBMutexRelease(&cmd_mutex);
}

void ViceConnect::modMem(uint16_t addr, uint8_t* bytes, int len)
{
	// send ">$addr $bb$bb... 
	if (activeConnection) {
		strown<512> line;
		while (len) {
			line.copy(">$");
			line.append_num(addr, 4, 16);
			line.append(' ');
			int push = len<32 ? len : 32;
			len -= push;
			while (push--) {
				line.append('$');
				line.append_num(*bytes++, 2, 16);
			}
			line.append('\n');
			char* copy = (char*)malloc(line.get_len());
			memcpy(copy, line.get(), line.get_len());
			sendCmdRecord rec = { copy, (int)line.get_len() };
			IBMutexLock(&cmd_mutex);
			commands.push_back(rec);
			IBMutexRelease(&cmd_mutex);
		}
	}
}

void ViceConnect::close()
{
	IBDestroyThread(&threadHandle);
	closesocket(s);
	WSACleanup();
	activeConnection = false;
	closeRequest = false;

}

// Open a connection to a remote host
bool ViceConnect::open(char* address, int port)
{
	// Make sure the user has specified a port
	if (port<0||port > 65535) { return false; }

	WSADATA wsaData = { 0 };
	int iResult = 0;

	DWORD dwRetval;

	struct sockaddr_in saGNI;
	char hostname[NI_MAXHOST];
	char servInfo[NI_MAXSERV];

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult!=0) {
		printf("WSAStartup failed: %d\n", iResult);
		return false;
	}
	//-----------------------------------------
	// Set up sockaddr_in structure which is passed
	// to the getnameinfo function
	saGNI.sin_family = AF_INET;

	inet_pton(AF_INET, address, &(saGNI.sin_addr.s_addr));

	//	saGNI.sin_addr.s_addr =
	//	InetPton(AF_INET, strIP, &ipv4addr)
	//	inet_addr(address);
	saGNI.sin_port = htons(port);

	//-----------------------------------------
	// Call getnameinfo
	dwRetval = getnameinfo((struct sockaddr *) &saGNI,
		sizeof(struct sockaddr),
		hostname,
		NI_MAXHOST, servInfo, NI_MAXSERV, NI_NUMERICSERV);

	if (dwRetval!=0) {
		return false;
	}

	iResult = ::connect(s, (struct sockaddr *)&saGNI, sizeof(saGNI));
	return iResult==0;
}

bool ViceConnect::openConnection(char* address, int port)
{
	WSADATA ws;

	// Load the WinSock dll
	long status = WSAStartup(0x0101, &ws);
	if (status!=0) { return false; }

	memset(&addr, 0, sizeof(addr));
	s = socket(AF_INET, SOCK_STREAM, 0);

	// Open the connection
	if (!open(address, port)) { return false; }

	activeConnection = true;
	return true;
}

enum ViceUpdate {
	Vice_None,
	Vice_Running,
	Vice_StartMonitor,
	//	Vice_Start,
	//	Vice_Memory,
	//	Vice_Labels,
	//	Vice_Breakpoints,
	//	Vice_Registers,
	Vice_Wait,
	Vice_Sync,

	//	Vice_SendBreakpoints,
	Vice_Return,
	//	Vice_Break,
};


static uint32_t lastBPID = ~0UL;

// const char* sMemory = "m $0000 $ffff\n";
//const char* sRegisters = "r\n";
//const char* sLabels = "shl\n";
//const char* sBreakpoints = "bk\n";

const char* sBundle = "registers\12show_labels\12break\12m $0000 $ffff\12";

// waits for vice break after connection is established
void ViceConnect::connectionThread()
{
	strown<512> lineOut;
	strown<512> lineInfo;
	DWORD timeout = 100;// SOCKET_READ_TIMEOUT_SEC*1000;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

	char recvBuf[RECEIVE_SIZE];
	char line[512];
	int offs = 0;
	bool next_line_is_trace_address = false;

	sViceExit[0] = 0;

	ViceUpdate state = Vice_None;
	viceRunning = false;

	if (sVice.logFunc) {
		sVice.logFunc(sVice.logUser, sViceConnected, strlen(sViceConnected));
	}

	uint8_t *RAM = nullptr;

	int currBreak = 0;

	stopRequest = false;

	while (activeConnection) {
		// close after all commands have been sent?
		if (closeRequest && !commands.size()) {
			threadHandle = INVALID_HANDLE_VALUE;
			close();
			break;
		}

		char* sendCmd = nullptr;
		int sendCmdLen = 0;
		IBMutexLock(&cmd_mutex);
		if (commands.size()) {
			if (monitorOn) {
				if (state==Vice_Wait) {
					sendCmdRecord& rec = commands[0];
					sendCmd = rec.buf;
					sendCmdLen = rec.length;
					commands.erase(commands.begin());
				}
			} else {
				stopRequest = true;
			}
		}

		if (viceRunning&&!commands.size()) {
			monitorOn = false;
			state = Vice_Running;
			offs = 0;
		}

		IBMutexRelease(&cmd_mutex);

		if (sendCmd) {
			send(s, sendCmd, sendCmdLen, 0);
			char buf[256];
			int siz = sendCmdLen<254 ? sendCmdLen : 254;
			memcpy(buf, sendCmd, siz);
			buf[siz] = '\n';
			buf[siz+1] = 0;
#ifdef _DEBUG
			OutputDebugStringA(buf);
#endif
			free(sendCmd);
		}

		int bytesReceived = recv(s, recvBuf, RECEIVE_SIZE, 0);
		if (bytesReceived==SOCKET_ERROR) {
			if (WSAGetLastError()==WSAETIMEDOUT) {
				if ((state==Vice_None||state==Vice_Running)&&stopRequest) {
					offs = 0;
					send(s, "r\n", 2, NULL);
					stopRequest = false;
				} else if (syncRequest) {
					syncRequest = false;
					ClearAllPCBreakpoints();
					ResetViceBP();
					send(s, sBundle, (int)strlen(sBundle), NULL);
					state = Vice_Sync;
					syncing = true;
					monitorOn = true;
					offs = 0;
					viceRunning = false;
					viceReloadSymbols = true;
				}
				Sleep(100);
			} else {
				activeConnection = false;
				break;
			}
		} else {
			int read = 0;
			while (read<bytesReceived) {
				char c = line[offs++] = recvBuf[read++];

				// vice prompt = (C:$????)
				bool prompt = offs>=9&&line[8]==')'&&strncmp(line, "(C:$", 4)==0;

				// vice sends lines so process one line at a time
				if (c==0x0a||offs==sizeof(line)||prompt) {//||prompt||((state==Vice_Wait) && read==bytesReceived)) {

					// Parse info:
					// "(C:$????)" -> prompt
					// ">C:???? -> memory listing "  xx xx xx xx  xx xx xx xx..." ends with 3 spaces
					// .;xxxx -> registers
					// .C:xxxx -> step, next PC, after -: A:xx X:xx Y:xx SP:xx NC-BDIZC
					// BREAK: ?... breakpoint
					// WATCH: ?... watch
					// STORE ... trace

					lineInfo.append(strref(line, offs));
					bool is_trace = lineInfo.get_first()=='#';	// better way to check for trace? anything else starts with '#'??
					if (next_line_is_trace_address && c!='\n') { continue; } else if (next_line_is_trace_address) {
						offs = 0;
						next_line_is_trace_address = false;
						if (sVice.logFunc) { sVice.logFunc(sVice.logUser, lineInfo.get(), lineInfo.get_len()); }
						lineInfo.clear();
						continue;
					} else if (is_trace) {//strref("#1 (Trace").is_prefix_of(lineInfo) ) {
						offs = 0;
						next_line_is_trace_address = true;
						if (sVice.logFunc) { sVice.logFunc(sVice.logUser, lineInfo.get(), lineInfo.get_len()); }
						lineInfo.clear();
						continue;
					} else if (state==Vice_StartMonitor||state==Vice_Running) {
#ifdef _DEBUG
						OutputDebugStringA(lineInfo.c_str());
#endif
						ClearAllPCBreakpoints();
						ResetViceBP();

						send(s, sBundle, (int)strlen(sBundle), NULL);
						state = Vice_Sync;
						syncing = true;
						monitorOn = true;
						offs = 0;
						viceRunning = false;
						viceReloadSymbols = true;
						lineInfo.clear();
						continue;
					}

					if (c==0x0a) {
						lineInfo.c_str();
						strref lineParse(lineInfo.get_strref());
						lineParse.skip_whitespace();
						lineInfo.clear();
						// remove prompt
						while (lineParse.get_len()>8&&lineParse[0]=='(' && lineParse[2]==':' && lineParse[3]=='$' && lineParse[8]==')') {
							lineParse.skip(9);
							lineParse.skip_whitespace();
						}
#ifdef _DEBUG
						if (state!=Vice_Sync) {
							const char* os = lineParse.get();
							OutputDebugStringA(os);
						}
#endif
						if (state!=Vice_Sync) {
							if (sVice.logFunc && offs) { sVice.logFunc(sVice.logUser, lineParse.get(), lineParse.get_len()); }
						}

						switch (lineParse.get_first()) {
						case '>': // read memory from VICE
							if (lineParse[1]=='C' && lineParse[2]==':' && lineParse.get_len()>7) {
								uint16_t addr = uint16_t(lineParse.get_substr(3, 4).ahextou64());
								lineParse += 7;
								while (lineParse) {
									lineParse.skip_whitespace();
									strref byte = lineParse.split(2);
									Set6502Byte(addr++, (uint8_t)byte.ahextoui());
									if (!addr && state == Vice_Sync) { 
										state = Vice_Wait;
										syncing = false;
										viceReloadSymbols = false;
										SetSandboxContext(false);
									}
									if (const char* parse = lineParse.get()) {
										if (parse[0]==' ' && parse[1]==' ' && parse[2]==' ') { break; }
									}
								}
							}
							break;
						case '.':
							if (lineParse[1]==';') {
								// read registers
								Regs& regs = GetRegs();
								if (lineParse.get_len()>=6) { regs.PC = uint16_t(lineParse.get_substr(2, 4).ahextoui()); }
								if (lineParse.get_len()>=9) { regs.A = uint8_t(lineParse.get_substr(7, 2).ahextoui()); }
								if (lineParse.get_len()>=12) { regs.X = uint8_t(lineParse.get_substr(10, 2).ahextoui()); }
								if (lineParse.get_len()>=15) { regs.Y = uint8_t(lineParse.get_substr(13, 2).ahextoui()); }
								if (lineParse.get_len()>=18) { regs.S = uint8_t(lineParse.get_substr(16, 2).ahextoui()); }
								if (lineParse.get_len()>=32) { regs.P = uint8_t(lineParse.get_substr(25, 8).abinarytoui_skip()); }
							} else if (lineParse[1]=='C' && lineParse[2]==':') {
							}
							break;
						case '$':
							if (lineParse.get_len()>6&&lineParse[5]==' ' && lineParse[6]=='.' && viceUpdatesSymbols) {
								if (viceReloadSymbols) {
									ShutdownSymbols();
									viceReloadSymbols = false;
								}
								uint16_t addr = uint16_t(lineParse.get_substr(1, 4).ahextou64());
								strref name = lineParse+6;
								name.trim_whitespace();
								AddSymbol(addr, name.get(), name.get_len());
							}
							break;
						case 'B':
						case 'b':
							// BREAK: 1  C:$1234  (Stop on exec)
							if (lineParse.grab_prefix("BREAK")) {
								++lineParse; lineParse.skip_whitespace();
								int index = lineParse.atoi_skip();
								lineParse.skip_whitespace();
								if (lineParse.grab_prefix("C:$")) {
									uint16_t addr = (uint16_t)lineParse.ahextoui_skip();
									lineParse.skip_whitespace();
									lineParse.split_lang(); // (Stop on exec)
									lineParse.skip_whitespace();
									ViceBPType type = VBP_Break;
									bool disabled = lineParse.grab_prefix("disabled");
									SetViceBP(addr, addr, index, true, type, disabled, true);
									SetPCBreakpoint(addr);
								}
								offs = 0;
							}
							break;
						case 'W':
						case 'w':
							// BREAK: 1  C:$1234  (Stop on stpre)
							if (lineParse.grab_prefix("WATCH")) {
								++lineParse; lineParse.skip_whitespace();
								int index = lineParse.atoi_skip();
								lineParse.skip_whitespace();
								if (lineParse.grab_prefix("C:$")) {
									uint16_t addr = (uint16_t)lineParse.ahextoui_skip();
									uint16_t end = addr;
									lineParse.skip_whitespace();
									if (lineParse.grab_char('-')) {
										++lineParse;
										end = (uint16_t)lineParse.ahextoui_skip();
										lineParse.skip_whitespace();
									}
									bool store = lineParse.has_prefix("(Stop on store)");
									lineParse.split_lang(); // (Stop on exec)
									lineParse.skip_whitespace();
									//										ViceBPType type = lineParse.grab_prefix( "disabled" ) ? VBP_BreakDisabled : VBP_Break;
									ViceBPType type = store ? VBP_WatchStore : VBP_WatchRead;
									bool disabled = lineParse.grab_prefix("disabled");
									SetViceBP(addr, end, index, true, type, disabled, true);
								}
								offs = 0;
							}
							break;
						}
					}

					if (state==Vice_Return) {
						if (prompt) { state = Vice_None; }
						if (sVice.logFunc) { sVice.logFunc(sVice.logUser, line, offs); }
					} else  if (state==Vice_None) {
						if (prompt) {
							state = Vice_Sync;
							syncing = true;
							send(s, sBundle, (int)strlen(sBundle), NULL);
							monitorOn = true;
							viceReloadSymbols = true;
						}
					}
					offs = 0;
				}
			}
		}
	}
	if (RAM) { free(RAM); }
	if (sVice.logFunc) { sVice.logFunc(sVice.logUser, sViceLost, strlen(sViceLost)); }
	//	if (CMainFrame *pFrame = theApp.GetMainFrame()) {
//		pFrame->VicePrint(sViceLost, sViceLostLen);
//	}
}
