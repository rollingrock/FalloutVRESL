#pragma once
#include "DataHandler.h"
#include <detours/detours.h>

namespace startuphooks
{
	static bool HasFile(RE::BSTArray<RE::TESFile*> a_array, RE::TESFile* a_file)
	{
		for (auto file : a_array) {
			if (file == a_file) {
				return true;
			}
		}
		return false;
	}

	// Reinterpret datahandler as our custom datahandler with ESL support
	// and add the file with the correct compile indexing
	// This is a replica of what Skyrim SE does when adding files
	static void AddFile(RE::TESFile* a_file)
	{
		logger::debug("AddFile invoked {} {:x}", std::string(a_file->filename), a_file->compileIndex);
		auto handler = DataHandler::GetSingleton();
		auto& fileCollection = handler->compiledFileCollection;
		if (!a_file->IsLight() && HasFile(fileCollection.files, a_file)) {
			return;
		} else if (a_file->IsLight() && HasFile(fileCollection.smallFiles, a_file)) {
			return;
		}

		if (!a_file->IsLight()) {
			logger::debug("Adding {} to loadedMods", a_file->filename);
			handler->loadedMods[handler->loadedModCount] = a_file;
			handler->loadedModCount++;
		}

		if (isOverlay(a_file)) {
			logger::debug("Adding {} as an overlay plugin", a_file->filename);
			fileCollection.overlayFiles.push_back(a_file);
			return;
		}

		if (a_file->IsLight()) {
			logger::debug("Adding {} as a light plugin", a_file->filename);
			fileCollection.smallFiles.push_back(a_file);
			auto smallFileCompileIndex = fileCollection.smallFiles.size() - 1;
			auto ul = reinterpret_cast<std::uint32_t*>(&a_file->flags);
			*ul &= 0xFFFFFFu;
			*ul |= 0xFE << 24;
			//a_file->flags &= 0xFFFFFFu;
			//a_file->flags |= 0xFE << 24;
			a_file->compileIndex = 0xFE;
			a_file->smallFileCompileIndex = smallFileCompileIndex;
			if (smallFileCompileIndex > 0xFFF) {
				auto msg = std::format("ESL plugin {} was added beyond the ESL range! Reduce the number of ESL plugins in your load order to fix this", a_file->filename);
				stl::report_and_fail(msg);
			}
		} else {
			logger::debug("Adding {} as a regular plugin", a_file->filename);
			fileCollection.files.push_back(a_file);
			auto fileCompileIndex = fileCollection.files.size() - 1;
			auto ul = reinterpret_cast<std::uint32_t*>(&a_file->flags);
			*ul &= 0xFFFFFFu;
			*ul |= fileCompileIndex << 24;
			//a_file->flags &= 0xFFFFFFu;
			//a_file->flags |= fileCompileIndex << 24;
			a_file->compileIndex = fileCompileIndex;
			a_file->smallFileCompileIndex = 0;
			if (fileCompileIndex >= 0xFE) {
				auto msg = std::format("Regular plugin {} was added beyond the ESP range. This will overflow with ESLs and break the game! Reduce the number of regular plugins in the load order to fix this", a_file->filename);
				stl::report_and_fail(msg);
			}
		}
		logger::debug("AddFile finished");
	}

	struct AddTESFileHook
	{
		static void AddFileThunk(DataHandler* a_handler, RE::TESFile* a_file)
		{
			return AddFile(a_file);
		}

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::Offset(0x011d590) };  // Skyrim was 0x17dff0

			auto& trampoline = F4SE::GetTrampoline();
			//F4SE::AllocTrampoline(14);
			trampoline.write_branch<5>(target.address(), AddFileThunk);
			logger::info("Install AddFile hook at {:x}", target.address());
			logger::info("Install AddFile hook at offset {:x}", target.offset());
		}
	};

	struct AddTESFileHook1
	{
		struct TrampolineCall : Xbyak::CodeGenerator
		{
			// Call AddFile method, then jump back to rest of code
			TrampolineCall(std::uintptr_t jmpAfterCall, std::uintptr_t func = stl::unrestricted_cast<std::uintptr_t>(AddFile))
			{
				Xbyak::Label funcLabel;
				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);
				mov(rcx, jmpAfterCall);
				jmp(rcx);
				L(funcLabel);
				dq(func);
			}
		};

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::Offset(0x0123490 + 0x9b5) };  // Skyrim was 0x1831f0 + 0x807
			REL::Relocation<std::uintptr_t> jmp{ REL::Offset(0x0123490 + 0x9F4) };  // 0x852
			REL::safe_fill(target.address(), REL::NOP, jmp.address() - target.address());

			auto trampolineJmp = TrampolineCall(jmp.address());
			REL::safe_write(target.address(), trampolineJmp.getCode(), trampolineJmp.getSize());

			logger::info("Install AddFile1 hook at address {:x}", target.address());
			logger::info("Install AddFile1 hook at offset {:x}", target.offset());
		}
	};

	struct AddTESFileHook2
	{
		struct TrampolineCall : Xbyak::CodeGenerator
		{
			// Call AddFile method, then jump back to rest of code
			TrampolineCall(std::uintptr_t jmpAfterCall, std::uintptr_t func = stl::unrestricted_cast<std::uintptr_t>(AddFile))
			{
				Xbyak::Label funcLabel;
				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);
				mov(rcx, jmpAfterCall);
				jmp(rcx);
				L(funcLabel);
				dq(func);
			}
		};

		static void Install()
		{
			REL::Relocation<std::uintptr_t> target{ REL::Offset(0x0123490 + 0x9b5) };
			REL::Relocation<std::uintptr_t> jmp{ REL::Offset(0x0123490 + 0x9F4) };
			REL::safe_fill(target.address(), REL::NOP, jmp.address() - target.address());

			auto trampolineJmp = TrampolineCall(jmp.address());
			REL::safe_write(target.address(), trampolineJmp.getCode(), trampolineJmp.getSize());

			logger::info("Install AddFile1 hook at address {:x}", target.address());
			logger::info("Install AddFile1 hook at offset {:x}", target.offset());
		}
	};

	struct CompileFilesHook
	{
		static inline REL::Relocation<std::uintptr_t> target{ REL::Offset(0x011e8e0) };  // Skyrim was 0x17eff0

		static inline REL::Relocation<int> iTotalForms{ REL::Offset(0x5935170) };  // Skyrim was 0x1f889b4

		struct TrampolineCall : Xbyak::CodeGenerator
		{
			// Call AddFile method, then jump back to rest of code
			TrampolineCall(std::uintptr_t jmpAfterCall, std::uintptr_t func = stl::unrestricted_cast<std::uintptr_t>(AddFile))
			{
				Xbyak::Label funcLabel;
				mov(rcx, rsi);  // was rbx
				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);
				mov(rcx, jmpAfterCall);
				jmp(rcx);
				L(funcLabel);
				dq(func);
			}
		};

		struct TrampolineCall2 : Xbyak::CodeGenerator
		{
			// Call AddFile method, then jump back to rest of code
			TrampolineCall2(std::uintptr_t jmpAfterCall, std::uintptr_t func = stl::unrestricted_cast<std::uintptr_t>(AddFile))
			{
				Xbyak::Label funcLabel;
				sub(rsp, 0x20);
				call(ptr[rip + funcLabel]);
				add(rsp, 0x20);
				mov(rcx, jmpAfterCall);
				jmp(rcx);
				L(funcLabel);
				dq(func);
			}
		};

		// Hook where TESDataHandler->loadedMods is used, and replace with the ESL/ESP split

		static void EraseLoadedModCountReset()
		{
#ifndef BACKWARDS_COMPATIBLE
			REL::safe_fill(target.address() + 0x6B, REL::NOP, 0x3);
			logger::info("Erased LoadedModCountReset");
#endif
		}

		static void InstallAddFile()
		{
			std::uintptr_t start = target.address() + 0x387; // 0x196
			std::uintptr_t end = target.address() + 0x3AA;   // 0x1b9
			REL::safe_fill(start, REL::NOP, end - start);

			auto trampolineJmp = TrampolineCall(end);

			auto& trampoline = F4SE::GetTrampoline();
			//F4SE::AllocTrampoline(trampolineJmp.getSize());
			auto result = trampoline.allocate(trampolineJmp);
			auto& trampoline2 = F4SE::GetTrampoline();
			//F4SE::AllocTrampoline(14);
			trampoline2.write_branch<5>(start, (std::uintptr_t)result);

			logger::info("Install LoadFilesHook hook at address {:x}", start);
		}

		static void InstallAddFile2()
		{
			std::uintptr_t start = target.address() + 0x3CD;   // 0x1f0
			std::uintptr_t end = target.address() + 0x3F4;     // 0x217
			REL::safe_fill(start, REL::NOP, end - start);

			auto trampolineJmp = TrampolineCall2(end);
			REL::safe_write(start, trampolineJmp.getCode(), trampolineJmp.getSize());

			if (trampolineJmp.getSize() > end - start) {
				logger::critical("InstallAddFile2 trampoline too big by {} bytes!", trampolineJmp.getSize() - (end - start));
			}

			logger::info("Install LoadFilesHook2 hook at address {:x}", start);
		}

		static void OpenTESLoopThunk()
		{
			logger::info("OpenTESLoop invoked!");
			// Replica of what SE does, but for VR
			auto handler = DataHandler::GetSingleton();
			int* totalForms = reinterpret_cast<int*>(iTotalForms.address());
			for (auto file : handler->compiledFileCollection.files) {
				logger::info("OpenTESLoop opening file! {} {:x}", std::string(file->filename), file->compileIndex);
				file->OpenTES(RE::NiFile::OpenMode::kReadOnly, 0);
				//*totalForms += file->unk430;   // ************ FIX
			}
			for (auto file : handler->compiledFileCollection.smallFiles) {
				logger::info("OpenTESLoop opening small file! {} {:x}", std::string(file->filename), file->compileIndex);
				file->OpenTES(RE::NiFile::OpenMode::kReadOnly, 0);
				//*totalForms += file->unk430;   // *************** FIX
			}
			logger::info("OpenTESLoop finished!");
		}

		static void InstallOpenTESLoop()
		{
			std::uintptr_t start = target.address() + 0x3FB;  // 0x21d
			std::uintptr_t end = target.address() + 0x448;    // 0x268
			REL::safe_fill(start, REL::NOP, end - start);

			auto trampolineJmp = TrampolineCall2(end, stl::unrestricted_cast<std::uintptr_t>(OpenTESLoopThunk));
			REL::safe_write(start, trampolineJmp.getCode(), trampolineJmp.getSize());

			if (trampolineJmp.getSize() > end - start) {
				logger::critical("InstallOpenTESLoop trampoline too big by {} bytes!", trampolineJmp.getSize() - (end - start));
			}

			logger::info("Install LoadFilesHook3 hook at address {:x}", start);
		}

		static void ConstructObjectList(DataHandler* a_handler, RE::TESFile* a_file, bool a_isFirstPlugin)
		{
			using func_t = decltype(&ConstructObjectList);
			REL::Relocation<func_t> func{ REL::Offset(0x0120350) };  // 0x180870
			return func(a_handler, a_file, a_isFirstPlugin);
		}

		static void ConstructObjectListThunk()
		{
			logger::info("ConstructObjectListThunk invoked!");
			// Replica of what SE does, but for VR
			auto handler = DataHandler::GetSingleton();
			bool firstPlugin = true;
			auto& overlayFiles = DataHandler::GetSingleton()->compiledFileCollection.overlayFiles;
			for (auto file : handler->files) {
				boolean isCompiledOverlay = std::find(overlayFiles.begin(), overlayFiles.end(), file) != overlayFiles.end();
				if ((isCompiledOverlay || file->compileIndex != 0xFF) && file != handler->activeFile) {
					logger::info("ConstructObjectListThunk on file {} {:x}", std::string(file->filename), file->compileIndex);
					ConstructObjectList(handler, file, firstPlugin);
					firstPlugin = false;
				}
			}
			if (handler->activeFile) {
				logger::info("ConstructObjectListThunk on active file {} {:x}",
					std::string(handler->activeFile->filename),
					handler->activeFile->compileIndex);
				ConstructObjectList(handler, handler->activeFile, firstPlugin);
				firstPlugin = false;
			}
			logger::info("ConstructObjectListThunk finished!");
		}

		static void InstallConstructObjectListLoop()
		{
			std::uintptr_t start = target.address() + 0x475;   // 0x29c
			std::uintptr_t end = target.address() + 0x4A6;     // 0x2d2
			REL::safe_fill(start, REL::NOP, end - start);

			auto trampolineJmp = TrampolineCall2(end, stl::unrestricted_cast<std::uintptr_t>(ConstructObjectListThunk));
			REL::safe_write(start, trampolineJmp.getCode(), trampolineJmp.getSize());

			if (trampolineJmp.getSize() > end - start) {
				logger::critical("InstallConstructObjectListLoop trampoline too big by {} bytes!", trampolineJmp.getSize() - (end - start));
			}

			logger::info("Install LoadFilesHook4 hook at address {:x}", start);
		}

		static void Install()
		{
			EraseLoadedModCountReset();
			InstallAddFile();
			InstallAddFile2();
			InstallOpenTESLoop();
			InstallConstructObjectListLoop();
		}
	};

	namespace eslExtensionHooks
	{
		// For spots where SkyrimVR checks against .esm extension, we also now check against .esl extension
		static bool EslExtensionCheck(char* a_extension, char* a_esm)
		{
			logger::debug("EslExtensionCheck Checking {}", a_extension);
			auto isESM = _stricmp(a_extension, a_esm) == 0;
			auto isESL = _stricmp(a_extension, ".esl") == 0;
			auto result = !(isESM || isESL);
			logger::debug("EslExtensionCheck Is extension ESM or ESL: {}", !result);
			return result;
		}

		struct ParsePluginTXTHook
		{
			// Hook plugin parsing to include .esl along with .esm and .esp

			static const char* thunk(const char* a_fileName, const char* a_esmString)
			{
				auto string = std::strstr(a_fileName, a_esmString);
				if (!string) {
					string = std::strstr(a_fileName, ".esl");
				}
				logger::info("{} returns string {}", a_fileName, string);
				return string;
			}

			static inline REL::Relocation<decltype(thunk)> func;

			static void Install()
			{
				REL::Relocation<std::uintptr_t> target{ REL::Offset(0x1BD9CD0 + 0x1ED) };  // 0xC70FB0 + 0x1ED
				pstl::write_thunk_call<ParsePluginTXTHook>(target.address());
				REL::safe_fill(target.address(), REL::NOP, 0x100);
				logger::info("Hooked PluginParsing at {:x}", target.address());
				logger::info("Hooked PluginParsing at offset {:x}", target.offset());
			}
		};

		struct PrepareBSAHook
		{
			static inline REL::Relocation<std::uintptr_t> target{ REL::Offset(0x0124c30) };  // 0x184360

			static void Install()
			{
				auto& trampoline = F4SE::GetTrampoline();
				//F4SE::AllocTrampoline(14);

				trampoline.write_call<6>(target.address() + 0x82, EslExtensionCheck);
				logger::info("PrepareBSAHook hooked at {:x}", target.address() + 0x82);    // 0x8F
				logger::info("PrepareBSAHook hooked at offset {:x}", target.offset() + 0x82);
			}
		};

		struct UnkUIModHook
		{
			static inline REL::Relocation<std::uintptr_t> target{ REL::Offset(0x0b76c10) };  // 0x52D230

			static void Install()
			{
				auto& trampoline = F4SE::GetTrampoline();
				//F4SE::AllocTrampoline(14);

				trampoline.write_call<6>(target.address() + 0x16B, EslExtensionCheck);   // 0x74
				logger::info("UnkUIModHook hooked at {:x}", target.address() + 0x16B);
				logger::info("UnkUIModHook hooked at offset {:x}", target.offset() + 0x16B);
			}
		};

		struct BuildFileListHook
		{
			static inline REL::Relocation<std::uintptr_t> target{ REL::Offset(0x011dcd0) };  // 0x17e540

			struct TrampolineCall : Xbyak::CodeGenerator
			{
				TrampolineCall(std::uintptr_t jmpAfterCall, std::uintptr_t func)
				{
					Xbyak::Label funcLabel;
					lea(rcx, ptr[rbp + 0x8C]);  // Move fileName into place
					sub(rsp, 0x20);
					call(ptr[rip + funcLabel]);
					add(rsp, 0x20);
					mov(rcx, jmpAfterCall);
					test(rax, rax);
					jmp(rcx);
					L(funcLabel);
					dq(func);
				}
			};

			static bool IsFilenameAPlugin(const char* a_fileName)
			{
				auto string = std::string(a_fileName);
				logger::info("IsFilenameAPlugin Checking {}", string);
				std::transform(string.begin(), string.end(), string.begin(), ::tolower);
				logger::info("IsFilenameAPlugin return {}", string.ends_with(".esl") || string.ends_with(".esm") || string.ends_with(".esp"));
				return !(string.ends_with(".esl") || string.ends_with(".esm") || string.ends_with(".esp"));
			}

			static void Install()
			{
				std::uintptr_t start = target.address() + 0xC0;  // 0xB0
				std::uintptr_t end = target.address() + 0x11B;   // 0x113
				REL::safe_fill(start, REL::NOP, end - start);

				auto trampolineJmp = TrampolineCall(end, stl::unrestricted_cast<std::uintptr_t>(IsFilenameAPlugin));
				REL::safe_write(start, trampolineJmp.getCode(), trampolineJmp.getSize());

				if (trampolineJmp.getSize() > (end - start)) {
					logger::critical("BuildFileListHook is {} bytes too big!", trampolineJmp.getSize() - (end - start));
				}

				logger::info("BuildFileListHook hooked at {:x}", target.address() + 0xC0);
				logger::info("BuildFileListHook hooked at offset {:x}", target.offset() + 0xC0);
			}
		};

		struct ParseINIHook
		{
			static inline REL::Relocation<std::uintptr_t> target{ REL::Offset(0x0d8d7e0) };   // Skyrim was 0x5c18d0

			static std::uint64_t thunk(char a_extension[], const char* a_esm)
			{
				logger::debug("ParseINIHook Checking {}", a_extension);
				auto result = func(a_extension, a_esm);
				if (!result) {
					result = func(a_extension, ".esl");
				}
				return result;
			}

			static inline REL::Relocation<decltype(thunk)> func;

			static void Install()
			{
				pstl::write_thunk_call<ParseINIHook>(target.address() + 0x44);
				logger::info("ParseINIHook hooked at {:x}", target.address() + 0x44);
				logger::info("ParseINIHook hooked at offset {:x}", target.offset() + 0x44);
			}
		};

		struct UnkSetCheckHook
		{
			static inline REL::Relocation<std::uintptr_t> target{ REL::Offset(0x0b6d710) };  // 0x51f890

			static void Install()
			{
				auto& trampoline = F4SE::GetTrampoline();
				//F4SE::AllocTrampoline(14);

				trampoline.write_call<6>(target.address() + 0x206, EslExtensionCheck);   // 0x1ba
				logger::info("UnkSetCheckHook hooked at {:x}", target.address() + 0x206);
				logger::info("UnkSetCheckHook hooked at offset {:x}", target.offset() + 0x206);
			}
		};

		struct UnkHook
		{
			static inline REL::Relocation<std::uintptr_t> target{ REL::Offset(0x0b6d4c0) };  // 0x51dc00

			static bool thunk(char* a_extension, const char* a_esm)
			{
				logger::info("UnkHook Checking {}", a_extension);
				auto isESM = func(a_extension, a_esm) == 0;
				auto isESL = func(a_extension, ".esl") == 0;
				logger::info("UnkHook Result: {}", !(isESM || isESL));
				return !(isESM || isESL);
			}

			static inline REL::Relocation<decltype(thunk)> func;

			static void Install()
			{
				pstl::write_thunk_call<UnkHook>(target.address() + 0x1d8);
				logger::info("UnkHook hooked at {:x}", target.address() + 0x1d8);
				logger::info("UnkHook hooked at offset {:x}", target.offset() + 0x1d8);
			}
		};

		static inline void InstallHooks()
		{
			ParsePluginTXTHook::Install();  // Unused, patching on the off-chance it is
			BuildFileListHook::Install();
			ParseINIHook::Install();    
			PrepareBSAHook::Install();
			UnkSetCheckHook::Install();
			UnkUIModHook::Install();
			UnkHook::Install();
		}
	}

	static inline void InstallHooks()
	{
		AddTESFileHook::Install();
		AddTESFileHook1::Install();
		AddTESFileHook2::Install();
		CompileFilesHook::Install();
		eslExtensionHooks::InstallHooks();
	}
}
