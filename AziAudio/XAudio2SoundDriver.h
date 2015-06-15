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
//#define _WIN32_WINNT 0x0601
#include <Windows.h>
#include "SoundDriver.h"
#include <xaudio2.h>
#include <wchar.h>

const IID IID_IXAudio2_LEGACY = {
	0x8BCF1F58, 0x9FE7, 0x4583,
	{0x8A, 0xC6, 0xE2, 0xAD, 0xC4, 0x65, 0xC8, 0xBB}
};
const IID IID_IXAudio2_NEW  = {
	0x60D8DAC8, 0x5AA1, 0x4E8E,
	{0xB5, 0x97, 0x2F, 0x5E, 0x28, 0x83, 0xD4, 0x84}
};

const CLSID CLSID_XAudio2_27 = {
	0x8BCF1F58, 0x9FE7, 0x4583,
	{0x8A, 0xC6, 0xE2, 0xAD, 0xC4, 0x65, 0xC8, 0xBB}
};
const CLSID CLSID_XAudio2_27D = {
	0xDB05EA35, 0x0329, 0x4D4B,
	{0xA5, 0x3A, 0x6D, 0xEA, 0xD0, 0x3D, 0x38, 0x52}
};

const CLSID CLSID_XAudio2_26 = {
	0x3EDA9B49, 0x2085, 0x498B,
	{0x9B, 0xB2, 0x39, 0xA6, 0x77, 0x84, 0x93, 0xDE}
};
const CLSID CLSID_XAudio2_26D = {
	0x47199894, 0x7CC2, 0x444D,
	{0x98, 0x73, 0xCE, 0xD2, 0x56, 0x2C, 0xC6, 0x0E}
};

const CLSID CLSID_XAudio2_25 = {
	0x4C9B6DDE, 0x6809, 0x46E6,
	{0xA2, 0x78, 0x9B, 0x6A, 0x97, 0x58, 0x86, 0x70}
};
const CLSID CLSID_XAudio2_25D = {
	0x715BDD1A, 0xAA82, 0x436B,
	{0xB0, 0xFA, 0x6A, 0xCE, 0xA3, 0x9B, 0xD0, 0xA1}
};

const CLSID CLSID_XAudio2_24 = {
	0x03219E78, 0x5BC3, 0x44D1,
	{0xB9, 0x2E, 0xF6, 0x3D, 0x89, 0xCC, 0x65, 0x26}
};
const CLSID CLSID_XAudio2_24D = {
	0x4256535C, 0x1EA4, 0x4D4B,
	{0x8A, 0xD5, 0xF9, 0xDB, 0x76, 0x2E, 0xCA, 0x9E}
};

const CLSID CLSID_XAudio2_23 = {
	0x4C5E637A, 0x16C7, 0x4DE3,
	{0x9C, 0x46, 0x5E, 0xD2, 0x21, 0x81, 0x96, 0x2D}
};
const CLSID CLSID_XAudio2_23D = {
	0xEF0AA05D, 0x8075, 0x4E5D,
	{0xBE, 0xAD, 0x45, 0xBE, 0x0C, 0x3C, 0xCB, 0xB3}
};

const CLSID CLSID_XAudio2_22 = {
	0xB802058A, 0x464A, 0x42DB,
	{0xBC, 0x10, 0xB6, 0x50, 0xD6, 0xF2, 0x58, 0x6A}
};
const CLSID CLSID_XAudio2_22D = {
	0x97DFB7E7, 0x5161, 0x4015,
	{0x87, 0xA9, 0xC7, 0x9E, 0x6A, 0x19, 0x52, 0xCC}
};

const CLSID CLSID_XAudio2_21 = {
	0xE21A7345, 0xEB21, 0x468E,
	{0xBE, 0x50, 0x80, 0x4D, 0xB9, 0x7C, 0xF7, 0x08}
};
const CLSID CLSID_XAudio2_21D = {
	0xF7A76C21, 0x53D4, 0x46BB,
	{0xAC, 0x53, 0x8B, 0x45, 0x9C, 0xAE, 0x46, 0xBD}
};

const CLSID CLSID_XAudio2_20 = {
	0xFAC23F48, 0x31F5, 0x45A8,
	{0xB4, 0x9B, 0x52, 0x25, 0xD6, 0x14, 0x01, 0xAA}
};
const CLSID CLSID_XAudio2_20D = {
	0xFAC23F48, 0x31F5, 0x45A8,
	{0xB4, 0x9B, 0x52, 0x25, 0xD6, 0x14, 0x01, 0xDB}
};

#if XA2_DLL_VER == 28
# ifndef XA2_NEW_API
#  define XA2_NEW_API
# endif
# define AZI_CLSID_XAudio2 CLSID_XAudio2_27
# define AZI_CLSID_XAudio2_Debug CLSID_XAudio2_27D
# define AZI_IID_IXAudio2 IID_IXAudio2_NEW
#elif !defined(XA2_DLL_VER) || (XA2_DLL_VER > 27) || (XA2_DLL_VER < 20)
# define XA2_DLL_VER 24
  // Default to 2.4, the earliest version that will work
  // (Tested in wine-staging 1.7.42 201505151816 with xact installed via winetricks and wine 1.7.24)
#endif

#if XA2_DLL_VER == 27
# define AZI_CLSID_XAudio2 CLSID_XAudio2_27
# define AZI_CLSID_XAudio2_Debug CLSID_XAudio2_27D
#elif XA2_DLL_VER == 26
# define AZI_CLSID_XAudio2 CLSID_XAudio2_26
# define AZI_CLSID_XAudio2_Debug CLSID_XAudio2_26D
#elif XA2_DLL_VER == 25
# define AZI_CLSID_XAudio2 CLSID_XAudio2_25
# define AZI_CLSID_XAudio2_Debug CLSID_XAudio2_25D
#elif XA2_DLL_VER == 24
# define AZI_CLSID_XAudio2 CLSID_XAudio2_24
# define AZI_CLSID_XAudio2_Debug CLSID_XAudio2_24D
#elif XA2_DLL_VER == 23
# define AZI_CLSID_XAudio2 CLSID_XAudio2_23
# define AZI_CLSID_XAudio2_Debug CLSID_XAudio2_23D
#elif XA2_DLL_VER == 22
# define AZI_CLSID_XAudio2 CLSID_XAudio2_22
# define AZI_CLSID_XAudio2_Debug CLSID_XAudio2_22D
#elif XA2_DLL_VER == 21
# define AZI_CLSID_XAudio2 CLSID_XAudio2_21
# define AZI_CLSID_XAudio2_Debug CLSID_XAudio2_21D
#elif XA2_DLL_VER == 20
# define AZI_CLSID_XAudio2 CLSID_XAudio2_20
# define AZI_CLSID_XAudio2_Debug CLSID_XAudio2_20D
#endif

#ifndef AZI_IID_IXAudio2
# define AZI_IID_IXAudio2 IID_IXAudio2_LEGACY
#endif

#ifdef XA2_DEBUG_CLSID
# undef AZI_CLSID_XAudio2
# define AZI_CLSID_XAudio2 AZI_CLSID_XAudio2_Debug
#endif

class VoiceCallback : public IXAudio2VoiceCallback
{
public:
	//HANDLE hBufferEndEvent;
	VoiceCallback() /*: hBufferEndEvent(CreateEvent(NULL, FALSE, FALSE, NULL))*/{}
	~VoiceCallback(){/* CloseHandle(hBufferEndEvent); */}

	//Called when the voice has just finished playing a contiguous audio stream.
	void __stdcall OnStreamEnd() {/* SetEvent(hBufferEndEvent); */}

	//Unused methods are stubs
	
	void __stdcall OnVoiceProcessingPassEnd() { }
	void __stdcall OnVoiceProcessingPassStart(UINT32 SamplesRequired);// {}
	void __stdcall OnBufferEnd(void * pBufferContext);//    {}
	void __stdcall OnBufferStart(void * pBufferContext) { UNREFERENCED_PARAMETER(pBufferContext); }
	void __stdcall OnLoopEnd(void * pBufferContext) { UNREFERENCED_PARAMETER(pBufferContext); }
	void __stdcall OnVoiceError(void * pBufferContext, HRESULT Error) { UNREFERENCED_PARAMETER(pBufferContext); UNREFERENCED_PARAMETER(Error); }
	
	/*
	STDMETHOD_(void, OnVoiceProcessingPassStart) (THIS_ UINT32 BytesRequired);
	STDMETHOD_(void, OnVoiceProcessingPassEnd) (THIS);
	STDMETHOD_(void, OnStreamEnd) (THIS);
	STDMETHOD_(void, OnBufferStart) (THIS_ void* pBufferContext);
	STDMETHOD_(void, OnBufferEnd) (THIS_ void* pBufferContext);
	STDMETHOD_(void, OnLoopEnd) (THIS_ void* pBufferContext);
	STDMETHOD_(void, OnVoiceError) (THIS_ void* pBufferContext, HRESULT Error);
	*/
};

typedef struct XAudio2DeviceID
{
	unsigned int index;
	char* devName;
	wchar_t* devIdStr;

} XAudio2DeviceID;

class XAudio2SoundDriver :
	public SoundDriver
{
public:
	XAudio2SoundDriver();
	~XAudio2SoundDriver();
	
	// Setup and Teardown Functions
	BOOL Initialize(HWND hwnd);
	void DeInitialize();

	BOOL Setup();
	void Teardown();

	// Buffer Functions for the Audio Code
	void SetFrequency(DWORD Frequency);		// Sets the Nintendo64 Game Audio Frequency
	DWORD AddBuffer(BYTE *start, DWORD length);	// Uploads a new buffer and returns status

	// Management functions
	void AiUpdate(BOOL Wait);
	void StopAudio();							// Stops the Audio PlayBack (as if paused)
	void StartAudio();							// Starts the Audio PlayBack (as if unpaused)

	DWORD GetReadStatus();						// Returns the status on the read pointer

	void SetVolume(DWORD volume);

	BOOL SwitchDevice(unsigned int deviceNum);	// Switches devices
	char* GetDeviceName(unsigned int devNum);	// Gets a device name
	BOOL RefreshDevices();						// Refresh device list


protected:

	bool dllInitialized;
	bool devEnumFailed;
	XAudio2DeviceID* deviceList;

	void DummyDevEnum();
};

/*
 * The GNU C++ compiler (ported to Windows through MinGW, for example)
 * references system C++ headers that use `__in` and `__out` for things
 * related to C++, which conflicts with Microsoft's <sal.h> driver macros for
 * the XAudio2 API.  Perhaps either side could be blamed for this, but I
 * think that it shouldn't hurt to un-define the __in and __out stuff after
 * we have finished prototyping everything relevant to XAudio2.  -- cxd4
 */
#if !defined(_MSC_VER)
#undef __in
#undef __out
#endif
