#pragma once

#include "RE/Bethesda/BSPointerHandle.h"
#include "RE/Bethesda/BSTArray.h"
#include "RE/Bethesda/BSTList.h"
#include "RE/Bethesda/BSTSingleton.h"
#include "RE/NetImmerse/NiTArray.h"
#include "RE/NetImmerse/NiTList.h"
#include "RE/Bethesda/TESForms.h"
#include <detours/detours.h>

using namespace RE;

class SaveLoadGame
{
public:
	static SaveLoadGame* GetSingleton()
	{
		//return reinterpret_cast<SaveLoadGame*>(RE::BGSSaveLoadManager::GetSingleton());
		REL::Relocation<SaveLoadGame**> singleton{ REL::Offset(0x5ad5a80) };
		return *singleton;
	}

	static void InstallHooks();
	// members
	// ~~~~~~~~~~~~~~~~~ below member differs from VR~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	BSTArray<TESFile*> regularPluginList;  // 000
	BSTArray<TESFile*> smallPluginList;    // 018

	std::uint64_t fakeVRPadding[0x9D];
	// ~~~~~~~~~~~~~~~~~ end VR difference ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//BGSSaveLoadFormIDMap worldspaceFormIDMap;                           // 200
	//BSTHashMap<FormID, ActorHandle> unk98;                              // 268
	//BGSSaveLoadReferencesMap unkC8;                                     // 298
	//BSTHashMap<FormID, FormID> unk158;                                  // 328
	//BGSConstructFormsInAllFilesMap reconstructFormsMap;                 // 358
	//BGSSaveLoadQueuedSubBufferMap queuedSubBuffersMap;                  // 3D8
	//BGSSaveLoadFormIDMap formIDMap;                                     // 468
	//BSTArray<void*> saveLoadHistory;                                    // 4D0
	//BSTArray<void*> unk318;                                             // 4E8
	//BGSSaveLoadChangesMap* saveLoadChanges;                             // 500
	//std::uint64_t unk338;                                               // 508
	//stl::enumeration<RE::BGSSaveLoadGame::Flags, std::uint32_t> flags;  // 510
	//std::uint8_t currentMinorVersion;                                   // 514
};
//static_assert(sizeof(SaveLoadGame) == 0x518);
