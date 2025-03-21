#pragma once
#include "DataHandler.h"
#include <detours/detours.h>

namespace RE
{
	using FormID = std::uint32_t;
	using RefHandle = std::uint32_t;
	using VMHandle = std::uint64_t;
	using VMStackID = std::uint32_t;
	using VMTypeID = std::uint32_t;
}

namespace tesfilehooks
{
	static inline RE::BSTArray<RE::TESFile*> filesArray = RE::BSTArray<RE::TESFile*>();
	static inline RE::BSTArray<RE::TESFile*> filesArrayOverlay = RE::BSTArray<RE::TESFile*>();

	// SE loops through handler->files and then handler->activeFile last
	// We use an array to mimic this for VR
	static void PopulateFilesArray(bool includeOverlay)
	{
		auto& fileArray = includeOverlay ? filesArrayOverlay : filesArray;
		auto handler = DataHandler::GetSingleton();
		if (fileArray.empty() ||
			fileArray.size() != handler->compiledFileCollection.files.size() + handler->compiledFileCollection.smallFiles.size()) {
			// Ensure filesArray is synchronized with handler
			fileArray.clear();
			for (auto file : handler->files) {
				if (file->compileIndex != 0xFF && file != handler->activeFile) {
					fileArray.push_back(file);
				} else if (isOverlay(file) && includeOverlay) {
					fileArray.push_back(file);
				}
			}

			if (handler->activeFile) {
				fileArray.push_back(handler->activeFile);
			}
		}
	}

	static std::uint32_t GetLoadedModCountSE(DataHandler* a_handler)
	{
		logger::info("GetLoadedModCountSE called!");
		PopulateFilesArray(false);
		return filesArray.size();
	}

	static RE::TESFile* GetModAtIndex(DataHandler* a_handler, std::uint32_t a_index)
	{
		logger::info("GetModAtIndex called for {:x}!", a_index);
		return filesArray[a_index];
	}

	struct TESQuestHook
	{
		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::Offset(0x05c88a0) };  // skyrim 0x389ca0
			//F4SE::AllocTrampoline(14);
			F4SE::GetTrampoline().write_call<5>(target.address() + 0x1E5, GetLoadedModCountSE);  // 0x181
			//F4SE::AllocTrampoline(14);
			F4SE::GetTrampoline().write_call<5>(target.address() + 0x209, GetModAtIndex);   // 0x199

			logger::info("Installed TESQuest LoadedModCheck hook");
		}
	};

	// TODO: FALLOUT only has one like the TESQuest hook.  So don't think this one is needed but will need to check other references to GetLoadedModCount
	struct UnkTESTopicHook
	{
		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::Offset(0x389ED0) };
			//F4SE::AllocTrampoline(14);
			F4SE::GetTrampoline().write_call<5>(target.address() + 0x147, GetLoadedModCountSE);
			//F4SE::AllocTrampoline(14);
			F4SE::GetTrampoline().write_call<5>(target.address() + 0x169, GetModAtIndex);

			logger::info("Installed UnkTESTopicHook hook");
		}
	};

	struct UnkTerrainHook
	{
		static std::uint32_t thunk()
		{
			// SE only uses the regular files size. This function is unknown, but we will do the same here to be safe
			return DataHandler::GetSingleton()->compiledFileCollection.files.size();
		}

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::Offset(0x079ef50 + 0xBA) };  // 0x4b80a0 + 0x159
			//F4SE::AllocTrampoline(14);
			F4SE::GetTrampoline().write_call<5>(target.address(), UnkTerrainHook::thunk);
			REL::Relocation<std::uintptr_t> target2{ REL::Offset(0x4B80A0 + 0x76) };  // 0xa3
			//F4SE::AllocTrampoline(14);
			F4SE::GetTrampoline().write_call<5>(target2.address(), UnkTerrainHook::thunk);
		}
	};

	struct UnkCOCHook
	{
		static inline REL::Relocation<std::uintptr_t> target{ REL::Offset(0x011acc0) };  // Skyrim 0x17c000

		static inline std::uint32_t fileIndexCount = 0;

		struct TrampolineCOCCall : Xbyak::CodeGenerator
		{
			// Call OpenFileLoop method, and check if file successfully opened
			TrampolineCOCCall(std::uintptr_t jmpOnSuccess, std::uintptr_t jmpOnFail, std::uintptr_t func)
			{
				Xbyak::Label funcLabel;
				Xbyak::Label succLabel;
				Xbyak::Label failLabel;
				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);
				mov(rbx, rax);
				test(rbx, rbx);
				jz(failLabel);
				L(succLabel);
				mov(rcx, jmpOnSuccess);
				jmp(rcx);
				L(failLabel);
				mov(rcx, jmpOnFail);
				jmp(rcx);
				L(funcLabel);
				dq(func);
			}
		};

		struct TrampolineFailCOCCall : Xbyak::CodeGenerator
		{
			// Loop to OpenFileLoop if more files left to open
			TrampolineFailCOCCall(std::uintptr_t jmpOnSuccess, std::uintptr_t jmpOnFail, std::uintptr_t func)
			{
				Xbyak::Label funcLabel;
				Xbyak::Label succLabel;
				Xbyak::Label failLabel;
				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);
				test(rax, rax);
				jnz(failLabel);
				L(succLabel);
				mov(rcx, jmpOnSuccess);
				jmp(rcx);
				L(failLabel);
				mov(rcx, jmpOnFail);
				jmp(rcx);
				L(funcLabel);
				dq(func);
			}
		};

		static bool IndexCheck()
		{
			fileIndexCount++;
			if (fileIndexCount >= filesArrayOverlay.size()) {
				fileIndexCount = 0;
				return true;
			}
			return false;
		}

		static RE::TESFile* OpenFileLoop()
		{
			// VR loops through and opens loadedMods until one returns true
			// SE loops through `files` then `activeFile` and opens until one returns true
			logger::debug("OpenFileLoop called with index {}", fileIndexCount);
			PopulateFilesArray(true);
			if (filesArrayOverlay[fileIndexCount]->OpenTES(RE::NiFile::OpenMode::kReadOnly, false)) {
				return filesArrayOverlay[fileIndexCount];
			}
			return nullptr;
		}

		static void SkipLoadedModsOptimizedLoopCheck()
		{
			// The loop only does logic on `loadedMods` if a single file is "optimized"
			// It doesn't seem like any plugins in vanilla or modded would have this flag, so we will
			// NOP all the logic

			// TODO: We should properly handle "optmized" TESFile cases
			std::uintptr_t start = target.address() + 0xE4;  // 0x93
			std::uintptr_t end = target.address() + 0x127;   // 0x16d
			REL::safe_fill(start, REL::NOP, end - start);
		}

		static void InstallFileOpenLoop()
		{
			std::uintptr_t start = target.address() + 0x127;  // 0x173
			std::uintptr_t end = target.address() + 0x212;    // 0x1a8
			std::uintptr_t failJmp = target.address() + 0x420; // 0x439
			REL::safe_fill(start, REL::NOP, end - start);

			auto trampolineJmp = TrampolineCOCCall(end, failJmp, stl::unrestricted_cast<std::uintptr_t>(OpenFileLoop));
			auto& trampoline = F4SE::GetTrampoline();
			//F4SE::AllocTrampoline(trampolineJmp.getSize());
			auto result = trampoline.allocate(trampolineJmp);
			//F4SE::AllocTrampoline(14);
			trampoline.write_branch<5>(start, (std::uintptr_t)result);
		}

		static void InstallFailOpenFileLoop()
		{
			std::uintptr_t start = target.address() + 0x420;   // 0x439
			std::uintptr_t end = target.address() + 0x43D;    // 0x44c
			std::uintptr_t failJmp = target.address() + 0x43D;  // 0x44c
			std::uintptr_t succJmp = target.address() + 0x127;  // 0x173
			REL::safe_fill(start, REL::NOP, end - start);

			auto trampolineJmp = TrampolineFailCOCCall(succJmp, failJmp, stl::unrestricted_cast<std::uintptr_t>(IndexCheck));
			auto& trampoline = F4SE::GetTrampoline();
			//F4SE::AllocTrampoline(trampolineJmp.getSize());
			auto result = trampoline.allocate(trampolineJmp);
			//F4SE::AllocTrampoline(14);
			trampoline.write_branch<5>(start, (std::uintptr_t)result);
		}

		static void Install()
		{
			SkipLoadedModsOptimizedLoopCheck();   // logic is different in fallout so skipping as not sure if even needed
			InstallFileOpenLoop();
			InstallFailOpenFileLoop();
		}
	};

	struct UnkCOCFileResetHook
	{
		static inline REL::Relocation<std::uintptr_t> target{  REL::Offset(0x011ede0) };  // 0x17f350
		static inline REL::Relocation<std::uintptr_t> target2{ REL::Offset(0x011f776) };  

		struct TrampolineCall : Xbyak::CodeGenerator
		{
			// Call OpenFileLoop method, and check if file successfully opened
			TrampolineCall(std::uintptr_t jmpaddr, std::uintptr_t func)
			{
				Xbyak::Label funcLabel;
				Xbyak::Label resumeLabel;
				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);
				jmp(resumeLabel);
				L(funcLabel);
				dq(func);
				L(resumeLabel);
			}
		};

		static void UnkTESFile(RE::TESFile* a_file)
		{
			using func_t = decltype(&UnkTESFile);
			REL::Relocation<func_t> func{ REL::Offset(0x013a060) };  // 0x18fba0
			return func(a_file);
		}

		// VR loops over loadedmods and calls some resetting function, then clears loadedmods
		// SE loops over regular/small files, calls resetting function then clears the arrays
		static void ClearFilesLoop()
		{
			logger::debug("ClearFilesLoop called!");
			auto handler = DataHandler::GetSingleton();
			for (auto file : handler->compiledFileCollection.files) {
				if (file->lastError.underlying() != 0) {
					UnkTESFile(file);
				}
			}
			handler->compiledFileCollection.files.clear();
			for (auto file : handler->compiledFileCollection.smallFiles) {
				if (file->lastError.underlying() != 0) {
					UnkTESFile(file);
				}
			}
			handler->compiledFileCollection.smallFiles.clear();

#ifdef BACKWARDS_COMPATIBLE
		//	handler->loadedModCount = 0;    // FIIIIIIXXXXXX
#endif
		}

		static void Install()
		{
			std::uintptr_t start = target.address() + 0x94E;  // 0x70B
			std::uintptr_t end = target.address() + 0x98E;    // 0x757
			REL::safe_fill(start, REL::NOP, end - start);
			REL::safe_fill(target2.address(), REL::NOP, 7);   // need to nop the next instruction since in skyrim this was before the end
			auto trampolineJmp = TrampolineCall(end, stl::unrestricted_cast<std::uintptr_t>(ClearFilesLoop));
			REL::safe_write(start, trampolineJmp.getCode(), trampolineJmp.getSize());
		}
	};

	struct UnkHook
	{
		// Using this hook to patch uses of GetModAtIndex
		static inline REL::Relocation<std::uintptr_t> target{ REL::Offset(0x017f700) };  // 0x1B9E60
		static inline REL::Relocation<std::uintptr_t> target2{ REL::Offset(0x017f4d0) };  // 0x1b9c50
		static inline REL::Relocation<std::uintptr_t> target3{ REL::Offset(0x05fdee0) };  
		static inline REL::Relocation<std::uintptr_t> target4{ REL::Offset(0x0c09e80) };  

		static std::uint64_t thunk(RE::FormID a_formID)
		{
			auto highestByte = a_formID >> 0x18;
			logger::info("Calling Unk with {:x}", highestByte);
			return func(highestByte);
		}

		static inline REL::Relocation<decltype(thunk)> func;

		static RE::TESFile* GetFileFromFormID(DataHandler* a_handler, RE::FormID a_formID)
		{
			logger::debug("GetFileFromFormID called on {:x}", a_formID);
			auto espIndex = a_formID >> 0x18;
			if (espIndex == 0xFE) {
				auto eslIndex = (a_formID >> 0x12) & 0xFFF;
				if (eslIndex < a_handler->compiledFileCollection.smallFiles.size()) {
					return a_handler->compiledFileCollection.smallFiles[eslIndex];
				}
			} else if (espIndex < a_handler->compiledFileCollection.files.size()) {
				return a_handler->compiledFileCollection.files[espIndex];
			}
			return nullptr;
		}

		static void EraseBitShift()
		{
			std::uintptr_t start = target.address() + 0x52;
			std::uintptr_t end = target.address() + 0x55;
			REL::safe_fill(start, REL::NOP, end - start);

			start = target2.address() + 0x47;
			end = target2.address() + 0x4A;
			REL::safe_fill(start, REL::NOP, end - start);

			start = target3.address() + 0xDD;
			end = target3.address() + 0xE0;
			REL::safe_fill(start, REL::NOP, end - start);

			start = target4.address() + 0xB4;
			end = target4.address() + 0xB7;
			REL::safe_fill(start, REL::NOP, end - start);
		}

		static void InstallThunkUnk()
		{
			pstl::write_thunk_call<UnkHook>(target.address() + 0x5B);
			pstl::write_thunk_call<UnkHook>(target2.address() + 0x50);
		}

		static void InstallGetFileFromFormID()
		{
			F4SE::GetTrampoline().write_call<5>(target.address() + 0x71, GetFileFromFormID);
			F4SE::GetTrampoline().write_call<5>(target2.address() + 0x66, GetFileFromFormID);
			F4SE::GetTrampoline().write_call<5>(target3.address() + 0xE0, GetFileFromFormID);
			F4SE::GetTrampoline().write_call<5>(target4.address() + 0xB7, GetFileFromFormID);
		}

		static void Install()
		{
			EraseBitShift();
			InstallThunkUnk();
			InstallGetFileFromFormID();
		}
	};

	struct DuplicateHook
	{
		static inline REL::Relocation<std::uintptr_t> target{ REL::Offset(0x18F060) };  // 0x18f060

		struct TrampolineCall : Xbyak::CodeGenerator
		{
			TrampolineCall(std::uintptr_t jmpAfterCall, std::uintptr_t func)
			{
				Xbyak::Label funcLabel;
				mov(rcx, rsi);
				mov(rdx, rdi);
				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);
				mov(rcx, jmpAfterCall);
				jmp(rcx);
				L(funcLabel);
				dq(func);
			}
		};

		// Copy over smallCompileIndex right around where SE copies it
		// The smallCompileIndex is padding in VR normally, so we can directly set it and reference it
		static void SetFlagsAndIndex(RE::TESFile* a_orig, RE::TESFile* a_duplicate)
		{
			// Copy of original VR logic we overwrote in the trampoline setup
			if (a_orig->flags.all(RE::TESFile::RecordFlag::kMaster)) {
				a_duplicate->flags.set(RE::TESFile::RecordFlag::kMaster);
			} else {
				a_duplicate->flags.reset(RE::TESFile::RecordFlag::kMaster);
			}
//  FIX THIS BELOW
			//a_duplicate->flags &= 0xFFFFFFu;
			//a_duplicate->flags |= a_orig->compileIndex << 24;
			a_duplicate->compileIndex = a_orig->compileIndex;
			// End of VR logic copy
			a_duplicate->smallFileCompileIndex = a_orig->smallFileCompileIndex;

			logger::debug("Successfully copied duplicate file {}, ESL flagged: {}", a_duplicate->filename, a_duplicate->IsLight());
		}

		// Install our hook at the specified address
		static inline void Install()
		{
			std::uintptr_t start = target.address() + 0x117;   // 0xfc
			std::uintptr_t end = target.address() + 0x16f;     // 0x137
			REL::safe_fill(start, REL::NOP, end - start);
			auto trampolineJmp = TrampolineCall(end, stl::unrestricted_cast<std::uintptr_t>(SetFlagsAndIndex));
			REL::safe_write(start, trampolineJmp.getCode(), trampolineJmp.getSize());

			if (trampolineJmp.getSize() > (end - start)) {
				logger::critical("DuplicateHook trampoline hook {} bytes too big!", trampolineJmp.getSize() - (end - start));
			}
		}
	};

	struct LoadedModCountHook
	{
		static std::uint32_t thunk(DataHandler* a_handler)
		{
			// This function should never be invoked since we hooked all usages, bad behavior occurred!
			// Crash here for crashlog to show missing usage
			throw std::invalid_argument("Unexpected invocation of LoadedModCountHook!");
		}

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::Offset(0x011e8d0) };  // 0x17EFE0
			auto& trampoline = F4SE::GetTrampoline();
			//F4SE::AllocTrampoline(14);
			trampoline.write_branch<5>(target.address(), thunk);
		}
	};

	struct GetModAtIndexHook
	{
		static RE::TESFile* thunk(DataHandler* a_handler, std::uint32_t a_index)
		{
			// This hook should never be invoked, this means a usage of this function was not patched!
			// Throw to prevent bad behavior and force a patch
			throw std::invalid_argument("Unexpected use of DataHandler::GetModAtIndex!");
		}

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::Offset(0x0122a60) };  // 0x182d40
			auto& trampoline = F4SE::GetTrampoline();
			//F4SE::AllocTrampoline(14);
			trampoline.write_branch<5>(target.address(), thunk);
		}
	};

	struct IsGameModdedHook
	{
		// Replica of Engine fixes. Return game isn't modded to enable achievements
		// For us, this removes one use of `loadedMods` that would cause problems otherwise
		static bool thunk(DataHandler* a_handler, std::uint32_t a_index)
		{
			return false;
		}

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::Offset(0x011f9b0) };  // 0x17FB90
			auto& trampoline = F4SE::GetTrampoline();
			//F4SE::AllocTrampoline(14);
			trampoline.write_branch<5>(target.address(), thunk);
		}
	};

	static inline void InstallHooks()
	{
		DuplicateHook::Install();
		LoadedModCountHook::Install();
		GetModAtIndexHook::Install();
		IsGameModdedHook::Install();
		TESQuestHook::Install();
	//	UnkTESTopicHook::Install();     // TODO: see if this one is needed in fallout.   
		UnkTerrainHook::Install();
		UnkCOCHook::Install();
		UnkCOCFileResetHook::Install();
		UnkHook::Install();
	}
}
