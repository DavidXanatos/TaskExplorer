#include "stdafx.h"
#include "AssemblyList.h"

int _CAssemblyListPtr_type = qRegisterMetaType<CAssemblyListPtr>("CAssemblyListPtr");

CAssemblyList::CAssemblyList()
{

}

void CAssemblyList::AddAssembly(quint64 ID, quint64 ParrentID, QString Structure, QString FileName, QString Flags, QString NativePath)
{
	SAssembly Assembly;
	Assembly.ID = ID;
	Assembly.ParrentID = ParrentID;
	Assembly.Structure = Structure;
	Assembly.FileName = FileName;
	Assembly.Flags = Flags;
	Assembly.NativePath = NativePath;
	m_Assemblies.prepend(Assembly);
}

const CAssemblyList::SAssembly& CAssemblyList::GetAssembly(int index) const
{
	static SAssembly dummy;
	if (index > GetCount())
		return dummy;
	return m_Assemblies.at(index);
}