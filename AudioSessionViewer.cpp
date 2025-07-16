// AudioSessionViewer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "AudioEndpointInterface.h"

int main()
{
	// EDataFlow::eCapture microphone
	// EDataFlow::eRender speakers
	auto endpoints = GetAudioEndpoints(EDataFlow::eCapture);

	for (auto ep : endpoints) {
		std::wcout << "=======================" << std::endl;
		std::wcout << ep.friendlyName << std::endl;

		std::wcout << "-----  Used by: -----------" << std::endl;
		for (auto sess : ep.sessions) {
			if (sess.state != AudioSessionState::AudioSessionStateActive) continue;
			std::wcout << sess.processDisplayName << " (" << sess.processBaseName << ")" << std::endl;
			std::wcout << sess.processId << std::endl;
			std::wcout << (sess.state & AudioSessionState::AudioSessionStateActive) << std::endl;
		}
		std::wcout << "=======================" << std::endl;

	}
}