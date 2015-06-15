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

#include "common.h"
#ifndef USE_XAUDIO2
#include <mmreg.h>
#pragma warning(disable : 4100)
#include <dsound.h>
#pragma warning(default : 4100)
#include <stdio.h>
#include "DirectSoundDriver.h"
#include "AudioSpec.h"
//#include "WaveOut.h"

#define STREAM_DMA

#define USE_PRINTF

#ifdef USE_PRINTF
#define dprintf printf
#else
#define dprintf //
#endif

// TODO: Clean this up a bit...
DWORD last_pos = 0, write_pos = 0, play_pos = 0, temp = 0, next_pos = 0;
DWORD last_play = 0;
DWORD last_write = -1;
LPVOID lpvPtr1, lpvPtr2;
DWORD dwBytes1, dwBytes2;
int AudioInterruptTime = -1;
DWORD lastLength = 0;
DWORD DMALen[3] = { 0, 0, 0 };
BYTE *DMAData[3] = { NULL, NULL, NULL };

LPDIRECTSOUNDBUFFER lpdsbuff = NULL;

DWORD buffsize = 0;
DWORD laststatus = 0;
DWORD interruptcnt = 0;

static char DummyDevStr[] = "Default DirectSound8 Device";

//WaveOut test;

// Fills up a buffer and remixes the audio
#ifdef STREAM_DMA
void DirectSoundDriver::FillBuffer(BYTE *buff, DWORD len) {
	DWORD cnt = 0;
	DWORD writeCnt = 0;
	DWORD lastValue = 0;
	// Fill buffer from play buffer
	if (remainingBytes >= LOCK_SIZE)
	{
		while ((remainingBytes > 0) && (cnt != len)) { // Optimize this copy routine later
			*(DWORD *)(&buff[cnt]) = *(DWORD *)(&SoundBuffer[readLoc]);
			cnt += 4; readLoc += 4; remainingBytes -= 4;
			if (readLoc == MAXBUFFER)
				readLoc = 0;
		}
		if (cnt > 0)
			lastValue = *(DWORD *)(&SoundBuffer[readLoc]);
	}
	// Write out silence

	if (cnt != len && cnt > 0) { dprintf("&"); }
	while (cnt != len) {
		*(DWORD *)(&buff[cnt]) = lastValue;
		cnt += 4;
	}

}
#else
void DirectSoundDriver::FillBuffer(BYTE *buff, DWORD len) {
	DWORD cnt = 0;
	DWORD x = 0;
	DWORD pastFill = readLoc;
	DWORD pastLoc = (len - remainingBytes) / 2;

	if (configForceSync) {
		*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
		AudioInfo.CheckInterrupts();
		interruptcnt--;
		*AudioInfo.AI_STATUS_REG &= ~0x80000000;
		//dprintf("I");
	}
	if (configAIEmulation == true) {
		if (*AudioInfo.AI_STATUS_REG & 0x80000000) {
			*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
			AudioInfo.CheckInterrupts();
			interruptcnt--;
			*AudioInfo.AI_STATUS_REG &= ~0x80000000;
			//dprintf("I");
		}
	}

	if (remainingBytes == 0) {
		dprintf("-");
		memset(buff, 0, len);
		return;
	}
	if (remainingBytes < len) {
		dprintf("!");
		memset(buff, 0, len); // Save it for another buffer fill
		return;
	}
	else {
		while ((remainingBytes > 0) && (cnt != len)) { // Optimize this copy routine later
			*(DWORD *)(&buff[cnt]) = *(DWORD *)(&SoundBuffer[readLoc]);
			//cnt++; readLoc++; remainingBytes--;
			cnt += 4; readLoc += 4; remainingBytes -= 4;
			if (readLoc == MAXBUFFER)
				readLoc = 0;
		}
	}
	// Check into cnt != len...
	if (cnt != len)
		printf("%");

	while (cnt != len) {
		buff[cnt] = 0;
		cnt++;
	}
}
#endif // STREAM_DMA

DWORD WINAPI AudioThreadProc(DirectSoundDriver *ac) {
	DWORD dwStatus;
	DWORD last_play_pos = 0, bytesMoved = 0;
	//LPDIRECTSOUNDBUFFER8  lpdsbuf = ac->lpdsbuf;
	LPDIRECTSOUND8        lpds = ac->lpds;

	lpdsbuff = ac->lpdsbuf;

	while (lpdsbuff == NULL)
		Sleep(10);

	IDirectSoundBuffer_GetStatus(lpdsbuff, &dwStatus);
	if ((dwStatus & DSBSTATUS_PLAYING) == 0) {
		IDirectSoundBuffer_Play(lpdsbuff, 0, 0, 0);
	}

	SetThreadPriority(ac->handleAudioThread, THREAD_PRIORITY_HIGHEST);

	while (ac->audioIsDone == false) { // While the thread is still alive
		while (last_pos == write_pos) { // Cycle around until a new buffer position is available
			if (lpdsbuff == NULL)
				ExitThread(-1);
			// Check to see if the audio pointer moved on to the next segment
			if (write_pos == last_pos) {
				Sleep (1);
			}
			WaitForSingleObject(ac->hMutex, INFINITE);
			if FAILED(lpdsbuff->GetCurrentPosition((unsigned long*)&play_pos, NULL)) {
				MessageBox(NULL, "Error getting audio position...", PLUGIN_VERSION, MB_OK | MB_ICONSTOP);
				goto _exit_;
			}
			ReleaseMutex(ac->hMutex);
			// *** Cached method ***
			// Determine our write position by where our play position resides
			if (play_pos < LOCK_SIZE) write_pos = (LOCK_SIZE * DS_SEGMENTS) - LOCK_SIZE;
			else write_pos = ((play_pos / LOCK_SIZE) * LOCK_SIZE) - LOCK_SIZE;
			// *** JIT ***
			//write_pos = ((play_pos / LOCK_SIZE) * LOCK_SIZE) + (LOCK_SIZE*2);
			//if (write_pos >= TOTAL_SIZE) {
			//	write_pos -= TOTAL_SIZE;
			//}
			//if (play_pos >= (TOTAL_SIZE-LOCK_SIZE)) write_pos = LOCK_SIZE;
			//else write_pos = ((play_pos / LOCK_SIZE) * LOCK_SIZE) + LOCK_SIZE;

			//			if (write_pos == last_pos) {
			last_play = play_pos; // Store the last play position
			//				Sleep (1);
			//			}


#ifdef STREAM_DMA
#ifdef SEH_SUPPORTED
			__try // PJ64 likes to close objects before it shuts down the DLLs completely...
			{
#endif
			if (play_pos > last_play_pos) bytesMoved = play_pos - last_play_pos; else bytesMoved = TOTAL_SIZE - last_play_pos + play_pos;
			last_play_pos = play_pos;
			if (DMALen[0] != 0 && (*AudioInfo.AI_CONTROL_REG & 0x01) == 1)
			{
				//WaitForSingleObject(ac->hMutex, INFINITE);
				DWORD writeCnt = 0;
				DWORD writeOut = 0;
				bool bDoInterrupt = false;
				// Write new data to play buffer from DMA
				if (DMALen[0] + DMALen[1] < LOCK_SIZE)
				{
					//*AudioInfo.AI_STATUS_REG &= ~0x80000001;
					//dprintf(";");
				}
				if (ac->remainingBytes < (MAXBUFFER - LOCK_SIZE))
				{
					*AudioInfo.AI_STATUS_REG &= ~0x80000001;
					//writeOut = bytesMoved;
					writeOut = LOCK_SIZE;
				}
				else
				{
					//if (DMALen[1] != 0)
					if (DMALen[0] + DMALen[1] > LOCK_SIZE)
						*AudioInfo.AI_STATUS_REG |= 0x80000001;
					writeOut = 0;
				}
				writeOut &= ~3;
				/*
				else if (ac->remainingBytes >= LOCK_SIZE * 3)
				writeOut = 0;
				else
				writeOut = LOCK_SIZE/8;			*/
				DWORD SavedDMALen0 = DMALen[0];
				DWORD SavedDMALen1 = DMALen[1];
				while (writeCnt != writeOut && DMAData[0] > 0)
				{
					ac->SoundBuffer[ac->writeLoc++] = DMAData[0][2];
					ac->SoundBuffer[ac->writeLoc++] = DMAData[0][3];
					ac->SoundBuffer[ac->writeLoc++] = DMAData[0][0];
					ac->SoundBuffer[ac->writeLoc++] = DMAData[0][1];
					DMAData[0] += 4;
					DMALen[0] -= 4;
					writeCnt += 4;
					ac->remainingBytes += 4;
					if (ac->writeLoc == MAXBUFFER)
						ac->writeLoc = 0;
					if (DMALen[0] == 0)
					{ // Swap buffer and invoke an interrupt -- potential issue should the lock_size be greater than the buffer size
						DMALen[0] = DMALen[1]; DMAData[0] = DMAData[1];
						DMALen[1] = DMALen[2]; DMAData[1] = DMAData[2];
						DMALen[2] = 0; DMAData[2] = NULL;

						if (bDoInterrupt == true)
							printf("D");
						bDoInterrupt = true;
						*AudioInfo.AI_STATUS_REG &= ~0x80000001;
						*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
						if (DMALen[0] == 0 && DMALen[1] == 0)
						{
							*AudioInfo.AI_STATUS_REG &= ~0xC0000001;
							dprintf("E");
						}
						AudioInfo.CheckInterrupts();
						break;
					}
				}

				/*
				if (writeCnt != writeOut) dprintf("#");
				while (writeCnt != writeOut)
				{
				ac->SoundBuffer[ac->writeLoc++] = 0;
				ac->SoundBuffer[ac->writeLoc++] = 0;
				ac->SoundBuffer[ac->writeLoc++] = 0;
				ac->SoundBuffer[ac->writeLoc++] = 0;
				writeCnt += 4;
				ac->remainingBytes += 4;
				if (ac->writeLoc == MAXBUFFER)
				ac->writeLoc = 0;
				}*/
				if (ac->remainingBytes > MAXBUFFER)
					dprintf("M");
				//ReleaseMutex(ac->hMutex);
				if (bDoInterrupt == true)
				{
					//Sleep(1);
					dprintf("I");
				}
			}
			else
			{
				//if (last_pos == write_pos)	Sleep(1);
			}
#ifdef SEH_SUPPORTED
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
			}
#endif
#endif
		}
		// This means we had a buffer segment skipped skip
		if (next_pos != write_pos) {
			dprintf("A");
		}

		// Store our last position
		last_pos = write_pos;

		// Set out next anticipated segment
		next_pos = write_pos + LOCK_SIZE;
		if (next_pos >= (LOCK_SIZE*DS_SEGMENTS)) {
			next_pos -= (LOCK_SIZE*DS_SEGMENTS);
		}
		if (ac->audioIsDone == true) break;
		// Fill queue buffer here with LOCK_SIZE
		// TODO: Add buffer processing command here....
		// Time to write out to the buffer
		WaitForSingleObject(ac->hMutex, INFINITE);
		if (DS_OK != lpdsbuff->Lock(write_pos, LOCK_SIZE, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0)) {
			MessageBox(NULL, "Error locking sound buffer", PLUGIN_VERSION, MB_OK | MB_ICONSTOP);
			goto _exit_;
		}
		// Fills dwBytes to the Sound Buffer
		ac->FillBuffer((BYTE *)lpvPtr1, dwBytes1);
		if (dwBytes2) { ac->FillBuffer((BYTE *)lpvPtr2, dwBytes2); dprintf("P"); }
		//
		if FAILED(lpdsbuff->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2)) {
			MessageBox(NULL, "Error unlocking sound buffer", PLUGIN_VERSION, MB_OK | MB_ICONSTOP);
			goto _exit_;
		}
		ReleaseMutex(ac->hMutex);

		//Sleep(10);
	}

_exit_:
	ReleaseMutex(ac->hMutex);
	ac->handleAudioThread = NULL;
	ExitThread(0);
	return 0;
}





//------------------------------------------------------------------------

// Setup and Teardown Functions

// Generates nice alignment with N64 samples...
void DirectSoundDriver::SetSegmentSize(DWORD length) {

	DSBUFFERDESC        dsbdesc;
	WAVEFORMATEX        wfm;
	HRESULT             hr;

	if (SampleRate == 0) { return; }
	SegmentSize = length;

	WaitForSingleObject(hMutex, INFINITE);
	memset(&wfm, 0, sizeof(WAVEFORMATEX));

	wfm.wFormatTag = WAVE_FORMAT_PCM;
	wfm.nChannels = 2;
	wfm.nSamplesPerSec = SampleRate;
	wfm.wBitsPerSample = 16;
	wfm.nBlockAlign = wfm.wBitsPerSample / 8 * wfm.nChannels;
	wfm.nAvgBytesPerSec = wfm.nSamplesPerSec * wfm.nBlockAlign;

	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	//dsbdesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPOSITIONNOTIFY;
#if defined(_XBOX)
	dsbdesc.dwFlags	= DSBCAPS_CTRLPOSITIONNOTIFY;
#else
	dsbdesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_LOCSOFTWARE;
#endif
	dsbdesc.dwBufferBytes = SegmentSize * DS_SEGMENTS;
	dsbdesc.lpwfxFormat = &wfm;

	hr = IDirectSound_CreateSoundBuffer(lpds, &dsbdesc, &lpdsbuf, NULL);
	assert(!FAILED(hr));

	IDirectSoundBuffer_Play(lpdsbuf, 0, 0, DSBPLAY_LOOPING);
	lpdsbuff = this->lpdsbuf;
	ReleaseMutex(hMutex);
}

LPDIRECTSOUNDBUFFER lpdsb = NULL;
// TODO: Should clear out AI registers on romopen and initialize
BOOL DirectSoundDriver::Initialize(HWND hwnd) {
	audioIsPlaying = FALSE;

	DSBUFFERDESC        dsPrimaryBuff;
	WAVEFORMATEX        wfm;
	HRESULT             hr;

	DeInitialize(); // Release just in case...

	dprintf("Initialize()\n");
	hMutex = CreateMutex(NULL, FALSE, NULL);

	WaitForSingleObject(hMutex, INFINITE);

#if defined(_XBOX)
	hr = DirectSoundCreate(NULL, &lpds, NULL);
#else
	RefreshDevices();

	if (devEnumFailed) {
		hr = DirectSoundCreate8(NULL, &lpds, NULL);
	} else {
		hr = DirectSoundCreate8(deviceList[configDeviceIdx].devGUID, &lpds, NULL);
	}
#endif
//	assert(!FAILED(hr)); // This happens if there is no sound device.
	if (FAILED(hr))
		return -2;

	if (FAILED(hr = IDirectSound_SetCooperativeLevel(lpds, hwnd, DSSCL_PRIORITY))) {
		return -1;
	}

	if (lpdsbuf) {
		IDirectSoundBuffer_Release(lpdsbuf);
		lpdsbuf = NULL;
	}
	memset(&dsPrimaryBuff, 0, sizeof(DSBUFFERDESC));

	dsPrimaryBuff.dwSize = sizeof(DSBUFFERDESC);
#if defined(_XBOX)
	dsPrimaryBuff.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY;
#else
	dsPrimaryBuff.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
#endif
	dsPrimaryBuff.dwBufferBytes = 0;
	dsPrimaryBuff.lpwfxFormat = NULL;
	memset(&wfm, 0, sizeof(WAVEFORMATEX));

	wfm.wFormatTag = WAVE_FORMAT_PCM;
	wfm.nChannels = 2;
	wfm.nSamplesPerSec = 44100;
	wfm.wBitsPerSample = 16;
	wfm.nBlockAlign = wfm.wBitsPerSample / 8 * wfm.nChannels;
	wfm.nAvgBytesPerSec = wfm.nSamplesPerSec * wfm.nBlockAlign;

	hr = IDirectSound_CreateSoundBuffer(lpds, &dsPrimaryBuff, &lpdsb, NULL);

	if (SUCCEEDED(hr)) {
		IDirectSoundBuffer_SetFormat(lpdsb, &wfm);
		IDirectSoundBuffer_Play(lpdsb, 0, 0, DSBPLAY_LOOPING);
	}

	ReleaseMutex(hMutex);

	SetSegmentSize(LOCK_SIZE);

	dprintf("Init Success...\n");

	DMALen[0] = DMALen[1] = 0;
	DMAData[0] = DMAData[1] = NULL;
	return FALSE;
}

static BOOL CALLBACK DSEnumCount(_GUID* devGUID, const char* devDescStr, const char* devModuleStr, void* data) {
	dprintf("AziDS8: DSEnumCount called.\n");
	UNREFERENCED_PARAMETER(devDescStr);
	UNREFERENCED_PARAMETER(devModuleStr);
	UNREFERENCED_PARAMETER(devGUID);
	DirectSoundDriver* snd = (DirectSoundDriver*)data;
	snd->numDevices++;
	return TRUE;
}

static BOOL CALLBACK DSEnumProc(LPGUID devGUID, LPCSTR devDescStr, LPCSTR devModuleStr, LPVOID data) {
	dprintf("AziDS8: DSEnumProc called.\n");
	DirectSoundDriver* snd = (DirectSoundDriver*)data;
	unsigned int devStrLen;

	dprintf("AziDS8: Device %u - Desc: %s; Module: %s.\n", snd->devicesEnumerated+1, devDescStr, devModuleStr);

	snd->deviceList[snd->devicesEnumerated].devGUID = devGUID;

	devStrLen = strlen(devDescStr) + 1;
	dprintf("AziDS8: Desc is %u chars long.\n", devStrLen);
	snd->deviceList[snd->devicesEnumerated].devDescStr = (char*)malloc(devStrLen * sizeof(char));
	strncpy(snd->deviceList[snd->devicesEnumerated].devDescStr, devDescStr, devStrLen);

	devStrLen = strlen(devModuleStr) + 1;
	dprintf("AziDS8: Module is %u chars long.\n", devStrLen);
	snd->deviceList[snd->devicesEnumerated].devModuleStr = (char*)malloc(devStrLen * sizeof(char));
	strncpy(snd->deviceList[snd->devicesEnumerated].devModuleStr, devModuleStr, devStrLen);

	dprintf("AziDS8: Desc: %s; Module: %s.\n", snd->deviceList[snd->devicesEnumerated].devDescStr, snd->deviceList[snd->devicesEnumerated].devModuleStr);

	snd->devicesEnumerated++;
	return TRUE;
}

BOOL DirectSoundDriver::RefreshDevices() {
	dprintf("AziDS8: RefreshDevices called.\n");
	numDevices = 0;

	if (DirectSoundEnumerate(DSEnumCount, (void*)this) != DS_OK) {
		DummyDevEnum();
		return false;
	}
	dprintf("AziDS8: Found %u devices.\n", numDevices);

	if (deviceList) {
		for (int i = 0; i < numDevices; i++) {
			free(deviceList[i].devGUID);
			free(deviceList[i].devDescStr);
			free(deviceList[i].devModuleStr);
		}
		free(deviceList);
	}

	deviceList = (DirectSoundDeviceID*)malloc(sizeof(DirectSoundDeviceID) * numDevices);
	devicesEnumerated = 0;

	if (DirectSoundEnumerate(DSEnumProc, (void*)this) != DS_OK) {
		DummyDevEnum();
		return false;
	}
	devEnumFailed = false;
	return true;
}

void DirectSoundDriver::DummyDevEnum()
{
	devEnumFailed = true;
	numDevices = 1;
}

BOOL DirectSoundDriver::SwitchDevice(unsigned int deviceNum)
{
	configDeviceIdx = deviceNum;
	if (devEnumFailed)
		return true;

	if (lpds) {
		DeInitialize();
		Initialize(NULL);
	}
	return true;
}

char* DirectSoundDriver::GetDeviceName(unsigned int devNum)
{
	char* name;
	if (devEnumFailed)
	{
		int devNameLen = strlen(DummyDevStr) +1;
		name = (char*)malloc(devNameLen * sizeof(char));
		strncpy(name, DummyDevStr, devNameLen);
	} else {
		int devNameLen = strlen(deviceList[devNum].devDescStr) +1;
		name = (char*)malloc(devNameLen * sizeof(char));
		strncpy(name, deviceList[devNum].devDescStr, devNameLen);
	}
	return name;
}

void DirectSoundDriver::DeInitialize() {

	dprintf("DeInitialize()\n");
	audioIsDone = true;
	StopAudio();
	if (lpdsbuf) {
		IDirectSoundBuffer_Stop(lpdsbuf);
		IDirectSoundBuffer_Release(lpdsbuf);
		lpdsbuf = NULL;
	}
	if (lpds) {
		IDirectSound_Release(lpds);
		lpds = NULL;
	}
	if (hMutex) {
		CloseHandle(hMutex);
		hMutex = NULL;
	}

	lpdsbuf = NULL;
	lpds = NULL;
	audioIsDone = false;
	hMutex = NULL;
	handleAudioThread = NULL;
	audioIsPlaying = FALSE;
	readLoc = 0;
	writeLoc = 0;
	remainingBytes = 0;
	DMALen[0] = DMALen[0] = 0;
	DMAData[0] = DMAData[0] = NULL;
}

DirectSoundDriver::DirectSoundDriver() {
	lpdsbuf = NULL;
	lpds = NULL;
	audioIsDone = false;
	hMutex = NULL;
	handleAudioThread = NULL;
	audioIsPlaying = FALSE;
	readLoc = 0;
	writeLoc = 0;
	remainingBytes = 0;
	configDeviceIdx = 0;
}

DirectSoundDriver::~DirectSoundDriver() {
	if (deviceList) {
		for (int i = 0; i < numDevices; i++) {
			free(deviceList[i].devGUID);
			free(deviceList[i].devDescStr);
			free(deviceList[i].devModuleStr);
		}
		free(deviceList);
		deviceList = NULL;
	}
}

// ---------BLAH--------

// Buffer Functions for the Audio Code
void DirectSoundDriver::SetFrequency(DWORD Frequency2) {

	DWORD Frequency = Frequency2;
	BOOL bAudioPlaying = audioIsPlaying;

	printf("SetFrequency()\n");
	// MusyX - (Frequency / 80) * 4
	// 
	if (bAudioPlaying == TRUE) {
		StopAudio();
	}

	sLOCK_SIZE = (Frequency / 80) * 4;// 0x600;// (22050 / 30) * 4;// 0x4000;// (Frequency / 60) * 4;
	SampleRate = Frequency;
	SegmentSize = 0; // Trash it... we need to redo the Frequency anyway...
	SetSegmentSize(LOCK_SIZE);
	printf("Frequency: %i - SegmentSize: %i\n", Frequency, SegmentSize);
	lastLength = 0;
	writeLoc = 0x0000;
	readLoc = 0x0000;
	remainingBytes = 0;
	//DMAData[0] = DMAData[1] = NULL;
	//DMALen[0] = DMALen[1] = NULL;
	*AudioInfo.AI_STATUS_REG = 0;
	if (bAudioPlaying == TRUE) {
		StartAudio();
	}
}

void DirectSoundDriver::AiUpdate(BOOL Wait) {

	if (Wait)
		WaitMessage();
	return;

	if (configForceSync && (*AudioInfo.AI_STATUS_REG & 0x80000000)) {
		if (remainingBytes < LOCK_SIZE * 2) {
			*AudioInfo.AI_STATUS_REG &= ~0x80000000;
			*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
			AudioInfo.CheckInterrupts();
			interruptcnt--;
		}
	}
}

#ifdef STREAM_DMA
DWORD DirectSoundDriver::AddBuffer(BYTE *start, DWORD length) {
	//DWORD retVal = 0;

	if (length == 0) {
		*AudioInfo.AI_STATUS_REG &= ~0xC0000001;
		*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
		AudioInfo.CheckInterrupts();
		return 0;
	}

	//test.WriteData(start, length);
	if (lastLength != length)
	{
		//printf("LL: %i\n", length);
		lastLength = length;
	}
	/*
	if (configSyncAudio && (DMALen[1] > 0) && ((*AudioInfo.AI_CONTROL_REG & 0x01) == 1)) {
	while (DMAData[1] != NULL) {
	Sleep(1);
	}
	}*/

	//WaitForSingleObject(hMutex, INFINITE);
	if (DMAData[0] == NULL) {
		*AudioInfo.AI_STATUS_REG |= 0x40000001;
		DMAData[0] = start;
		DMALen[0] = length;
		//dprintf("0");
	}
	else if (DMAData[1] == NULL) {
		*AudioInfo.AI_STATUS_REG |= 0x80000001;
		DMAData[1] = start;
		DMALen[1] = length;
		//dprintf("1");
	}
	else
	{
		//*AudioInfo.AI_STATUS_REG |= 0x80000001;
		//DMAData[2] = start;
		//DMALen[2] = length;
		dprintf("$");
	}
	//ReleaseMutex(hMutex);

	if (!audioIsPlaying)
	{
		StartAudio();
	}
	return length;
}
#else
DWORD DirectSoundDriver::AddBuffer(BYTE *start, DWORD length) {
	DWORD retVal = 0;
	DWORD max = remainingBytes + length;
	// One DMA buffer = one interrupt
	interruptcnt++;

	// Store the size of the last buffer length.  Used to give "accurate" length feedback
	if (lastLength != length) {
		//dprintf ("len: %i\n", length);
		lastLength = length;
	}

	//dprintf ("B");
	if ((length == 0) || (lpdsbuf == NULL)) {
		*AudioInfo.AI_STATUS_REG &= ~0xC0000001;
		*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
		AudioInfo.CheckInterrupts();
		interruptcnt--;
		//dprintf ("I");
		return 0;
	}

	//test.WriteData(start, length);

	if (!audioIsPlaying)
	{
		//test.BeginWaveOut("D:\\test.wav", 2, 16,SampleRate);
		StartAudio();
	}

	if (max > MAXBUFFER) {
		if (lastLength != length) {
			lastLength = length;
			dprintf("\nlast: %i, len: %i, MB: %i, SEG: %i\n", lastLength, length, MAXBUFFER, LOCK_SIZE);
		}
		dprintf("\nlast: %i, len: %i, MB: %i, SEG: %i\n", lastLength, length, MAXBUFFER, LOCK_SIZE);
		dprintf(",");
	}
	if (configSyncAudio && (max > MAXBUFFER)) {
		while ((remainingBytes + length) > MAXBUFFER) { // Halve the buffer...
			Sleep(1);
		}
	}
	else if (max > MAXBUFFER) {
		// Toss out the extra data...
		return retVal;
	}
	if (configForceSync) {
		//bool test;
		if (remainingBytes > LOCK_SIZE * 2) {
			*AudioInfo.AI_STATUS_REG |= 0xC0000000;
		}
		while (remainingBytes > LOCK_SIZE * 2) { // Force buffer sync
			Sleep(1);
		}
	}
	/*
	while (remainingBytes > lastLength) { // Force buffer sync
	Sleep(1);
	}*/

	WaitForSingleObject(hMutex, INFINITE);

	// Move our audio data from N64 memory to our SoundBuffer
	for (DWORD x = 0; x < length; x += 4) {
		SoundBuffer[writeLoc++] = start[x + 2];
		SoundBuffer[writeLoc++] = start[x + 3];
		SoundBuffer[writeLoc++] = start[x];
		SoundBuffer[writeLoc++] = start[x + 1];
		/*SoundBuffer[writeLoc++] = start[x];
		SoundBuffer[writeLoc++] = start[x+1];
		SoundBuffer[writeLoc++] = start[x+2];
		SoundBuffer[writeLoc++] = start[x+3];*/
		//writeLoc+=4; 
		remainingBytes += 4;
		if (writeLoc == MAXBUFFER)
			writeLoc = 0;
	}
	ReleaseMutex(hMutex);

	if (configAIEmulation && !configForceSync) {
		// If the next anticipated length is greater than MAXBUFFER
		// Make the audio status busy
		if ((remainingBytes + length) > MAXBUFFER) {
			*AudioInfo.AI_STATUS_REG |= 0xC0000000;
		}
		else {
			// Otherwise we are not so busy.  Execute an interrupt
			//if (remainingBytes <= TOTAL_SIZE)
				{
					*AudioInfo.AI_STATUS_REG &= ~0x80000000;
					*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
					AudioInfo.CheckInterrupts();
					interruptcnt--;
					//dprintf("i");
				}
		}
	}


	//if (interruptcnt > 1)
	//	dprintf ("NO!!!\n");


	return retVal;
}
#endif // Stream DMA

// Management functions
// TODO: For silent emulation... the Audio should still be "processed" somehow...
void DirectSoundDriver::StopAudio() {
	dprintf("StopAudio()\n");
	if (!audioIsPlaying) return;
	//test.EndWaveOut();
	audioIsPlaying = FALSE;
	audioIsDone = true;
	Sleep(100);
	TerminateThread(this->handleAudioThread, 0);
}

void DirectSoundDriver::StartAudio() {
	dprintf("StartAudio()\n");
	if (audioIsPlaying) return;
	audioIsPlaying = TRUE;
	audioIsDone = false;
	writeLoc = 0x0000;
	readLoc = 0x0000;
	remainingBytes = 0;
	this->handleAudioThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)AudioThreadProc, this, NULL, &this->dwAudioThreadId);
	//test.BeginWaveOut("D:\\test.wav", 2, 16, SampleRate);
}

DWORD DirectSoundDriver::GetReadStatus() {
	if (configForceSync)
		return 0;//remainingBytes;
	if (configAIEmulation == true) {
#ifdef STREAM_DMA
		return DMALen[0] & ~7;
		if (remainingBytes < LOCK_SIZE)
			return 0;
		else return DMALen[0] & ~3;
#else
		if (remainingBytes < (LOCK_SIZE * 2)) {
			return 0;
		}
		else { // This was the problem with SP_DMA_READ Error
			if (lastLength == 0) return 0;
			if (remainingBytes > lastLength) return (remainingBytes % lastLength) & ~0x3;
			else return remainingBytes & ~0x3;
			//return 0;//remainingBytes;
		}
#endif
	}
	else {
		return 0;
	}
}


void DirectSoundDriver::SetVolume(DWORD volume) {
	DWORD dsVolume = (DWORD)((volume * -25));
	if (volume == 100) dsVolume = DSBVOLUME_MIN;
	if (volume = 0) dsVolume = DSBVOLUME_MAX;
	if (lpdsb != NULL) lpdsb->SetVolume(dsVolume);
}

#endif
