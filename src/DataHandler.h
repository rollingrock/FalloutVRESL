#pragma once

#include "RE/Bethesda/BSPointerHandle.h"
#include "RE/Bethesda/BSTArray.h"
#include "RE/Bethesda/BSTList.h"
#include "RE/Bethesda/BSTSingleton.h"
#include "RE/NetImmerse/NiTArray.h"
#include "RE/NetImmerse/NiTList.h"
#include "RE/Bethesda/TESForms.h"
#include "FalloutVRESLAPI.h"
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

class DataHandler : public
#ifndef BACKWARDS_COMPATIBLE
		BSTEventSource<BGSHotloadCompletedEvent>,  // 0000
		BSTSingletonSDM<DataHandler>            // 0058
#else
		TESDataHandler
#endif
{
public:
	static DataHandler* GetSingleton();
	static void InstallHooks();

#ifndef BACKWARDS_COMPATIBLE
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
#else
	std::uint32_t loadedModCount; /* 0FC0 */
	std::uint32_t pad14;              /* 0FC4 */
	TESFile* loadedMods[0xFF];        /* 0FC8 */
	BSTArray<std::uint32_t> releasedFormIDArray;                              /* 0FF0 */
	bool masterSave;                                                          /* 1008 */
	bool blockSave;                                                           /* 1009 */
	bool saveLoadGame;                                                        /* 100A */
	bool autoSaving;                                                          /* 100B */
	bool exportingPlugin;                                                     /* 100C */
	bool clearingData;                                                        /* 100D */
	bool hasDesiredFiles;                                                     /* 100E */
	bool checkingModels;                                                      /* 100F */
	bool loadingFiles;                                                        /* 1010 */
	bool dontRemoveIDs;                                                       /* 1011 */
	char gameSettingsLoadState;                                               /* 1012*/
	TESRegionDataManager* regionDataManager;          // 1018, VR 17E8
	TESOverlayFileCollection compiledFileCollection;                          // 17f0
};
static_assert(sizeof(DataHandler) == 0x1838);
static_assert(offsetof(DataHandler, compiledFileCollection) == 0x17f0);
#endif


namespace FalloutVRESLPluginAPI
{
	// Handles skse mod messages requesting to fetch API functions from FalloutVRESL
	void ModMessageHandler(F4SE::MessagingInterface::Message* message);

	// This object provides access to FalloutVRESL's mod support API version 1
	struct FalloutVRESLInterface001 : IFalloutVRESLInterface001
	{
		virtual unsigned int GetBuildNumber();

		/// @brief Get the NG compatible TESFileCollection for FalloutVR.
		/// @return Pointer to CompiledFileCollection.
		const RE::TESFileCollection* GetCompiledFileCollection();
	};

}  // namespace FalloutVRESLPluginAPI
extern FalloutVRESLPluginAPI::FalloutVRESLInterface001 g_interface001;

// Function that tests GetCompiledFileCollectionExtern() and prints to log.
// This is also an example of how to use GetHandle and GetProc
void TestGetCompiledFileCollectionExtern();
