#include "DataHandler.h"
#include "Papyrus.h"
#include "Settings.h"
#include "FalloutVRESLAPI.h"
#include "eslhooks.h"
#include "hooks.h"
#include "saveloadhooks.h"
#include "f4sevrhooks.h"
#include "startuphooks.h"
#include "tesfilehooks.h"

int GetMaxStdio()
{
	const auto crtStdioModule = REX::W32::GetModuleHandleW(REL::Module::IsNG() ? L"api-ms-win-crt-runtime-l1-1-0.dll" : L"msvcr110.dll");

	if (!crtStdioModule) {
		logger::critical("crt stdio module not found, failed to check stdio patch");
		return 0;
	}

	const auto maxStdio = reinterpret_cast<decltype(&_getmaxstdio)>(REX::W32::GetProcAddress(crtStdioModule, "_getmaxstdio"));
	const auto result = maxStdio();
	return result;
}

void AllocTrampoline()
{
	auto& trampoline = F4SE::GetTrampoline();
	if (trampoline.empty()) {
		F4SE::AllocTrampoline(1u << 10);
	}
}

void F4SEAPI MessageHandler(F4SE::MessagingInterface::Message* a_message)
{
	switch (a_message->type) {
	case F4SE::MessagingInterface::kPostPostLoad:
		{  // Called after all plugins have finished running
			// F4SEPlugin_Load.
			// It is now safe to do multithreaded operations, or operations against other plugins.
			logger::info("kPostPostLoad");
			F4SE::stl::zstring svNull(nullptr, 0);
			if (F4SE::GetMessagingInterface()->RegisterListener(FalloutVRESLPluginAPI::ModMessageHandler, svNull))
				logger::info("Successfully registered F4SE listener {} with buildnumber {}",
					FalloutVRESLPluginAPI::FalloutVRESLPluginName, g_interface001.GetBuildNumber());
			else
				logger::info("Unable to register F4SE listener");

			if (GetMaxStdio() < 2048)
				logger::warn("Required Buffout MaxStdio patch not detected. FalloutVR will hang if you have more than {} plugins installed in /Data--even if inactive!", GetMaxStdio());
			break;
		}
	case F4SE::MessagingInterface::kGameLoaded:
		{
			logger::info("kGameLoaded: Printing files");
			auto handler = DataHandler::GetSingleton();
			for (auto file : handler->files) {
				logger::info("file {} recordFlags: {:x} index {:x} isOverlay: {}",
					std::string(file->filename),
					file->flags.underlying(),
					file->compileIndex,
					isOverlay(file));
			}

			logger::info("kGameLoaded: Printing loaded files");
			for (std::uint32_t i = 0; i < handler->loadedModCount; i++) {
				auto file = handler->loadedMods[i];
				logger::info("Regular file {} recordFlags: {:x} index {:x}",
					std::string(file->filename),
					file->flags.underlying(),
					file->compileIndex);
			}

			for (auto file : handler->compiledFileCollection.smallFiles) {
				logger::debug("Small file {} recordFlags: {:x} index {:x}",
					std::string(file->filename),
					file->flags.underlying(),
					file->smallFileCompileIndex);
			}

			auto [formMap, lock] = RE::TESForm::GetAllForms();
			lock.get().lock_read();
			for (auto& [formID, form] : *formMap) {
				if (formID >> 24 == 0xFE) {
					logger::info("ESL form (map ID){:x} (real ID){:x} from file {} found", formID, form->formID, std::string(form->GetFile()->filename));
				}
			}
			lock.get().unlock_read();
//			TestGetCompiledFileCollectionExtern();
		}
	default:
		break;
	}
}

void InitializeLog()
{
	auto path = logger::log_directory();
	const auto gamepath = REL::Module::IsVR() ? "Fallout4VR/F4SE" : "Fallout4/F4SE";
	if (!path.value().generic_string().ends_with(gamepath)) {
		// handle bug where game directory is missing
		path = path.value().parent_path().append(gamepath);
	}

	*path /= fmt::format("{}.log"sv, "FalloutVRESL"sv);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto settings = Settings::GetSingleton();

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
	log->set_level(settings->settings.logLevel);
	log->flush_on(settings->settings.flushLevel);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%Y-%m-%d %T.%e][%-16s:%-4#][%L]: %v"s);
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_skse, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < (REL::Module::IsF4() ? F4SE::RUNTIME_LATEST : F4SE::RUNTIME_LATEST_VR)) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	try {
		Settings::GetSingleton()->Load();
	} catch (...) {
		logger::error("Exception caught when loading settings! Default settings will be used");
	}
	InitializeLog();
	logger::info("FalloutVRESL v{}.{}.{} {} {} is loading"sv, Version::MAJOR, Version::MINOR, Version::PATCH, __DATE__, __TIME__);
	const auto runtimeVer = REL::Module::get().version();
	logger::info("Fallout 4 v{}.{}.{}"sv, runtimeVer[0], runtimeVer[1], runtimeVer[2]);
	logger::info("loaded plugin");

	AllocTrampoline();
	F4SE::Init(a_f4se, false);
	auto messaging = F4SE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);
	tesfilehooks::InstallHooks();
	startuphooks::InstallHooks();
	saveloadhooks::InstallHooks();
	eslhooks::InstallHooks();
	DataHandler::InstallHooks();
	SaveLoadGame::InstallHooks();
	F4SEVRHooks::Install(a_f4se->F4SEVersion().pack());
	logger::info("finish hooks");

	auto papyrus = F4SE::GetPapyrusInterface();
	papyrus->Register(Papyrus::Bind);

	return true;
}

 //@brief Get the SSE compatible TESFileCollection for SkyrimVR using GetProcAddress.
 //This should be called after kDataLoaded to ensure it's been populated.
 //This is not intended to be a stable API for other SKSE plugins. Use SkyrimVRESLAPI instead.
 //@return Pointer to TESFileCollection CompiledFileCollection.
extern "C" DLLEXPORT const RE::TESFileCollection* APIENTRY GetCompiledFileCollectionExtern()
{
	const auto& dh = DataHandler::GetSingleton();
	return &(dh->compiledFileCollection);
}
