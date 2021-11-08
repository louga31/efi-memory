#pragma once

// Defines used to check if call is really coming from client
#define BASE_OPERATION 0x7cd4
#define VARIABLE_NAME L"zLjiCTzRj"

//This is only to modify every command/magic key with only 1 def and don't need to go everywhere, the compiler will automatically parse the operation to number
#define COMMAND_MAGIC BASE_OPERATION * 0xbb50

/* Operations */
#define COPY_OPERATION BASE_OPERATION * 0xdf5
#define SETUP_OPERATION BASE_OPERATION * 0x68c
#define GET_PROCESS_BASE_ADDRESS_OPERATION BASE_OPERATION * 0x86e

/* Operation modifiers */
#define DIRECT_COPY (void*)4ULL

typedef unsigned long long ptr64;
// Struct containing data used to communicate with the client
typedef struct _MemoryCommand
{
	int magic;
	int operation;
	ptr64 data[6];
} MemoryCommand;