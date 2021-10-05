#include "stdafx.h"
#include "RpcEndpoint.h"


CRpcEndpoint::CRpcEndpoint(QObject *parent) : CAbstractInfoEx(parent)
{
}

CRpcEndpoint::~CRpcEndpoint()
{
}

bool CRpcEndpoint::UpdateDynamicData(void* hEnumBind)
{
	/*if (UuidToString(&uuid, &str) == RPC_S_OK) {
		pRpcEndpoint->m_UUID = QString::fromWCharArray((wchar_t*)str);
		RpcStringFree(&str);
	}

	if (verbosity >= 1) {
		RPC_IF_ID_VECTOR* pVector;
		RPC_STATS_VECTOR* pStats;

		RPC_WSTR strBinding = NULL;
		RPC_WSTR strObj = NULL;
		RPC_WSTR strProtseq = NULL;
		RPC_WSTR strNetaddr = NULL;
		RPC_WSTR strEndpoint = NULL;
		RPC_WSTR strNetoptions = NULL;
		RPC_BINDING_HANDLE hIfidsBind;

		//
		// Ask the RPC server for its supported interfaces
		//
		//
		// Because some of the binding handles may refer to
		// the machine name, or a NAT'd address that we may
		// not be able to resolve/reach, parse the binding and
		// replace the network address with the one specified
		// from the command line.  Unfortunately, this won't
		// work for ncacn_nb_tcp bindings because the actual
		// NetBIOS name is required.  So special case those.
		//
		// Also, skip ncalrpc bindings, as they are not
		// reachable from a remote machine.
		//
		rpcErr2 = RpcBindingToStringBinding(hEnumBind, &strBinding);
		RpcBindingFree(&hEnumBind);
		if (rpcErr2 != RPC_S_OK) {
			fprintf(stderr, ("RpcBindingToStringBinding failed\n"));
			printf("\n");
			continue;
		}

		rpcErr2 = RpcStringBindingParse(
			strBinding,
			&strObj,
			&strProtseq,
			&strNetaddr,
			&strEndpoint,
			&strNetoptions
		);
		RpcStringFree(&strBinding);
		strBinding = NULL;
		if (rpcErr2 != RPC_S_OK) {
			fprintf(stderr, ("RpcStringBindingParse failed\n"));
			printf("\n");
			continue;
		}

		rpcErr2 = RpcStringBindingCompose(
			strObj,
			strProtseq,
			wcscmp(L"ncacn_nb_tcp", (LPCWSTR)strProtseq) == 0 ? strNetaddr : server,
			strEndpoint, strNetoptions,
			&strBinding
		);
		RpcStringFree(&strObj);
		RpcStringFree(&strProtseq);
		RpcStringFree(&strNetaddr);
		RpcStringFree(&strEndpoint);
		RpcStringFree(&strNetoptions);
		if (rpcErr2 != RPC_S_OK) {
			fprintf(stderr, ("RpcStringBindingCompose failed\n"));
			printf("\n");
			continue;
		}

		rpcErr2 = RpcBindingFromStringBinding(strBinding, &hIfidsBind);
		RpcStringFree(&strBinding);
		if (rpcErr2 != RPC_S_OK) {
			fprintf(stderr, ("RpcBindingFromStringBinding failed\n"));
			printf("\n");
			continue;
		}

		if ((rpcErr2 = RpcMgmtInqIfIds(hIfidsBind, &pVector)) == RPC_S_OK) {
			unsigned int i;
			wprintf(L"RpcMgmtInqIfIds succeeded\n");
			wprintf(L"Interfaces: %d\n", pVector->Count);
			for (i = 0; i < pVector->Count; i++) {
				RPC_WSTR str = NULL;
				UuidToString(&pVector->IfId[i]->Uuid, &str);
				wprintf(L"  %s v%d.%d\n", str ? str : (RPC_WSTR)L"(null)",
					pVector->IfId[i]->VersMajor,
					pVector->IfId[i]->VersMinor);
				if (str) RpcStringFree(&str);
			}
			RpcIfIdVectorFree(&pVector);
		}
		else {
			wprintf(L"RpcMgmtInqIfIds failed: 0x%x\n", rpcErr2);
		}

		//if (verbosity >= 2) { // No extra verbosity check -v should be enough
		if ((rpcErr2 = RpcMgmtInqServerPrincName(
			hEnumBind,
			RPC_C_AUTHN_WINNT,
			&princName
		)) == RPC_S_OK) {
			wprintf(L"RpcMgmtInqServerPrincName succeeded\n");
			wprintf(L"Name: %s\n", princName);
			RpcStringFree(&princName);
		}
		else {
			wprintf(L"RpcMgmtInqServerPrincName failed: 0x%x\n", rpcErr2);
		}

		if ((rpcErr2 = RpcMgmtInqStats(
			hEnumBind,
			&pStats
		)) == RPC_S_OK) {
			unsigned int i;
			wprintf(L"RpcMgmtInqStats succeeded\n");
			for (i = 0; i < pStats->Count; i++) {
				wprintf(L"  Stats[%d]: %d\n", i, pStats->Stats[i]);
			}
			RpcMgmtStatsVectorFree(&pStats);
		}
		else {
			wprintf(L"RpcMgmtInqStats failed: 0x%x\n", rpcErr2);
		}
		//}
		RpcBindingFree(&hIfidsBind);
	}
	wprintf(L"\n");
	*/

	return false;
}