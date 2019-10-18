#ifndef PROCINFO_H
#define PROCINFO_H

#if defined(__INTELLISENSE__) || defined(__RESHARPER__) || defined(__clang__)
	// None of these seem to know that compiling for kernel mode implies -D_KERNEL_MODE=1 (a reserved define that you can't pass to cl.exe)
	#define _KERNEL_MODE 1
#endif

#include <ntddk.h>

// Help the helpers
#if defined(__INTELLISENSE__) && defined(NT_ASSERT_ACTION)
	// Make Intellisense shut up about every single occurrence of PAGED_CODE() and NT_ASSERT() so it can find something useful instead
	#undef NT_ASSERT_ACTION
	#undef NT_ASSERTMSG_ASSUME
	#undef NT_ASSERTMSGW_ASSUME
	#define NT_ASSERT_ACTION(exp)			(NT_ANALYSIS_ASSUME(exp), 0)
	#define NT_ASSERTMSG_ASSUME(msg, exp)	(NT_ANALYSIS_ASSUME(exp), 0)
	#define NT_ASSERTMSGW_ASSUME(msg, exp)	(NT_ANALYSIS_ASSUME(exp), 0)
#elif defined(__clang__)
	#ifdef PAGED_CODE
		// Help out poor Clang, which has a __readcr8() intrinsic declared in its headers, but of the wrong type and not actually implemented.
		// This breaks anything that touches the IRQL on x64. For us simply getting rid of PAGED_CODE() is enough.
		#undef PAGED_CODE
		#define PAGED_CODE()				((void)0)
	#endif
	#ifdef ALLOC_PRAGMA
		// wdm.h assumes every x64 compiler must be MSVC and defines ALLOC_PRAGMA unconditionally
		#undef ALLOC_PRAGMA
		#undef ALLOC_DATA_PRAGMA
	#endif

	// Don't warn on harmless pragmas, which were added to make MSVC and Prefast shut up in the first place anyway
	#pragma clang diagnostic ignored "-Wunknown-pragmas"

	// Finally, stop this idiotic warning from causing the build to fail.
	// (issued for "MyStruct S = { 0 }", which is perfectly standards compliant and well-defined in both C and C++)
	#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
#if defined(__RESHARPER__)
	// Work around the lame fact that Resharper doesn't understand __declspec(dllimport)
	#undef DECLSPEC_IMPORT
	#define DECLSPEC_IMPORT extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _PROCESS_MITIGATION_POLICY_INFORMATION
{
	PROCESS_MITIGATION_POLICY Policy;
	union
	{
		PROCESS_MITIGATION_ASLR_POLICY ASLRPolicy;
		PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY StrictHandleCheckPolicy;
		PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY SystemCallDisablePolicy;
		PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY ExtensionPointDisablePolicy;
		PROCESS_MITIGATION_DYNAMIC_CODE_POLICY DynamicCodePolicy;
		PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY ControlFlowGuardPolicy;
		PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY SignaturePolicy;
		PROCESS_MITIGATION_FONT_DISABLE_POLICY FontDisablePolicy;
		PROCESS_MITIGATION_IMAGE_LOAD_POLICY ImageLoadPolicy;
		PROCESS_MITIGATION_SYSTEM_CALL_FILTER_POLICY SystemCallFilterPolicy;
		PROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY PayloadRestrictionPolicy;
		PROCESS_MITIGATION_CHILD_PROCESS_POLICY ChildProcessPolicy;
	} u;
} PROCESS_MITIGATION_POLICY_INFORMATION, *PPROCESS_MITIGATION_POLICY_INFORMATION;

typedef enum _PS_PROTECTED_TYPE
{
	PsProtectedTypeNone,
	PsProtectedTypeProtectedLight,
	PsProtectedTypeProtected,
	PsProtectedTypeMax
} PS_PROTECTED_TYPE;

typedef enum _PS_PROTECTED_SIGNER
{
	PsProtectedSignerNone,
	PsProtectedSignerAuthenticode,
	PsProtectedSignerCodeGen,
	PsProtectedSignerAntimalware,
	PsProtectedSignerLsa,
	PsProtectedSignerWindows,
	PsProtectedSignerWinTcb,
	PsProtectedSignerWinSystem,
	PsProtectedSignerApp,
	PsProtectedSignerMax
} PS_PROTECTED_SIGNER;

typedef struct _PS_PROTECTION
{
	union
	{
		struct
		{
			PS_PROTECTED_TYPE Type : 3;
			BOOLEAN Audit : 1;
			PS_PROTECTED_SIGNER Signer : 4;
		} s;
		UCHAR Level;
	};
} PS_PROTECTION, *PPS_PROTECTION;

NTKERNELAPI
BOOLEAN
PsIsSystemProcess(
    _In_ PEPROCESS Process
);

#endif // PROCINFO_H
