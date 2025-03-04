#pragma once

namespace Papyrus
{
	static const int LIGHT_MOD_OFFSET = 0x100;

// Hook SKSEVR papyrus functions and use our version which mimics SE/AE version to include ESL
#define BIND(a_method, ...) a_vm->BindNativeMethod(script, #a_method##sv, a_method __VA_OPT__(, ) __VA_ARGS__)
	using VM = RE::BSScript::IVirtualMachine;
	using StackID = RE::VMStackID;

	inline constexpr auto script = "Game"sv;

	bool Bind(VM* a_vm);
}
