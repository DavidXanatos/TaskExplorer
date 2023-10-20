/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     jxy-s   2022
 *
 */

#pragma once

// alloc

#define KPH_TAG_PAGED_LOOKASIDE_OBJECT          '0ApK'
#define KPH_TAG_NPAGED_LOOKASIDE_OBJECT         '1ApK'

// comms

#define KPH_TAG_CLIENT                          '0CpK'
#define KPH_TAG_MESSAGE                         '1CpK'
#define KPH_TAG_NPAGED_MESSAGE                  '2CpK'
#define KPH_TAG_QUEUE_ITEM                      '3CpK'
#define KPH_TAG_THREAD_POOL                     '4CpK'

// dyndata

#define KPH_TAG_DYNDATA                         '0DpK'

// object

#define KPH_TAG_OBJECT_QUERY                    '0OpK'

// process

#define KPH_TAG_PROCESS_INFO                    '0PpK'

// thread

#define KPH_TAG_THREAD_BACK_TRACE               '0TpK'
#define KPH_TAG_THREAD_INFO                     '1TpK'

// util

#define KPH_TAG_REG_STRING                      '0UpK'
#define KPH_TAG_REG_BINARY                      '1UpK'
#define KPH_TAG_FILE_OBJECT_NAME                '2UpK'

// vm

#define KPH_TAG_COPY_VM                         '0vpK'

// debug

#define KPH_TAG_DBG_SLOTS                       '0dpK'

// hash

#define KPH_TAG_HASHING_BUFFER                  '0HpK'
#define KPH_TAG_AUTHENTICODE_SIG                '1HpK'
#define KPH_TAG_HASHING_INFRA                   '2HpK'

// sign

#define KPH_TAG_SIGNING_INFRA                   '0SpK'

// informer

#define KPH_TAG_INFORMER_OB_NAME                '0IpK'
#define KPH_TAG_PROCESS_CREATE_APC              '1IpK'

// cid_tracking

#define KPH_TAG_CID_TABLE                       '0cpK'
#define KPH_TAG_CID_POPULATE                    '1cpK'
#define KPH_TAG_PROCESS_CONTEXT                 '2cpK'
#define KPH_TAG_THREAD_CONTEXT                  '3cpK'
#define KPH_TAG_CID_APC                         '4cpK'

// protection

#define KPH_TAG_IMAGE_LOAD_APC                  '0ppK'

// alpc

#define KPH_TAG_ALPC_NAME_QUERY                 '0apK'
#define KPH_TAG_ALPC_QUERY                      '1apK'

// file

#define KPH_TAG_FILE_QUERY                      '0FpK'
#define KPH_TAG_VOL_FILE_QUERY                  '1FpK'

// socket

#define KPH_TAG_SOCKET                          '0spK'
#define KPH_TAG_TLS                             '1spK'
#define KPH_TAG_TLS_BUFFER                      '2spK'

// back_trace

#define KPH_TAG_BACK_TRACE_OBJECT               '0BpK'
