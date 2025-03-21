#pragma once

// SKSEVR patching code from po3-Tweaks under MIT
// https://github.com/powerof3/po3-Tweaks/blob/850f80f298c0250565ff24ff3aba68a45ca8b73a/src/Fixes/CrosshairRefEventVR.cpp

namespace F4SEVRHooks
{
	// code converted from skse64
	static std::unordered_map<std::uint32_t, std::uint32_t> s_savedModIndexMap;

	struct TrampolineJmp : Xbyak::CodeGenerator
	{
		TrampolineJmp(std::uintptr_t func)
		{
			Xbyak::Label funcLabel;

			jmp(ptr[rip + funcLabel]);
			L(funcLabel);
			dq(func);
		}
	};

	void EraseMap()
	{
		s_savedModIndexMap.clear();
	}

	void SavePluginsList(F4SE::SerializationInterface* intfc)
	{
		DataHandler* dhand = DataHandler::GetSingleton();

		std::uint16_t regCount = 0;
		std::uint16_t lightCount = 0;
		std::uint16_t fileCount = 0;
		for (auto file : dhand->files) {
			if (file && file->compileIndex != 0xFF) {
				if (file->compileIndex != 0xFE) {
					regCount++;
				} else {
					lightCount++;
				}
			}
		}
		fileCount = regCount + lightCount;
		bool saveSuccessful = true;
		saveSuccessful &= intfc->OpenRecord('PLGN', 0);
		saveSuccessful &= intfc->WriteRecordData(&fileCount, sizeof(fileCount));

		logger::info("Saving plugin list ({} regular {} light {} total):", regCount, lightCount, fileCount);

		for (auto file : dhand->files) {
			if (file && file->compileIndex != 0xFF) {
				saveSuccessful &= intfc->WriteRecordData(&file->compileIndex, sizeof(file->compileIndex));
				if (file->compileIndex == 0xFE) {
					saveSuccessful &= intfc->WriteRecordData(&file->smallFileCompileIndex, sizeof(file->smallFileCompileIndex));
				}

				std::uint16_t nameLen = strlen(file->filename);
				saveSuccessful &= intfc->WriteRecordData(&nameLen, sizeof(nameLen));
				saveSuccessful &= intfc->WriteRecordData(file->filename, nameLen);
				if (file->compileIndex != 0xFE) {
					logger::info("\t[{}]\t{}", file->compileIndex, file->filename);
				} else {
					logger::info("\t[FE:{}]\t{}", file->smallFileCompileIndex, file->filename);
				}
			}
		}
		if (!saveSuccessful) {
			logger::error("F4SE cosave failed");
		}
	}

	//FUN_180028550 + 0x144
	//Probably not necessary but kept just in case
	void LoadLightModList(F4SE::SerializationInterface* intfc, std::uint32_t version = 1)
	{
		enum LightModVersion
		{
			kVersion1 = 1,
			kVersion2 = 2
		};

		logger::info("Loading light mod list:");
		// in skse64 version is a param, but this should coincide with VR
		DataHandler* dhand = DataHandler::GetSingleton();

		char name[0x104] = { 0 };
		std::uint16_t nameLen = 0;

		std::uint16_t numSavedMods = 0;
		if (version == kVersion1) {
			intfc->ReadRecordData(&numSavedMods, sizeof(std::uint8_t));
		} else if (version == kVersion2) {
			intfc->ReadRecordData(&numSavedMods, sizeof(std::uint16_t));
		}

		for (std::uint32_t i = 0; i < numSavedMods; i++) {
			intfc->ReadRecordData(&nameLen, sizeof(nameLen));
			intfc->ReadRecordData(&name, nameLen);
			name[nameLen] = 0;

			std::uint32_t lightIndex = 0xFE000 | i;

			const TESFile* file = dhand->LookupModByName(name);
			if (file) {
				//std::uint32_t newIndex = file->GetPartialIndex();
				std::uint32_t newIndex = !file->IsLight() ? file->compileIndex : (0xFE000 | file->smallFileCompileIndex);
				s_savedModIndexMap[lightIndex] = newIndex;
				logger::info("\t({} -> {})\t{}", lightIndex, newIndex, name);
			} else {
				s_savedModIndexMap[lightIndex] = 0xFF;
			}
			logger::info("\t({} -> {})\t{}", lightIndex, s_savedModIndexMap[lightIndex], name);
		}
	}

	//FUN_180028550 + 0x1cb LoadModList // 2871b
	void LoadModList(F4SE::SerializationInterface* intfc)
	{
		logger::info("Loading mod list:");

		DataHandler* dhand = DataHandler::GetSingleton();

		char name[0x104] = { 0 };
		std::uint16_t nameLen = 0;

		std::uint8_t numSavedMods = 0;
		intfc->ReadRecordData(&numSavedMods, sizeof(numSavedMods));
		for (std::uint32_t i = 0; i < numSavedMods; i++) {
			intfc->ReadRecordData(&nameLen, sizeof(nameLen));
			intfc->ReadRecordData(&name, nameLen);
			name[nameLen] = 0;

			const TESFile* modInfo = dhand->LookupModByName(name);
			if (modInfo) {
				//std::uint32_t newIndex = modInfo->GetPartialIndex();
				std::uint32_t newIndex = !modInfo->IsLight() ? modInfo->compileIndex : (0xFE000 | modInfo->smallFileCompileIndex);
				s_savedModIndexMap[i] = newIndex;
				logger::info("\t({} -> {})\t{}", i, newIndex, name);
			} else {
				s_savedModIndexMap[i] = 0xFF;
			}
		}
	}

	//This is new for skse64 and should be triggered on 'PLGN':
	//Attempt to inject at SKSEVR::FUN_180028550 + 0x30 before the old case statement.
	void LoadPluginList(F4SE::SerializationInterface* intfc)
	{
		DataHandler* dhand = DataHandler::GetSingleton();

		logger::info("Loading plugin list:");

		char name[0x104] = { 0 };
		std::uint16_t nameLen = 0;

		std::uint16_t modCount = 0;
		intfc->ReadRecordData(&modCount, sizeof(modCount));
		for (std::uint32_t i = 0; i < modCount; i++) {
			std::uint8_t modIndex = 0xFF;
			std::uint16_t lightModIndex = 0xFFFF;
			intfc->ReadRecordData(&modIndex, sizeof(modIndex));
			if (modIndex == 0xFE) {
				intfc->ReadRecordData(&lightModIndex, sizeof(lightModIndex));
			}

			intfc->ReadRecordData(&nameLen, sizeof(nameLen));
			intfc->ReadRecordData(&name, nameLen);
			name[nameLen] = 0;

			std::uint32_t newIndex = 0xFF;
			std::uint32_t oldIndex = modIndex == 0xFE ? (0xFE000 | lightModIndex) : modIndex;

			const TESFile* modInfo = dhand->LookupModByName(name);
			if (modInfo) {
				//newIndex = modInfo->GetPartialIndex();
				std::uint32_t newIndex = !modInfo->IsLight() ? modInfo->compileIndex : (0xFE000 | modInfo->smallFileCompileIndex);
			}

			s_savedModIndexMap[oldIndex] = newIndex;

			logger::info("\t({} -> {})\t{}", oldIndex, newIndex, name);
		}
	}

	std::uint32_t ResolveModIndex(std::uint32_t modIndex)
	{
		auto it = s_savedModIndexMap.find(modIndex);
		if (it != s_savedModIndexMap.end()) {
			return it->second;
		}

		return 0xFF;
	}

	struct ResolveFormIdHook
	{
		// Converted from SKSESE
		static bool ResolveFormId(std::uint32_t formId, std::uint32_t* formIdOut)
		{
			std::uint32_t modID = formId >> 24;
			if (modID == 0xFF) {
				*formIdOut = formId;
				return true;
			}

			if (modID == 0xFE) {
				modID = formId >> 12;
			}

			std::uint32_t loadedModID = ResolveModIndex(modID);
			if (loadedModID < 0xFF) {
				*formIdOut = (formId & 0x00FFFFFF) | (((std::uint32_t)loadedModID) << 24);
				return true;
			} else if (loadedModID > 0xFF) {
				*formIdOut = (loadedModID << 12) | (formId & 0x00000FFF);
				return true;
			}
			return false;
		}

		static void Install(std::uintptr_t a_base)
		{
			std::uintptr_t target = a_base + 0x61e40;
			auto jmp = TrampolineJmp((std::uintptr_t)ResolveFormId);
			REL::safe_write(target, jmp.getCode(), jmp.getSize());
		}
	};

	struct ResolveHandleHook
	{
		// Converted from SKSESE
		static bool ResolveHandle(std::uint64_t handle, std::uint64_t* handleOut)
		{
			std::uint32_t modID = (handle & 0xFF000000) >> 24;
			if (modID == 0xFF) {
				*handleOut = handle;
				return true;
			}

			if (modID == 0xFE) {
				modID = (handle >> 12) & 0xFFFFF;
			}

			std::uint64_t loadedModID = (std::uint64_t)ResolveModIndex(modID);
			if (loadedModID < 0xFF) {
				*handleOut = (handle & 0xFFFFFFFF00FFFFFF) | (((std::uint64_t)loadedModID) << 24);
				return true;
			} else if (loadedModID > 0xFF) {
				*handleOut = (handle & 0xFFFFFFFF00000FFF) | (loadedModID << 12);
				return true;
			}
			return false;
		}

		static void Install(std::uintptr_t a_base)
		{
			std::uintptr_t target = a_base + 0x61ee0;
			auto jmp = TrampolineJmp((std::uintptr_t)ResolveHandle);
			REL::safe_write(target, jmp.getCode(), jmp.getSize());
		}
	};

	struct Core_LoadCallback_Switch : Xbyak::CodeGenerator
	{
		Core_LoadCallback_Switch(std::uintptr_t beginSwitch, std::uintptr_t endLoop, std::uintptr_t func = stl::unrestricted_cast<std::uintptr_t>(LoadPluginList))
		{
			Xbyak::Label funcLabel;
			mov(edx, dword[rsp + 0x30]);
			cmp(edx, 'PLGN');  // 'PLGN'
			jnz("KeepChecking");
			mov(rcx, rbx);  // LoadPluginList(intfc);
			sub(rsp, 0x20);
			call(ptr[rip + funcLabel]);
			add(rsp, 0x20);
			mov(rcx, endLoop);  // break out
			jmp(rcx);
			L("KeepChecking");
			cmp(edx, 'LMOD');
			mov(rcx, beginSwitch);  // keep switching
			jmp(rcx);
			L(funcLabel);
			dq(func);
		}
		// Install our hook at the specified address

		static inline void Install(std::uintptr_t a_base, F4SE::Trampoline* a_trampoline)
		{
			std::uintptr_t target{ a_base + 0x18d80 };
			std::uintptr_t beginSwitch{ a_base + 0x18d8a };
			std::uintptr_t endSwitch{ a_base + 0x18eca };

			auto newCompareCheck = Core_LoadCallback_Switch(beginSwitch, endSwitch);
			newCompareCheck.ready();
			int fillRange = beginSwitch - target;
			REL::safe_fill(target, REL::NOP, fillRange);
			auto result = a_trampoline->allocate(newCompareCheck);
			logger::info("Core_LoadCallback_switch hookin {:x} to jmp to {:x} with base {:x}", target, (std::uintptr_t)result, a_base);
			a_trampoline->write_branch<5>(target, result);

			logger::info("Core_LoadCallback_Switch hooked at address F4SEVR::{:x}", target);
			logger::info("Core_LoadCallback_Switch hooked at offset F4SEVR::{:x}", target);
		}
	};

	struct Core_RevertCallbackHook : Xbyak::CodeGenerator
	{
		Core_RevertCallbackHook(std::uintptr_t jmpBack, std::uintptr_t func = stl::unrestricted_cast<std::uintptr_t>(EraseMap))
		{
			Xbyak::Label funcLabel;
			// SKyrim Patch
			//mov(ptr[rsp + 0x8], rbx);
			//push(rdi);
			//sub(rsp, 0x20);
			//sub(rsp, 0x20);
			//call(ptr[rip + funcLabel]);
			//add(rsp, 0x20);
			//mov(rcx, jmpBack);  // break out
			//jmp(rcx);
			//L(funcLabel);
			//dq(func);

			// F4SEVR Patch
			push(rbx);
			push(rbp);
			push(rsi);
			push(rdi);
			push(r14);
			sub(rsp, 0x50);
			sub(rsp, 0x20);
			call(ptr[rip + funcLabel]);
			add(rsp, 0x20);
			mov(rcx, jmpBack);  // break out
			jmp(rcx);
			L(funcLabel);
			dq(func);
		}

		// Install our hook at the specified address
		static inline void Install(std::uintptr_t a_base, F4SE::Trampoline* a_trampoline)
		{
			std::uintptr_t target{ a_base + 0x18890 };
			std::uintptr_t jmpBack{ a_base + 0x1889b };

			auto newCompareCheck = Core_RevertCallbackHook(jmpBack);
			newCompareCheck.ready();
			int fillRange = jmpBack - target;
			REL::safe_fill(target, REL::NOP, fillRange);
			auto result = a_trampoline->allocate(newCompareCheck);
			logger::info("Core_RevertCallbackHook hookin {:x} to jmp to {:x} with base {:x}", target, (std::uintptr_t)result, a_base);
			a_trampoline->write_branch<5>(target, result);

			logger::info("Core_RevertCallbackHook hooked at address F4SEVR::{:x}", target);
			logger::info("Core_RevertCallbackHook hooked at offset F4SEVR::{:x}", target);
		}
	};

	struct F4SEVRPatches
	{
		std::string name;
		std::uintptr_t offset;
		void* function;
	};

	std::vector<F4SEVRPatches> patches{
		{ "SaveModList", 0x18680, SavePluginsList },
		{ "LoadModList", 0x184e0, LoadModList },
		{ "LoadLightModList", 0x18750, LoadLightModList },
	};
	void Install(std::uint32_t a_f4se_version)
	{
		auto f4sevr_base = reinterpret_cast<uintptr_t>(GetModuleHandleA("f4sevr_1_2_72"));
		if (a_f4se_version == 393536) {  //0.6.21  // TODO fix up number
			logger::info("Found patchable f4sevr_1_2_72.dll version {} with base {:x}", a_f4se_version, f4sevr_base);
		} else {
			logger::info("Found unknown f4sevr_1_2_72.dll version {} with base {:x}; not patching", a_f4se_version, f4sevr_base);
			return;
		}

		for (const auto& patch : patches) {
			logger::info("Trying to patch {} at {:x} with {:x}"sv, patch.name, f4sevr_base + patch.offset, (std::uintptr_t)patch.function);
			std::uintptr_t target = (uintptr_t)(f4sevr_base + patch.offset);
			auto jmp = TrampolineJmp((std::uintptr_t)patch.function);
			REL::safe_write(target, jmp.getCode(), jmp.getSize());

			logger::info("F4SEVR {} patched"sv, patch.name);
		}

		// Allocate space near the module's address for all of our assembly hooks to go into
		// Each hook has to be within 2 GB of the trampoline space for the REL 32-bit jmps to work
		// The trampoline logic checks for first available region to allocate from 2 GB below addr to 2 GB above addr
		// So we add a gigabyte to ensure the entire DLL is within 2 GB of the allocated region
		constexpr std::size_t gigabyte = static_cast<std::size_t>(1) << 30;
		auto F4SEVRTrampoline = new F4SE::Trampoline();
		F4SEVRTrampoline->create(0x100, (void*)(f4sevr_base + gigabyte));

		Core_LoadCallback_Switch::Install(f4sevr_base, F4SEVRTrampoline);
		Core_RevertCallbackHook::Install(f4sevr_base, F4SEVRTrampoline);
		ResolveFormIdHook::Install(f4sevr_base);
		ResolveHandleHook::Install(f4sevr_base);
	}
}
