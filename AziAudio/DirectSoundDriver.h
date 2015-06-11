/****************************************************************************
*                                                                           *
* Azimer's HLE Audio Plugin for Project64 Compatible N64 Emulators          *
* http://www.apollo64.com/                                                  *
* Copyright (C) 2000-2015 Azimer. All rights reserved.                      *
*                                                                           *
* License:                                                                  *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                        *
*                                                                           *
****************************************************************************/

#pragma once

#include "common.h"
#include <mmreg.h>
#pragma warning(disable : 4100)
#include <dsound.h>
#pragma warning(default : 4100)
#include "SoundDriver.h"

static DWORD sLOCK_SIZE;

#define DS_SEGMENTS     4
#define LOCK_SIZE 0x800
// 0x600
//sLOCK_SIZE
//0x700

#define TOTAL_SIZE (LOCK_SIZE*DS_SEGMENTS)

#define MAXBUFFER 27000
//27000
//(LOCK_SIZE*DS_SEGMENTS+LOCK_SIZE)
// LOCKSIZE must not be fractional
//#define LOCK_SIZE    (ac->SegmentSize)

#define BUFFSIZE (writeLoc-readLoc)

typedef struct DirectSoundDeviceID
{
	LPGUID devGUID;
	char* devDescStr;
	char* devModuleStr;

} DirectSoundDeviceID;

class DirectSoundDriver : public SoundDriver {
protected:
	DWORD dwFreqTarget; // Frequency of the Nintendo64 Game Audio
	void(*CallBack)(DWORD);
	BOOL audioIsPlaying;
	HANDLE handleAudioThread;
	DWORD  dwAudioThreadId;
	HANDLE hMutex;
	LPDIRECTSOUNDBUFFER  lpdsbuf;
	LPDIRECTSOUND8        lpds;
	bool audioIsDone;
	// Buffer Variables
	BYTE SoundBuffer[500000];//[MAXBUFFER];
	DWORD readLoc;
	DWORD writeLoc;
	volatile DWORD remainingBytes;
	DWORD SampleRate;
	DWORD SegmentSize;
	BOOL devEnumFailed;

	void DummyDevEnum();

public:

	DirectSoundDeviceID* deviceList;
	unsigned int devicesEnumerated;

	friend DWORD WINAPI AudioThreadProc(DirectSoundDriver *ac);

	DirectSoundDriver() { lpdsbuf = NULL; lpds = NULL; audioIsDone = false; hMutex = NULL; handleAudioThread = NULL; audioIsPlaying = FALSE; readLoc = writeLoc = remainingBytes = 0; configDeviceIdx = 0; };
	//DirectSoundDriver() {};
	~DirectSoundDriver() { };

	// Setup and Teardown Functions
	BOOL Initialize(HWND hwnd);
	void DeInitialize();

	// Buffer Functions for the Audio Code
	void SetFrequency(DWORD Frequency);		// Sets the Nintendo64 Game Audio Frequency
	DWORD AddBuffer(BYTE *start, DWORD length);	// Uploads a new buffer and returns status
	void FillBuffer(BYTE *buff, DWORD len);
	void SetSegmentSize(DWORD length);

	// Management functions
	void AiUpdate(BOOL Wait);
	void StopAudio();							// Stops the Audio PlayBack (as if paused)
	void StartAudio();							// Starts the Audio PlayBack (as if unpaused)

	DWORD GetReadStatus();						// Returns the status on the read pointer

	void SetVolume(DWORD volume);

	BOOL SwitchDevice(unsigned int deviceNum);	// Switches devices
	char* GetDeviceName(unsigned int devNum);	// Gets a device name
	BOOL RefreshDevices();						// Refresh device list

};
