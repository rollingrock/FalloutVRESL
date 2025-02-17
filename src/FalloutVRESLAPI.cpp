#include "FalloutVRESLAPI.h"
// Interface code based on https://github.com/adamhynek/higgs

// Stores the API after it has already been fetched
FalloutVRESLPluginAPI::IFalloutVRESLInterface001* g_FalloutVRESLInterface = nullptr;

// Fetches the interface to use from FalloutVRESL
FalloutVRESLPluginAPI::IFalloutVRESLInterface001* FalloutVRESLPluginAPI::GetFalloutVRESLInterface001()
{
	// If the interface has already been fetched, rturn the same object
	if (g_FalloutVRESLInterface) {
		return g_FalloutVRESLInterface;
	}

	// Dispatch a message to get the plugin interface from FalloutVRESL
	FalloutVRESLMessage message;
	const auto f4seMessaging = F4SE::GetMessagingInterface();
	f4seMessaging->Dispatch(FalloutVRESLMessage::kMessage_GetInterface, (void*)&message,
		sizeof(FalloutVRESLMessage*), FalloutVRESLPluginName);
	if (!message.GetApiFunction) {
		return nullptr;
	}

	// Fetch the API for this version of the FalloutVRESL interface
	g_FalloutVRESLInterface = static_cast<IFalloutVRESLInterface001*>(message.GetApiFunction(1));
	return g_FalloutVRESLInterface;
}
