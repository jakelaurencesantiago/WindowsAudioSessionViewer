#pragma once


#include <Windows.h>
#include <mfapi.h>
#include <string>
#include <vector>
#include <Functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <algorithm>
#include "ProcessInfoHeader.h"


#pragma comment(lib, "Mfplat.lib")

#define SAFE_RELEASE(punk)  \
		if ((punk) != NULL)  \
        { (punk)->Release(); (punk) = NULL; }
#define PSTR_MEM_FREE(pstr) \
		if((pstr) != NULL) \
		{ CoTaskMemFree(pstr); (pstr) = NULL; }

// #define STRING_TO_LOWER(str) transform(str.begin(), str.end(), str.begin(), towlower);
// #define STRING_TO_UPPER(str) transform(str.begin(), str.end(), str.begin(), towupper);


const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioSessionManager2 = __uuidof(IAudioSessionManager2);


struct AudioEndpointSessionInformation {
	AudioSessionState state = AudioSessionState::AudioSessionStateInactive;
	DWORD processId = 0;
	std::wstring processDisplayName;
	std::wstring processBaseName;
	std::wstring processPath;
};

struct AudioEndpointDeviceInformation {
	std::wstring deviceId;
	std::wstring friendlyName;
	std::wstring simpleDescription;
	std::wstring deviceName;
	EDataFlow dataFlow = eCapture;
	std::vector<AudioEndpointSessionInformation> sessions;
};

HRESULT GetEndpointPropertyValue(IPropertyStore* pPropStore, const PROPERTYKEY& key, VOID* outValue) {
	if (pPropStore == NULL || outValue == NULL) {
		return E_INVALIDARG;
	}

	HRESULT hr = ERROR_SUCCESS;
	PROPVARIANT propVariant;
	PropVariantInit(&propVariant);

	hr = pPropStore->GetValue(
		key, &propVariant);

	switch (propVariant.vt) {
	case VT_LPWSTR:
	{
		size_t len = wcslen(propVariant.pwszVal) + 1;

		if (len > 1) {
			WCHAR* pStr = new(std::nothrow) WCHAR[len];

			if (pStr != nullptr) {
				lstrcpyW(pStr, propVariant.pwszVal);

				*(LPWSTR*)outValue = pStr;
			}
		}
		break;
	}
	case VT_UI4:
		*((UINT*)outValue) = propVariant.uintVal;
		break;
	case VT_CLSID:
	{
		// Convert GUID to string
		LPWSTR pGuid = NULL;
		hr = StringFromCLSID(*propVariant.puuid, &pGuid);
		if (SUCCEEDED(hr)) {
			size_t len = wcslen(pGuid) + 1;
			if (len > 1) {
				WCHAR* pStr = new(std::nothrow) WCHAR[len];

				if (pStr != nullptr) {
					lstrcpyW(pStr, pGuid);

					*((LPWSTR*)outValue) = pStr;
				}

			}
			CoTaskMemFree(pGuid);
		}
		break;
	}
	case VT_BOOL:
		*((BOOL*)outValue) = propVariant.boolVal;
		break;
	default:
		if (SUCCEEDED(hr)) {
			// data was retrieved but no output value
			hr = ERROR_UNSUPPORTED_TYPE;
		}
		break;
	}

	PropVariantClear(&propVariant);

	return hr;
}

std::vector<AudioEndpointSessionInformation> GetMMDeviceSessionInfo(IMMDevice* pEndpoint) {

	IAudioSessionManager2* pSessManager2 = NULL;
	IAudioSessionControl* pSessControl = NULL;
	IAudioSessionControl2* pSessControl2 = NULL;
	IAudioSessionEnumerator* pSessEnum = NULL;

	HRESULT hr;

	std::vector<AudioEndpointSessionInformation> ret;


	if (pEndpoint == NULL) {
		return ret;
	}

	do {
		hr = pEndpoint->Activate(IID_IAudioSessionManager2, CLSCTX_ALL, NULL, (LPVOID*)&pSessManager2);
		if (FAILED(hr)) break;

		hr = pSessManager2->GetSessionEnumerator(&pSessEnum);
		if (FAILED(hr)) break;

		int sessCnt = 0;
		hr = pSessEnum->GetCount(&sessCnt);
		if (FAILED(hr)) break;

		for (int i = 0; i < sessCnt; i++) {
			hr = pSessEnum->GetSession(i, &pSessControl);
			if (FAILED(hr)) continue;
			
			hr = pSessControl->QueryInterface(&pSessControl2);
			if (FAILED(hr) || 
				pSessControl2->IsSystemSoundsSession() == S_OK // ignore system sound
				) {
				SAFE_RELEASE(pSessControl);
				continue;
			}

			AudioEndpointSessionInformation sessionInfo;
			DWORD processId = 0;
			AudioSessionState sessState;
			BOOL isDuplicate = TRUE;

			hr = pSessControl->GetState(&sessState);
			if (SUCCEEDED(hr)) {
				sessionInfo.state = sessState;
			}


			hr = pSessControl2->GetProcessId(&processId);
			if (SUCCEEDED(hr)) {
				sessionInfo.processId = processId;
				
				// Check if duplicate process exists
				const int cnt = std::count_if(ret.cbegin(), ret.cend(), 
					[id = processId](auto x) { return x.processId == id; });
				isDuplicate = cnt > 0;

				if (!isDuplicate) {
					// Get the process information such as process name
					ProcessInformation processInfo;
					if (GetProcessInfo(processId, &processInfo)) {
						sessionInfo.processBaseName = processInfo.processBaseName;
						sessionInfo.processDisplayName = processInfo.processDisplayName;
						sessionInfo.processPath = processInfo.processPath;
					}
				}
			}

			if (!isDuplicate) { // ignore duplicates
				ret.push_back(sessionInfo);
			}

			SAFE_RELEASE(pSessControl2);
			SAFE_RELEASE(pSessControl);
		}
	} while (0);

	SAFE_RELEASE(pSessControl2);
	SAFE_RELEASE(pSessControl);
	SAFE_RELEASE(pSessEnum);
	SAFE_RELEASE(pSessManager2);

	return ret;
}

HRESULT GetMMDeviceInfo(IMMDevice* pEndpoint, AudioEndpointDeviceInformation* audioEndpointInfo) {

	IPropertyStore* pProps = NULL;
	LPWSTR pStrInfo = NULL;

	HRESULT hr;


	if (pEndpoint == NULL || audioEndpointInfo == NULL) {
		return E_INVALIDARG;
	}

	do {
		hr = pEndpoint->OpenPropertyStore(
			STGM_READ, &pProps);
		if (FAILED(hr)) break;

		/*****************************/
		// Get the endpoint ID string.
		/*****************************/
		hr = pEndpoint->GetId(&pStrInfo);
		if (FAILED(hr)) break;

		audioEndpointInfo->deviceId = std::wstring(pStrInfo);

		/*****************************/
		// Get the endpoint's properties.
		/*****************************/
		hr = GetEndpointPropertyValue(pProps, PKEY_Device_FriendlyName, &pStrInfo);
		if (FAILED(hr)) break;

		if (pStrInfo != NULL) {
			audioEndpointInfo->friendlyName = std::wstring(pStrInfo);

			delete[] pStrInfo; // release char array
			pStrInfo = NULL;
		}


		hr = GetEndpointPropertyValue(pProps, PKEY_Device_DeviceDesc, &pStrInfo);
		if (FAILED(hr)) break;

		if (pStrInfo != NULL) {
			audioEndpointInfo->simpleDescription = std::wstring(pStrInfo);

			delete[] pStrInfo; // release char array
			pStrInfo = NULL;
		}

		hr = GetEndpointPropertyValue(pProps, PKEY_DeviceInterface_FriendlyName, &pStrInfo);
		if (FAILED(hr)) break;

		if (pStrInfo != NULL) {
			audioEndpointInfo->deviceName = std::wstring(pStrInfo);

			delete[] pStrInfo; // release char array
			pStrInfo = NULL;
		}


		SAFE_RELEASE(pProps);

	} while (0);


	PSTR_MEM_FREE(pStrInfo);
	SAFE_RELEASE(pProps);

	return hr;
}


std::vector<AudioEndpointDeviceInformation> GetAudioEndpoints(EDataFlow dataFlow)
{
	HRESULT hr = S_OK;
	IMMDeviceEnumerator* pEnumerator = NULL;
	IMMDeviceCollection* pCollection = NULL;
	IMMDevice* pEndpoint = NULL;
	UINT nConnectorCnt = 0;

	std::vector<AudioEndpointDeviceInformation> ret;

	do {

		hr = CoInitializeEx(0, COINIT_MULTITHREADED);
		if (FAILED(hr)) break;

		// Make device enumerator instance
		hr = CoCreateInstance(
			CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, IID_IMMDeviceEnumerator,
			(void**)&pEnumerator);
		if (FAILED(hr)) break;

		// Get device collection pointer
		hr = pEnumerator->EnumAudioEndpoints(
			dataFlow, DEVICE_STATE_ACTIVE,
			&pCollection);
		if (FAILED(hr)) break;

		UINT  deviceCnt;
		hr = pCollection->GetCount(&deviceCnt);
		if (FAILED(hr)) break;

		for (ULONG i = 0; i < deviceCnt; i++)
		{
			AudioEndpointDeviceInformation audioEndpointInfo;
			audioEndpointInfo.dataFlow = dataFlow;

			// Get pointer to endpoint number i.
			hr = pCollection->Item(i, &pEndpoint);
			if (FAILED(hr)) break;

			hr = GetMMDeviceInfo(pEndpoint, &audioEndpointInfo);
			if (FAILED(hr)) break;

			audioEndpointInfo.sessions = GetMMDeviceSessionInfo(pEndpoint);

			SAFE_RELEASE(pEndpoint);

			ret.push_back(audioEndpointInfo);
		}

	} while (0);


	SAFE_RELEASE(pEnumerator);
	SAFE_RELEASE(pCollection);
	SAFE_RELEASE(pEndpoint);

	CoUninitialize();

	return ret;
}