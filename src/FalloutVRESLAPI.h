#pragma once
#include <RE/Fallout.h>
#include <F4SE/F4SE.h>
// Interface code based on https://github.com/adamhynek/higgs

namespace FalloutVRESLPluginAPI
{
	constexpr const auto FalloutVRESLPluginName = "FalloutVRESL";
	// A message used to fetch FalloutVRESL's interface
	struct FalloutVRESLMessage
	{
		enum : uint32_t
		{
			kMessage_GetInterface = 0xeacb2bef
		};  // Randomly generated
		void* (*GetApiFunction)(unsigned int revisionNumber) = nullptr;
	};

	// Returns an IFalloutVRESLInterface001 object compatible with the API shown below
	// This should only be called after F4SE sends kMessage_PostLoad to your plugin
	struct IFalloutVRESLInterface001;
	IFalloutVRESLInterface001* GetFalloutVRESLInterface001();

	// This object provides access to FalloutVRESL's mod support API
	struct IFalloutVRESLInterface001
	{
		// Gets the FalloutVRESL build number
		virtual unsigned int GetBuildNumber() = 0;

		/// @brief Get the SSE compatible TESFileCollection for FalloutVR.
		/// This should be called after kDataLoaded to ensure it's been populated.
		/// @return Pointer to TESFileCollection CompiledFileCollection.
		const virtual RE::TESFileCollection* GetCompiledFileCollection() = 0;
	};

}  // namespace FalloutVRESLPluginAPI
extern FalloutVRESLPluginAPI::IFalloutVRESLInterface001* g_FalloutVRESLInterface;
