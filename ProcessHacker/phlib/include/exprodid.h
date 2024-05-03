/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     dmex    2020-2022
 *
 */

#ifndef _PH_EXPRODID_H
#define _PH_EXPRODID_H

/*
 * This file contains the required types for the RICH header.
 *
 * References:
 * http://bytepointer.com/articles/the_microsoft_rich_header.htm
 * https://infocon.hackingand.coffee/Hacktivity/Hacktivity%202016/Presentations/George_Webster-and-Julian-Kirsch.pdf
 * https://www.virusbulletin.com/virusbulletin/2020/01/vb2019-paper-rich-headers-leveraging-mysterious-artifact-pe-format/
 */

#define ProdIdTagStart 0x68636952
#define ProdIdTagEnd 0x536e6144

// private
typedef struct PRODITEM
{
    ULONG dwProdid; // Product identity
    ULONG dwCount; // Count of objects built with that product
} PRODITEM, *PPRODITEM;

// This enum was built from https://github.com/kirschju/richheader licensed under GPL3 (dmex)
typedef enum _PRODID
{
    prodidUnknown = 0x0000,
    prodidImport0 = 0x0001, // Linker generated import object version 0
    prodidLinker510 = 0x0002, // LINK 5.10 (Visual Studio 97 SP3)
    prodidCvtomf510 = 0x0003, // LINK 5.10 (Visual Studio 97 SP3) OMF to COFF conversion
    prodidLinker600 = 0x0004, // LINK 6.00 (Visual Studio 98)
    prodidCvtomf600 = 0x0005, // LINK 6.00 (Visual Studio 98) OMF to COFF conversion
    prodidCvtres500 = 0x0006, // CVTRES 5.00
    prodidUtc11_Basic = 0x0007, // VB 5.0 native code
    prodidUtc11_C = 0x0008, // VC++ 5.0 C/C++
    prodidUtc12_Basic = 0x0009, // VB 6.0 native code
    prodidUtc12_C = 0x000A, // VC++ 6.0 C
    prodidUtc12_CPP = 0x000B, // VC++ 6.0 C++
    prodidAliasObj60 = 0x000C, // ALIASOBJ.EXE (CRT Tool that builds OLDNAMES.LIB)
    prodidVisualBasic60 = 0x000D, // VB 6.0 generated object
    prodidMasm613 = 0x000E, // MASM 6.13
    prodidMasm701 = 0x000F, // MASM 7.01 (prodidMasm710)
    prodidLinker511 = 0x0010, // LINK 5.11
    prodidCvtomf511 = 0x0011, // LINK 5.11 OMF to COFF conversion
    prodidMasm614 = 0x0012, // MASM 6.14 (MMX2 support)
    prodidLinker512 = 0x0013, // LINK 5.12
    prodidCvtomf512 = 0x0014, // LINK 5.12 OMF to COFF conversion
    prodidUtc12_C_Std = 0x0015,
    prodidUtc12_CPP_Std = 0x0016,
    prodidUtc12_C_Book = 0x0017,
    prodidUtc12_CPP_Book = 0x0018,
    prodidImplib700 = 0x0019,
    prodidCvtomf700 = 0x001a,
    prodidUtc13_Basic = 0x001b,
    prodidUtc13_C = 0x001c,
    prodidUtc13_CPP = 0x001d,
    prodidLinker610 = 0x001e,
    prodidCvtomf610 = 0x001f,
    prodidLinker601 = 0x0020,
    prodidCvtomf601 = 0x0021,
    prodidUtc12_1_Basic = 0x0022,
    prodidUtc12_1_C = 0x0023,
    prodidUtc12_1_CPP = 0x0024,
    prodidLinker620 = 0x0025,
    prodidCvtomf620 = 0x0026,
    prodidAliasObj70 = 0x0027,
    prodidLinker621 = 0x0028,
    prodidCvtomf621 = 0x0029,
    prodidMasm615 = 0x002a,
    prodidUtc13_LTCG_C = 0x002b,
    prodidUtc13_LTCG_CPP = 0x002c,
    prodidMasm620 = 0x002d,
    prodidILAsm100 = 0x002e,
    prodidUtc12_2_Basic = 0x002f,
    prodidUtc12_2_C = 0x0030,
    prodidUtc12_2_CPP = 0x0031,
    prodidUtc12_2_C_Std = 0x0032,
    prodidUtc12_2_CPP_Std = 0x0033,
    prodidUtc12_2_C_Book = 0x0034,
    prodidUtc12_2_CPP_Book = 0x0035,
    prodidImplib622 = 0x0036,
    prodidCvtomf622 = 0x0037,
    prodidCvtres501 = 0x0038,
    prodidUtc13_C_Std = 0x0039,
    prodidUtc13_CPP_Std = 0x003a,
    prodidCvtpgd1300 = 0x003b,
    prodidLinker622 = 0x003c,
    prodidLinker700 = 0x003d,
    prodidExport622 = 0x003e,
    prodidExport700 = 0x003f,
    prodidMasm700 = 0x0040,
    prodidUtc13_POGO_I_C = 0x0041,
    prodidUtc13_POGO_I_CPP = 0x0042,
    prodidUtc13_POGO_O_C = 0x0043,
    prodidUtc13_POGO_O_CPP = 0x0044,
    prodidCvtres700 = 0x0045,
    prodidCvtres710p = 0x0046,
    prodidLinker710p = 0x0047,
    prodidCvtomf710p = 0x0048,
    prodidExport710p = 0x0049,
    prodidImplib710p = 0x004a,
    prodidMasm710p = 0x004b,
    prodidUtc1310p_C = 0x004c,
    prodidUtc1310p_CPP = 0x004d,
    prodidUtc1310p_C_Std = 0x004e,
    prodidUtc1310p_CPP_Std = 0x004f,
    prodidUtc1310p_LTCG_C = 0x0050,
    prodidUtc1310p_LTCG_CPP = 0x0051,
    prodidUtc1310p_POGO_I_C = 0x0052,
    prodidUtc1310p_POGO_I_CPP = 0x0053,
    prodidUtc1310p_POGO_O_C = 0x0054,
    prodidUtc1310p_POGO_O_CPP = 0x0055,
    prodidLinker624 = 0x0056,
    prodidCvtomf624 = 0x0057,
    prodidExport624 = 0x0058,
    prodidImplib624 = 0x0059,
    prodidLinker710 = 0x005a,
    prodidCvtomf710 = 0x005b,
    prodidExport710 = 0x005c,
    prodidImplib710 = 0x005d,
    prodidCvtres710 = 0x005e,
    prodidUtc1310_C = 0x005f,
    prodidUtc1310_CPP = 0x0060,
    prodidUtc1310_C_Std = 0x0061,
    prodidUtc1310_CPP_Std = 0x0062,
    prodidUtc1310_LTCG_C = 0x0063,
    prodidUtc1310_LTCG_CPP = 0x0064,
    prodidUtc1310_POGO_I_C = 0x0065,
    prodidUtc1310_POGO_I_CPP = 0x0066,
    prodidUtc1310_POGO_O_C = 0x0067,
    prodidUtc1310_POGO_O_CPP = 0x0068,
    prodidAliasObj710 = 0x0069,
    prodidAliasObj710p = 0x006a,
    prodidCvtpgd1310 = 0x006b,
    prodidCvtpgd1310p = 0x006c,
    prodidUtc1400_C = 0x006d,
    prodidUtc1400_CPP = 0x006e,
    prodidUtc1400_C_Std = 0x006f,
    prodidUtc1400_CPP_Std = 0x0070,
    prodidUtc1400_LTCG_C = 0x0071,
    prodidUtc1400_LTCG_CPP = 0x0072,
    prodidUtc1400_POGO_I_C = 0x0073,
    prodidUtc1400_POGO_I_CPP = 0x0074,
    prodidUtc1400_POGO_O_C = 0x0075,
    prodidUtc1400_POGO_O_CPP = 0x0076,
    prodidCvtpgd1400 = 0x0077,
    prodidLinker800 = 0x0078,
    prodidCvtomf800 = 0x0079,
    prodidExport800 = 0x007a,
    prodidImplib800 = 0x007b,
    prodidCvtres800 = 0x007c,
    prodidMasm800 = 0x007d,
    prodidAliasObj800 = 0x007e,
    prodidPhoenixPrerelease = 0x007f,
    prodidUtc1400_CVTCIL_C = 0x0080,
    prodidUtc1400_CVTCIL_CPP = 0x0081,
    prodidUtc1400_LTCG_MSIL = 0x0082,
    prodidUtc1500_C = 0x0083,
    prodidUtc1500_CPP = 0x0084,
    prodidUtc1500_C_Std = 0x0085,
    prodidUtc1500_CPP_Std = 0x0086,
    prodidUtc1500_CVTCIL_C = 0x0087,
    prodidUtc1500_CVTCIL_CPP = 0x0088,
    prodidUtc1500_LTCG_C = 0x0089,
    prodidUtc1500_LTCG_CPP = 0x008a,
    prodidUtc1500_LTCG_MSIL = 0x008b,
    prodidUtc1500_POGO_I_C = 0x008c,
    prodidUtc1500_POGO_I_CPP = 0x008d,
    prodidUtc1500_POGO_O_C = 0x008e,
    prodidUtc1500_POGO_O_CPP = 0x008f,
    prodidCvtpgd1500 = 0x0090,
    prodidLinker900 = 0x0091,
    prodidExport900 = 0x0092,
    prodidImplib900 = 0x0093,
    prodidCvtres900 = 0x0094,
    prodidMasm900 = 0x0095,
    prodidAliasObj900 = 0x0096,
    prodidResource = 0x0097,
    prodidAliasObj1000 = 0x0098,
    prodidCvtpgd1600 = 0x0099,
    prodidCvtres1000 = 0x009a,
    prodidExport1000 = 0x009b,
    prodidImplib1000 = 0x009c,
    prodidLinker1000 = 0x009d,
    prodidMasm1000 = 0x009e,
    prodidPhx1600_C = 0x009f,
    prodidPhx1600_CPP = 0x00a0,
    prodidPhx1600_CVTCIL_C = 0x00a1,
    prodidPhx1600_CVTCIL_CPP = 0x00a2,
    prodidPhx1600_LTCG_C = 0x00a3,
    prodidPhx1600_LTCG_CPP = 0x00a4,
    prodidPhx1600_LTCG_MSIL = 0x00a5,
    prodidPhx1600_POGO_I_C = 0x00a6,
    prodidPhx1600_POGO_I_CPP = 0x00a7,
    prodidPhx1600_POGO_O_C = 0x00a8,
    prodidPhx1600_POGO_O_CPP = 0x00a9,
    prodidUtc1600_C = 0x00aa,
    prodidUtc1600_CPP = 0x00ab,
    prodidUtc1600_CVTCIL_C = 0x00ac,
    prodidUtc1600_CVTCIL_CPP = 0x00ad,
    prodidUtc1600_LTCG_C = 0x00ae,
    prodidUtc1600_LTCG_CPP = 0x00af,
    prodidUtc1600_LTCG_MSIL = 0x00b0,
    prodidUtc1600_POGO_I_C = 0x00b1,
    prodidUtc1600_POGO_I_CPP = 0x00b2,
    prodidUtc1600_POGO_O_C = 0x00b3,
    prodidUtc1600_POGO_O_CPP = 0x00b4,
    prodidAliasObj1010 = 0x00b5,
    prodidCvtpgd1610 = 0x00b6,
    prodidCvtres1010 = 0x00b7,
    prodidExport1010 = 0x00b8,
    prodidImplib1010 = 0x00b9,
    prodidLinker1010 = 0x00ba,
    prodidMasm1010 = 0x00bb,
    prodidUtc1610_C = 0x00bc,
    prodidUtc1610_CPP = 0x00bd,
    prodidUtc1610_CVTCIL_C = 0x00be,
    prodidUtc1610_CVTCIL_CPP = 0x00bf,
    prodidUtc1610_LTCG_C = 0x00c0,
    prodidUtc1610_LTCG_CPP = 0x00c1,
    prodidUtc1610_LTCG_MSIL = 0x00c2,
    prodidUtc1610_POGO_I_C = 0x00c3,
    prodidUtc1610_POGO_I_CPP = 0x00c4,
    prodidUtc1610_POGO_O_C = 0x00c5,
    prodidUtc1610_POGO_O_CPP = 0x00c6,
    prodidAliasObj1100 = 0x00c7,
    prodidCvtpgd1700 = 0x00c8,
    prodidCvtres1100 = 0x00c9,
    prodidExport1100 = 0x00ca,
    prodidImplib1100 = 0x00cb,
    prodidLinker1100 = 0x00cc,
    prodidMasm1100 = 0x00cd,
    prodidUtc1700_C = 0x00ce,
    prodidUtc1700_CPP = 0x00cf,
    prodidUtc1700_CVTCIL_C = 0x00d0,
    prodidUtc1700_CVTCIL_CPP = 0x00d1,
    prodidUtc1700_LTCG_C = 0x00d2,
    prodidUtc1700_LTCG_CPP = 0x00d3,
    prodidUtc1700_LTCG_MSIL = 0x00d4,
    prodidUtc1700_POGO_I_C = 0x00d5,
    prodidUtc1700_POGO_I_CPP = 0x00d6,
    prodidUtc1700_POGO_O_C = 0x00d7,
    prodidUtc1700_POGO_O_CPP = 0x00d8,
    prodidAliasObj1200 = 0x00d9,
    prodidCvtpgd1800 = 0x00da,
    prodidCvtres1200 = 0x00db,
    prodidExport1200 = 0x00dc,
    prodidImplib1200 = 0x00dd,
    prodidLinker1200 = 0x00de,
    prodidMasm1200 = 0x00df,
    prodidUtc1800_C = 0x00e0,
    prodidUtc1800_CPP = 0x00e1,
    prodidUtc1800_CVTCIL_C = 0x00e2,
    prodidUtc1800_CVTCIL_CPP = 0x00e3, // 0x00d3 ??
    prodidUtc1800_LTCG_C = 0x00e4,
    prodidUtc1800_LTCG_CPP = 0x00e5,
    prodidUtc1800_LTCG_MSIL = 0x00e6,
    prodidUtc1800_POGO_I_C = 0x00e7,
    prodidUtc1800_POGO_I_CPP = 0x00e8,
    prodidUtc1800_POGO_O_C = 0x00e9,
    prodidUtc1800_POGO_O_CPP = 0x00ea,
    prodidAliasObj1210 = 0x00eb,
    prodidCvtpgd1810 = 0x00ec,
    prodidCvtres1210 = 0x00ed,
    prodidExport1210 = 0x00ee,
    prodidImplib1210 = 0x00ef,
    prodidLinker1210 = 0x00f0,
    prodidMasm1210 = 0x00f1,
    prodidUtc1810_C = 0x00f2,
    prodidUtc1810_CPP = 0x00f3,
    prodidUtc1810_CVTCIL_C = 0x00f4,
    prodidUtc1810_CVTCIL_CPP = 0x00f5,
    prodidUtc1810_LTCG_C = 0x00f6,
    prodidUtc1810_LTCG_CPP = 0x00f7,
    prodidUtc1810_LTCG_MSIL = 0x00f8,
    prodidUtc1810_POGO_I_C = 0x00f9,
    prodidUtc1810_POGO_I_CPP = 0x00fa,
    prodidUtc1810_POGO_O_C = 0x00fb,
    prodidUtc1810_POGO_O_CPP = 0x00fc,
    prodidAliasObj1400 = 0x00fd,
    prodidCvtpgd1900 = 0x00fe,
    prodidCvtres1400 = 0x00ff,
    prodidExport1400 = 0x0100,
    prodidImplib1400 = 0x0101,
    prodidLinker1400 = 0x0102,
    prodidMasm1400 = 0x0103,
    prodidUtc1900_C = 0x0104,
    prodidUtc1900_CPP = 0x0105,
    prodidUtc1900_CVTCIL_C = 0x0106,
    prodidUtc1900_CVTCIL_CPP = 0x0107,
    prodidUtc1900_LTCG_C = 0x0108,
    prodidUtc1900_LTCG_CPP = 0x0109,
    prodidUtc1900_LTCG_MSIL = 0x010a,
    prodidUtc1900_POGO_I_C = 0x010b,
    prodidUtc1900_POGO_I_CPP = 0x010c,
    prodidUtc1900_POGO_O_C = 0x010d,
    prodidUtc1900_POGO_O_CPP = 0x010e
} PRODID;

#define DwProdidFromProdidWBuild(Prodid, Build) ((((ULONG)(Prodid)) << 16) | (Build))
#define ProdidFromDwProdid(Prodid) ((PRODID)((Prodid) >> 16))
#define WBuildFromDwProdid(Prodid) ((Prodid) & 0xFFFF)

#endif
