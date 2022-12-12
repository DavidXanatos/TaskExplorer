#include "stdafx.h"
#include "WinGDI.h"
#include "ProcessHacker.h"
#define GDI_HANDLE_UNIQUE(Handle) ((ULONG)(Handle >> GDI_HANDLE_INDEX_BITS) & GDI_HANDLE_INDEX_MASK)


CWinGDI::CWinGDI(QObject *parent) : CAbstractInfoEx(parent)
{
	m_HandleId = 0;
	m_Object = -1;
}

CWinGDI::~CWinGDI()
{
}

PPH_STRING PhpGetGdiHandleInformation(_In_ ULONG Handle)
{
    HGDIOBJ handle;

    handle = (HGDIOBJ)UlongToPtr(Handle);

    switch (GDI_CLIENT_TYPE_FROM_HANDLE(Handle))
    {
    case GDI_CLIENT_BITMAP_TYPE:
    case GDI_CLIENT_DIBSECTION_TYPE:
        {
            BITMAP bitmap;

            if (GetObject(handle, sizeof(BITMAP), &bitmap))
            {
                return PhFormatString(
                    L"Width: %u, Height: %u, Depth: %u",
                    bitmap.bmWidth,
                    bitmap.bmHeight,
                    bitmap.bmBitsPixel
                    );
            }
        }
        break;
    case GDI_CLIENT_BRUSH_TYPE:
        {
            LOGBRUSH brush;

            if (GetObject(handle, sizeof(LOGBRUSH), &brush))
            {
                return PhFormatString(
                    L"Style: %u, Color: 0x%08x, Hatch: 0x%Ix",
                    brush.lbStyle,
                    _byteswap_ulong(brush.lbColor),
                    brush.lbHatch
                    );
            }
        }
        break;
    case GDI_CLIENT_EXTPEN_TYPE:
        {
            EXTLOGPEN pen;

            if (GetObject(handle, sizeof(EXTLOGPEN), &pen))
            {
                return PhFormatString(
                    L"Style: 0x%x, Width: %u, Color: 0x%08x",
                    pen.elpPenStyle,
                    pen.elpWidth,
                    _byteswap_ulong(pen.elpColor)
                    );
            }
        }
        break;
    case GDI_CLIENT_FONT_TYPE:
        {
            LOGFONT font;

            if (GetObject(handle, sizeof(LOGFONT), &font))
            {
                return PhFormatString(
                    L"Face: %s, Height: %d",
                    font.lfFaceName,
                    font.lfHeight
                    );
            }
        }
        break;
    case GDI_CLIENT_PALETTE_TYPE:
        {
            USHORT count;

            if (GetObject(handle, sizeof(USHORT), &count))
            {
                return PhFormatString(
                    L"Entries: %u",
                    (ULONG)count
                    );
            }
        }
        break;
    case GDI_CLIENT_PEN_TYPE:
        {
            LOGPEN pen;

            if (GetObject(handle, sizeof(LOGPEN), &pen))
            {
                return PhFormatString(
                    L"Style: %u, Width: %u, Color: 0x%08x",
                    pen.lopnStyle,
                    pen.lopnWidth.x,
                    _byteswap_ulong(pen.lopnColor)
                    );
            }
        }
        break;
    }

    return NULL;
}


bool CWinGDI::InitData(quint32 index, struct _GDI_HANDLE_ENTRY* handle, const QString& ProcessName)
{
	QWriteLocker Locker(&m_Mutex);

	m_ProcessName = ProcessName;

	m_ProcessId = handle->Owner.ProcessId;

	m_HandleId = GDI_MAKE_HANDLE(index, handle->Unique);

	m_Object = (quint64)handle->Object;
	m_Informations = CastPhString(PhpGetGdiHandleInformation(m_HandleId));

	return true;
}

QString CWinGDI::GetTypeString() const
{
	QReadLocker Locker(&m_Mutex);

	ulong Unique = GDI_HANDLE_UNIQUE(m_HandleId);
    switch (GDI_CLIENT_TYPE_FROM_UNIQUE(Unique))
    {
    case GDI_CLIENT_ALTDC_TYPE:
        return tr("Alt. DC");
    case GDI_CLIENT_BITMAP_TYPE:
        return tr("Bitmap");
    case GDI_CLIENT_BRUSH_TYPE:
        return tr("Brush");
    case GDI_CLIENT_CLIENTOBJ_TYPE:
        return tr("Client Object");
    case GDI_CLIENT_DIBSECTION_TYPE:
        return tr("DIB Section");
    case GDI_CLIENT_DC_TYPE:
        return tr("DC");
    case GDI_CLIENT_EXTPEN_TYPE:
        return tr("ExtPen");
    case GDI_CLIENT_FONT_TYPE:
        return tr("Font");
    case GDI_CLIENT_METADC16_TYPE:
        return tr("Metafile DC");
    case GDI_CLIENT_METAFILE_TYPE:
        return tr("Enhanced Metafile");
    case GDI_CLIENT_METAFILE16_TYPE:
        return tr("Metafile");
    case GDI_CLIENT_PALETTE_TYPE:
        return tr("Palette");
    case GDI_CLIENT_PEN_TYPE:
        return tr("Pen");
    case GDI_CLIENT_REGION_TYPE:
        return tr("Region");
    default:
        return tr("Unknown");
    }
}