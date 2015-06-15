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
#ifdef USE_XAUDIO2
#include "XAudio2SoundDriver.h"
#include "AudioSpec.h"
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#ifdef XA2_NEW_API
#include <mmdeviceapi.h>
#endif

#define USE_PRINTF

#ifdef USE_PRINTF
#define dprintf printf
#define dwprintf wprintf
#else
#define dprintf //
#define dwprintf //
#endif

static IXAudio2SourceVoice* g_source;
static IXAudio2MasteringVoice* g_master;
static IXAudio2* g_engine;

static bool audioIsPlaying = false;
static bool canPlay = false;

static BYTE bufferData[10][44100 * 4];
static int bufferLength[10];
static int writeBuffer = 0;
static int readBuffer = 0;
static int filledBuffers;
static int bufferBytes;
static int lastLength = 1;

static int cacheSize = 0;
static int interrupts = 0;
static VoiceCallback voiceCallback;

static char DummyDevStr[] = "Default XAudio2 Device";
//static CLSID CLSID_MMDeviceEnumerator = {
//	0xBCDE0395, 0xE52F, 0x467C,
//	{0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}
//};
//static IID IID_IMMDeviceEnumerator = {
//	0xA95664D2, 0x9614, 0x4F35,
//	{0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}
//};
static PROPERTYKEY PKEY_AudioEndpoint_Path = {
	{0x9c119480, 0xddc2, 0x4954,
	{0xa1, 0x50, 0x5b, 0xd2, 0x40, 0xd4, 0x54, 0xad}},
	1
};

XAudio2SoundDriver::XAudio2SoundDriver()
{
	g_engine = NULL;
	g_source = NULL;
	g_master = NULL;
	dllInitialized = false;
	devEnumFailed = false;
	deviceList = NULL;
	configDeviceIdx = 0;
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
}


XAudio2SoundDriver::~XAudio2SoundDriver()
{
	DeInitialize();
	//Teardown();
	CoUninitialize();

	if (deviceList)
	{
		for (int i = 0; i < numDevices; i++)
		{
			free(deviceList[i].devName);
			free(deviceList[i].devIdStr);
		}
		free(deviceList);
		deviceList = NULL;
	}
}
static HANDLE hMutex;


BOOL XAudio2SoundDriver::Initialize(HWND hwnd)
{
	UNREFERENCED_PARAMETER(hwnd);
	if (g_source != NULL)
	{
		g_source->Start();
	}
	bufferLength[0] = bufferLength[1] = bufferLength[2] = bufferLength[3] = bufferLength[4] = bufferLength[5] = 0;
	bufferLength[6] = bufferLength[7] = bufferLength[8] = bufferLength[9] = 0;
	audioIsPlaying = false;
	writeBuffer = 0;
	readBuffer = 0;
	filledBuffers = 0;
	bufferBytes = 0;
	lastLength = 1;

	cacheSize = 0;
	interrupts = 0;

	hMutex = CreateMutex(NULL, FALSE, NULL);
	if (FAILED(XAudio2Create(&g_engine)))
	{
		dprintf("AziXA2: XAudio2Create failed.\n");
		CoUninitialize();
		return -1;
	}

	RefreshDevices();

	return false;
}

BOOL XAudio2SoundDriver::Setup()
{
	if (dllInitialized == true) return true;
	dllInitialized = true;
	bufferLength[0] = bufferLength[1] = bufferLength[2] = bufferLength[3] = bufferLength[4] = bufferLength[5] = 0;
	bufferLength[6] = bufferLength[7] = bufferLength[8] = bufferLength[9] = 0;
	audioIsPlaying = false;
	writeBuffer = 0;
	readBuffer = 0;
	filledBuffers = 0;
	bufferBytes = 0;
	lastLength = 1;

	cacheSize = 0;
	interrupts = 0;

	if (devEnumFailed)
	{
		if (FAILED(g_engine->CreateMasteringVoice(&g_master)))
		{
			g_engine->Release();
			CoUninitialize();
			return -2;
		}
	} else {
		if (configDeviceIdx >= numDevices)
			configDeviceIdx = 0;
		dwprintf(L"AziXA2: Opening audio device %s, ID: %ls.\n", deviceList[configDeviceIdx].devName, deviceList[configDeviceIdx].devIdStr);
#ifdef XA2_NEW_API
		if (FAILED(g_engine->CreateMasteringVoice(&g_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, deviceList[configDeviceIdx].devIdStr)))
		{
			dprintf("AziXA2: Failed opening audio device.\n");
			g_engine->Release();
			CoUninitialize();
			return -2;
		}
#else
		if (configDeviceIdx >= numDevices)
			configDeviceIdx = 0;
		if (FAILED(g_engine->CreateMasteringVoice(&g_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, deviceList[configDeviceIdx].index)))
		{
			dprintf("AziXA2: Failed opening audio device.\n");
			g_engine->Release();
			CoUninitialize();
			return -2;
		}
#endif
	}
	canPlay = true;

	// Load Wave File

	WAVEFORMATEX wfm;

	memset(&wfm, 0, sizeof(WAVEFORMATEX));

	wfm.wFormatTag = WAVE_FORMAT_PCM;
	wfm.nChannels = 2;
	wfm.nSamplesPerSec = 44100;
	wfm.wBitsPerSample = 16; // TODO: Allow 8bit audio...
	wfm.nBlockAlign = wfm.wBitsPerSample / 8 * wfm.nChannels;
	wfm.nAvgBytesPerSec = wfm.nSamplesPerSec * wfm.nBlockAlign;


	if (FAILED(g_engine->CreateSourceVoice(&g_source, &wfm, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceCallback, NULL, NULL)))
	{
		g_engine->Release();
		CoUninitialize();
		return -3;
	}

	g_source->Start();
	
	return FALSE;
}

BOOL XAudio2SoundDriver::RefreshDevices()
{
	dprintf("AziXA2: RefreshDevices called.\n");
	unsigned int nDevices, devStrLen, i, j = 0;
#ifdef XA2_NEW_API
	IMMDeviceEnumerator* immDevEnum;
	IMMDeviceCollection* immDevList;
	IMMDevice* immDev;

	if (FAILED(CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**) &immDevEnum)))
	{
		dprintf("AziXA2: CoCreateInstance failed.\n");
		DummyDevEnum();
		return false;
	}

	if (FAILED(immDevEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &immDevList)))
	{
		dprintf("AziXA2: EnumAudioEndpoints failed.\n");
		DummyDevEnum();
		return false;
	}

	if (FAILED(immDevList->GetCount(&numDevices)))
	{
		dprintf("AziXA2: GetCount failed.\n");
		DummyDevEnum();
		return false;
	}

	if (deviceList)
	{
		for (int i = 0; i < numDevices; i++)
		{
			free(deviceList[i].devName);
			free(deviceList[i].devIdStr);
		}
		free(deviceList);
	}

	deviceList = (XAudio2DeviceID*)malloc(sizeof(XAudio2DeviceID) * numDevices);
	nDevices = numDevices;
	dprintf("AziXA2: %u devices found.\n", numDevices);

	for (i = 0; i < nDevices; i++)
	{
		dprintf("AziXA2: Getting info on device %u.\n", i);
		IPropertyStore *propStore;
		PROPVARIANT varDevName;
		PROPVARIANT varDevID;
		PropVariantInit(&varDevName);
		PropVariantInit(&varDevID);
		if (FAILED(immDevList->Item(i, &immDev)))
		{
			dprintf("AziXA2: Item failed.\n");
			PropVariantClear(&varDevName);
			PropVariantClear(&varDevID);

			if (immDev)
				immDev->Release();

			numDevices--;
			continue;
		}
		if (FAILED(immDev->OpenPropertyStore(STGM_READ, &propStore)))
		{
			dprintf("AziXA2: OpenPropertyStore failed.\n");
			PropVariantClear(&varDevName);
			PropVariantClear(&varDevID);

			if (propStore)
				propStore->Release();

			immDev->Release();

			numDevices--;
			continue;
		}
		if (FAILED(propStore->GetValue(PKEY_Device_FriendlyName, &varDevName)))
		{
			dprintf("AziXA2: GetValue (devname) failed.\n");
			PropVariantClear(&varDevName);
			PropVariantClear(&varDevID);
			
			propStore->Release();
			immDev->Release();

			numDevices--;
			continue;
		}
		if (FAILED(propStore->GetValue(PKEY_AudioEndpoint_Path, &varDevID)))
		{
			dprintf("AziXA2: GetValue (devname) failed.\n");
			PropVariantClear(&varDevName);
			PropVariantClear(&varDevID);

			propStore->Release();
			immDev->Release();

			numDevices--;
			continue;
		}
		devStrLen = wcslen(varDevName.pwszVal);
		deviceList[j].devName = (char*)malloc(devStrLen * sizeof(char));
		wcstombs(deviceList[j].devName, varDevName.pwszVal, devStrLen);
		devStrLen = wcslen(varDevID.pwszVal);
		deviceList[j].devIdStr = (wchar_t*)malloc(devStrLen * sizeof(wchar_t));
		wcscpy_s(deviceList[j].devIdStr, devStrLen, varDevID.pwszVal);

		PropVariantClear(&varDevName);
		PropVariantClear(&varDevID);

		propStore->Release();
		immDev->Release();

		dprintf("AziXA2: Device %u name: %s\n", deviceList[j].devName);
		dwprintf(L"AziXA2: Device %u ID: %ls\n", deviceList[j].devIdStr);
		j++;
	}
#else
	XAUDIO2_DEVICE_DETAILS devDetails;
	if (FAILED(g_engine->GetDeviceCount(&numDevices)))
	{
		dprintf("AziXA2: GetDeviceCount failed.\n");
		DummyDevEnum();
		return false;
	}

	if (deviceList)
	{
		for (int i = 0; i < numDevices; i++)
		{
			free(deviceList[i].devName);
			free(deviceList[i].devIdStr);
		}
		free(deviceList);
	}

	deviceList = (XAudio2DeviceID*)malloc(sizeof(XAudio2DeviceID) * numDevices);
	nDevices = numDevices;
	dprintf("AziXA2: %u devices found.\n", numDevices);

	for (i = 0; i < nDevices; i++)
	{
		dprintf("AziXA2: Getting info on device %u.\n", i);
		if (FAILED(g_engine->GetDeviceDetails(i, &devDetails)))
		{
			dprintf("AziXA2: GetDeviceDetails failed.\n");
			numDevices--;
			continue;
		}
		devStrLen = wcslen(devDetails.DisplayName) + 1;
		deviceList[j].devName = (char*)malloc(devStrLen * sizeof(char));
		wcstombs(deviceList[j].devName, devDetails.DisplayName, devStrLen);
		devStrLen = wcslen(devDetails.DeviceID) + 2;
		deviceList[j].devIdStr = (wchar_t*)malloc(devStrLen * sizeof(wchar_t));
		wcsncpy(deviceList[j].devIdStr, devDetails.DeviceID, devStrLen);

		dprintf("AziXA2: Device %u name: %s\n", deviceList[j].devName);
		dwprintf(L"AziXA2: Device %u ID: %ls\n", deviceList[j].devIdStr);
		j++;
	}
#endif
	dprintf("AziXA2: %u useable devices found.\n", numDevices);

	if (numDevices)
	{
		devEnumFailed = false;
		return true;
	} else {
		DummyDevEnum();
		return false;
	}
}

void XAudio2SoundDriver::DummyDevEnum()
{
	dprintf("AziXA2: Device enumeration failed. We will use default device.\n");
	devEnumFailed = true;
	numDevices = 1;
}

BOOL XAudio2SoundDriver::SwitchDevice(unsigned int deviceNum)
{
	configDeviceIdx = deviceNum;
	if (configDeviceIdx >= numDevices)
	{
		dprintf("AziXA2: Attempting to switch to device of index %u. There are only %u devices. Using index 0.\n", deviceNum, numDevices);
		configDeviceIdx = 0;
	}
	if (devEnumFailed)
		return true;

	if (dllInitialized)
	{
		dwprintf(L"AziXA2: Opening audio device %s, ID: %ls.\n", deviceList[configDeviceIdx].devName, deviceList[configDeviceIdx].devIdStr);
#ifdef XA2_NEW_API		
		if (FAILED(g_engine->CreateMasteringVoice(&g_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, deviceList[configDeviceIdx].devIdStr)))
		{
			dprintf("AziXA2: Failed opening audio device.\n");
			// We might be fucked.
			return -2;
		}
#else
		if (FAILED(g_engine->CreateMasteringVoice(&g_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, deviceList[configDeviceIdx].index)))
		{
			dprintf("AziXA2: Failed opening audio device.\n");
			// We might be fucked.
			return -2;
		}
#endif
	}
	return true;
}

char* XAudio2SoundDriver::GetDeviceName(unsigned int devNum)
{
	char *name;
	if (devEnumFailed)
	{
		int devNameLen = strlen(DummyDevStr) + 1;
		name = (char*)malloc(devNameLen * sizeof(char));
		strncpy(name, DummyDevStr, devNameLen);
		//return &DummyDevStr[0];
	} else {
		if (devNum >= numDevices)
		{
			dprintf("AziXA2: Trying to get device name of device index %u. There are only %u devices. Returning dummy string.\n", devNum, numDevices);
			int devNameLen = strlen(DummyDevStr) +1;
			name = (char*)malloc(devNameLen * sizeof(char));
			strncpy(name, DummyDevStr, devNameLen);
		}
		int devNameLen = strlen(deviceList[devNum].devName) + 1;
		name = (char*)malloc(devNameLen * sizeof(char));
		strncpy(name, deviceList[devNum].devName, devNameLen);
		//return deviceList[devNum].devName;
	}
	return name;
}

void XAudio2SoundDriver::DeInitialize()
{
	Teardown();
	/*
	if (g_source != NULL)
	{
		g_source->Stop();
		g_source->FlushSourceBuffers();
		//g_source->DestroyVoice();
	}*/

}

void XAudio2SoundDriver::Teardown()
{
	if (dllInitialized == false) return;
	if (hMutex != NULL)
		WaitForSingleObject(hMutex, INFINITE);
	if (g_source != NULL)
	{
		g_source->Stop();
		g_source->FlushSourceBuffers();
		g_source->DestroyVoice();
	}
	
	if (g_master != NULL) g_master->DestroyVoice();
	if (g_engine != NULL && canPlay)
	{
		g_engine->StopEngine();
		g_engine->Release();
	}
	g_engine = NULL;
	g_master = NULL;
	g_source = NULL;
	if (hMutex != NULL)
	{
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
	}
	hMutex = NULL;
	dllInitialized = false;
}

void XAudio2SoundDriver::SetFrequency(DWORD Frequency)
{
	cacheSize = (Frequency / 25) * 4;// (((Frequency * 4) / 100) & ~0x3) * 8;
	if (Setup() < 0) /* failed to apply a sound device */
		return;
	g_source->SetSourceSampleRate(Frequency);
}

DWORD XAudio2SoundDriver::AddBuffer(BYTE *start, DWORD length)
{
	if (length == 0 || g_source == NULL) {
		*AudioInfo.AI_STATUS_REG = 0;
		*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
		AudioInfo.CheckInterrupts();
		return 0;
	}
	lastLength = length;

	// Gracefully waiting for filled buffers to deplete
	if (configSyncAudio == true || configForceSync == true)
		while (filledBuffers == 10) Sleep(1);
	else 
		if (filledBuffers == 10) return 0;
		
	WaitForSingleObject(hMutex, INFINITE);
	for (DWORD x = 0; x < length; x += 4)
	{
		bufferData[writeBuffer][x] = start[x + 2];
		bufferData[writeBuffer][x+1] = start[x + 3];
		bufferData[writeBuffer][x+2] = start[x];
		bufferData[writeBuffer][x+3] = start[x + 1];
	}
	bufferLength[writeBuffer] = length;
	bufferBytes += length;
	filledBuffers++;

	XAUDIO2_BUFFER xa2buff;

	xa2buff.Flags = XAUDIO2_END_OF_STREAM; // Suppress XAudio2 warnings
	xa2buff.PlayBegin = 0;
	xa2buff.PlayLength = 0;
	xa2buff.LoopBegin = 0;
	xa2buff.LoopLength = 0;
	xa2buff.LoopCount = 0;
	xa2buff.pContext = &bufferLength[writeBuffer];
	xa2buff.AudioBytes = length;
	xa2buff.pAudioData = bufferData[writeBuffer];
	if (canPlay)
		g_source->SubmitSourceBuffer(&xa2buff);

	++writeBuffer;
	writeBuffer %= 10;

	if (bufferBytes < cacheSize || configForceSync == true)
	{
		*AudioInfo.AI_STATUS_REG = AI_STATUS_DMA_BUSY;
		*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
		AudioInfo.CheckInterrupts();
	}
	else
	{
		if (filledBuffers >= 2)
			*AudioInfo.AI_STATUS_REG |= AI_STATUS_FIFO_FULL;
		interrupts++;
	}
	ReleaseMutex(hMutex);
	
	return 0;
}

void XAudio2SoundDriver::AiUpdate(BOOL Wait)
{
	if (Wait)
		WaitMessage();
}

void XAudio2SoundDriver::StopAudio()
{
	
	audioIsPlaying = false;
	if (g_source != NULL)
		g_source->FlushSourceBuffers();
		
}

void XAudio2SoundDriver::StartAudio()
{
	audioIsPlaying = true;
}

DWORD XAudio2SoundDriver::GetReadStatus()
{
	XAUDIO2_VOICE_STATE xvs;
	int retVal;

	if (canPlay)
		g_source->GetState(&xvs);
	else
		return 0;

//	printf("%i - %i - %i\n", xvs.SamplesPlayed, bufferLength[0], bufferLength[1]);

	if (xvs.BuffersQueued == 0 || configForceSync == true) return 0;

	if (bufferBytes + lastLength < cacheSize)
		return 0;
	else
		retVal = (lastLength - xvs.SamplesPlayed * 4) & ~0x7;// bufferBytes % lastLength;// *(int *)xvs.pCurrentBufferContext - (int)xvs.SamplesPlayed;

	if (retVal < 0) return 0; else return retVal % lastLength;
//	return 0;
}

// 100 - Mute to 0 - Full Volume
void XAudio2SoundDriver::SetVolume(DWORD volume)
{
	float xaVolume = 1.0f - ((float)volume / 100.0f);
	if (g_source != NULL) g_source->SetVolume(xaVolume);
	//XAUDIO2_MAX_VOLUME_LEVEL
}


void __stdcall VoiceCallback::OnBufferEnd(void * pBufferContext)
{
	WaitForSingleObject(hMutex, INFINITE);
#ifdef SEH_SUPPORTED
	__try // PJ64 likes to close objects before it shuts down the DLLs completely...
	{
#endif
		*AudioInfo.AI_STATUS_REG = AI_STATUS_DMA_BUSY;
		if (interrupts > 0)
		{
			interrupts--;
			*AudioInfo.MI_INTR_REG |= MI_INTR_AI;
			AudioInfo.CheckInterrupts();
		}
#ifdef SEH_SUPPORTED
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
	}
#endif
	bufferBytes -= *(int *)(pBufferContext);
	filledBuffers--;
	ReleaseMutex(hMutex);
}

void __stdcall VoiceCallback::OnVoiceProcessingPassStart(UINT32 SamplesRequired) 
{
	UNREFERENCED_PARAMETER(SamplesRequired);
	//if (SamplesRequired > 0)
	//	printf("SR: %i FB: %i BB: %i  CS:%i\n", SamplesRequired, filledBuffers, bufferBytes, cacheSize);
}
#endif
