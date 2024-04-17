#pragma once

#include "RE/Bethesda/BSPointerHandle.h"
#include "RE/Bethesda/BSTArray.h"
#include "RE/Bethesda/BSTList.h"
#include "RE/Bethesda/BSTSingleton.h"
#include "RE/NetImmerse/NiTArray.h"
#include "RE/NetImmerse/NiTList.h"
#include "RE/Bethesda/TESForms.h"
//#include "SkyrimVRESLAPI.h"
#include <detours/detours.h>

using namespace RE;

struct TESOverlayFileCollection : RE::TESFileCollection
{
	BSTArray<TESFile*> overlayFiles;  // 20
};

static inline std::uint32_t overlayBit = 1 << 20;

static inline void setOverlay(RE::TESFile* a_file)
{
	auto recordFlagsAddr = reinterpret_cast<std::uintptr_t>(a_file) + offsetof(RE::TESFile, flags);
	auto recordFlags = reinterpret_cast<int*>(recordFlagsAddr);
	auto newFlags = a_file->flags.underlying() | overlayBit;
	*recordFlags = newFlags;
}

static inline bool isOverlay(RE::TESFile* a_file)
{
	int isOverlay = a_file->flags.underlying() & overlayBit;
	return isOverlay != 0;
}

class DataHandler : 
		public BSTEventSource<BGSHotloadCompletedEvent>,  // 0000
		public BSTSingletonSDM<DataHandler>            // 0058

{
public:
	static DataHandler* GetSingleton();
	static void InstallHooks();

	const RE::TESFile* LookupModByName(std::string_view a_modName);
	// members
	TESObjectList* objectList;                                                // 0060
	BSTArray<TESForm*> formArrays[stl::to_underlying(ENUM_FORM_ID::kTotal)];  // 0068
	TESRegionList* regionList;                                                // 0F50
	NiTPrimitiveArray<TESObjectCELL*> interiorCells;                          // 0F58
	NiTPrimitiveArray<BGSAddonNode*> addonNodes;                              // 0F70
	NiTList<TESForm*> badForms;                                               // 0F88
	std::uint32_t nextID;                                                     // 0FA0
	TESFile* activeFile;                                                      // 0FA8
	BSSimpleList<TESFile*> files;                                             // 0FB0
	std::uint32_t loadedModCount;											  // 0FC0
	std::uint32_t padfc4;													  // 0FC4
	TESFile* loadedMods[0xFF];												  // 0FC8
	BSTArray<std::uint32_t> releasedFormIDArray;                              // 17c0
	bool masterSave;                                                          // 17d8
	bool blockSave;                                                           // 17d9
	bool saveLoadGame;                                                        // 17da
	bool autoSaving;                                                          // 17db
	bool exportingPlugin;                                                     // 17dc
	bool clearingData;                                                        // 17dd
	bool hasDesiredFiles;                                                     // 17de
	bool checkingModels;                                                      // 17df
	bool loadingFiles;                                                        // 17e0
	bool dontRemoveIDs;                                                       // 17e1
	char gameSettingsLoadState;                                               // 17e2
	TESRegionDataManager* regionDataManager;                                  // 17e8
	TESOverlayFileCollection compiledFileCollection;                          // 17f0
};
static_assert(sizeof(DataHandler) == 0x1838);
static_assert(offsetof(DataHandler, compiledFileCollection) == 0x17f0);


//namespace SkyrimVRESLPluginAPI
//{
//	// Handles skse mod messages requesting to fetch API functions from SkyrimVRESL
//	void ModMessageHandler(SKSE::MessagingInterface::Message* message);
//
//	// This object provides access to SkyrimVRESL's mod support API version 1
//	struct SkyrimVRESLInterface001 : ISkyrimVRESLInterface001
//	{
//		virtual unsigned int GetBuildNumber();
//
//		/// @brief Get the SSE compatible TESFileCollection for SkyrimVR.
//		/// @return Pointer to CompiledFileCollection.
//		const RE::TESFileCollection* GetCompiledFileCollection();
//	};
//
//}  // namespace SkyrimVRESLPluginAPI
//extern SkyrimVRESLPluginAPI::SkyrimVRESLInterface001 g_interface001;

// Function that tests GetCompiledFileCollectionExtern() and prints to log.
// This is also an example of how to use GetHandle and GetProc
void TestGetCompiledFileCollectionExtern();
