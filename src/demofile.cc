#include <cstring>
#include <fstream>
#include <iostream>

#include "protobuffs/netmessages_public.pb.h"

#include "demobinreader.h"
#include "demofile.h"

Demofile::Demofile(std::string fn)
    : filename(fn)
{
    fileHandle
        = std::ifstream(filename, std::ios_base::in | std::ios_base::binary);

    if (!fileHandle.is_open()) {
        std::string errmsg = "unable to open file: '" + filename + "'";
        throw std::ios_base::failure(errmsg);
    }

    read_header();

    DemoData *demo_blob = read_next_blob();
    while (demo_blob->cmd != DEMO_STOP) {
        // std::cerr << "CMD TYPE: " << (int32_t)demo_blob->cmd << std::endl;
        // std::cerr << "CMD TICK: " << demo_blob->tick << std::endl;

        if (demo_blob->cmd == DEMO_PACKET) {
            BinReader bin_reader(demo_blob->blob);
            uint32_t packet_type = bin_reader.read_var_uint32();
            uint32_t size = bin_reader.read_var_uint32();

            std::cout << "PACKET TYPE: " << packet_type << std::endl
                      << "PACKET SIZE: " << size << std::endl;

            if (bin_reader.bytes_read + size > demo_blob->blob_size) {
                throw std::ios_base::failure("Failed to parse packet.")
            }
            
            switch (packet_type) {
            // contents of this switch were copied from github.com/ValveSoftware/csgo-demoinfo
#define HANDLE_NetMsg( _x )     case net_ ## _x: PrintNetMessage< CNETMsg_ ## _x, net_ ## _x >( *this, buf.GetBasePointer() + buf.GetNumBytesRead(), Size ); break
#define HANDLE_SvcMsg( _x )     case svc_ ## _x: PrintNetMessage< CSVCMsg_ ## _x, svc_ ## _x >( *this, buf.GetBasePointer() + buf.GetNumBytesRead(), Size ); break

        default:
            // unknown net message
            break;

        HANDLE_NetMsg( NOP );               // 0
        HANDLE_NetMsg( Disconnect );        // 1
        HANDLE_NetMsg( File );              // 2
        HANDLE_NetMsg( Tick );              // 4
        HANDLE_NetMsg( StringCmd );         // 5
        HANDLE_NetMsg( SetConVar );         // 6
        HANDLE_NetMsg( SignonState );       // 7
        HANDLE_SvcMsg( ServerInfo );        // 8
        HANDLE_SvcMsg( SendTable );         // 9
        HANDLE_SvcMsg( ClassInfo );         // 10
        HANDLE_SvcMsg( SetPause );          // 11
        HANDLE_SvcMsg( CreateStringTable ); // 12
        HANDLE_SvcMsg( UpdateStringTable ); // 13
        HANDLE_SvcMsg( VoiceInit );         // 14
        HANDLE_SvcMsg( VoiceData );         // 15
        HANDLE_SvcMsg( Print );             // 16
        HANDLE_SvcMsg( Sounds );            // 17
        HANDLE_SvcMsg( SetView );           // 18
        HANDLE_SvcMsg( FixAngle );          // 19
        HANDLE_SvcMsg( CrosshairAngle );    // 20
        HANDLE_SvcMsg( BSPDecal );          // 21
        HANDLE_SvcMsg( UserMessage );       // 23
        HANDLE_SvcMsg( GameEvent );         // 25
        HANDLE_SvcMsg( PacketEntities );    // 26
        HANDLE_SvcMsg( TempEntities );      // 27
        HANDLE_SvcMsg( Prefetch );          // 28
        HANDLE_SvcMsg( Menu );              // 29
        HANDLE_SvcMsg( GameEventList );     // 30
        HANDLE_SvcMsg( GetCvarValue );      // 31

#undef HANDLE_SvcMsg
#undef HANDLE_NetMsg
            }
        }

        delete[] demo_blob->blob;
        delete demo_blob;

        demo_blob = read_next_blob();
    }

    delete[] demo_blob->blob;
    delete demo_blob;
}


/*
 * Fixme: CDemoFileDump& Demo is our `Demofile` object reference which needs to get
 * .m_GameEventList.CopyFrom( msg ) and .MsgPrintf()
 */
template < class T, int msgType >
void PrintNetMessage( CDemoFileDump& Demo, const void *parseBuffer, int BufferSize )
{
    T msg;

    if ( msg.ParseFromArray( parseBuffer, BufferSize ) )
    {
        if ( msgType == svc_GameEventList )
        {
            Demo.m_GameEventList.CopyFrom( msg );
        }

        Demo.MsgPrintf( msg, BufferSize, "%s", msg.DebugString().c_str() );
    }
}

template <>
void PrintNetMessage< CSVCMsg_UserMessage, svc_UserMessage >( CDemoFileDump& Demo, const void *parseBuffer, int BufferSize )
{
    Demo.DumpUserMessage( parseBuffer, BufferSize );
}

player_info_t *FindPlayerByEntity( int entityId )
{
    for ( std::vector< player_info_t >::iterator j = s_PlayerInfos.begin(); j != s_PlayerInfos.end(); j++ )
    {
        if ( j->entityID == entityId )
        {
            return &(*j);
        }
    }

    return NULL;
}

template <>
void PrintNetMessage< CSVCMsg_GameEvent, svc_GameEvent >( CDemoFileDump& Demo, const void *parseBuffer, int BufferSize )
{
    CSVCMsg_GameEvent msg;

    if ( msg.ParseFromArray( parseBuffer, BufferSize ) )
    {
        const CSVCMsg_GameEventList::descriptor_t *pDescriptor = GetGameEventDescriptor( msg, Demo );
        if ( pDescriptor )
        {
            ParseGameEvent( msg, pDescriptor );
        }
    }
}

template <>
void PrintNetMessage< CSVCMsg_CreateStringTable, svc_CreateStringTable >( CDemoFileDump& Demo, const void *parseBuffer, int BufferSize )
{
    CSVCMsg_CreateStringTable msg;

    if ( msg.ParseFromArray( parseBuffer, BufferSize ) )
    {
        bool bIsUserInfo = !strcmp( msg.name().c_str(), "userinfo" );
        if ( g_bDumpStringTables )
        {
            printf( "CreateStringTable:%s:%d:%d:%d:%d:\n", msg.name().c_str(), msg.max_entries(), msg.num_entries(), msg.user_data_size(), msg.user_data_size_bits() );
        }
        CBitRead data( &msg.string_data()[ 0 ], msg.string_data().size() );
        ParseStringTableUpdate( data,  msg.num_entries(), msg.max_entries(), msg.user_data_size(), msg.user_data_size_bits(), msg.user_data_fixed_size(), bIsUserInfo );

        snprintf( s_StringTables[ s_nNumStringTables ].szName, sizeof( s_StringTables[ s_nNumStringTables ].szName ), "%s", msg.name().c_str() );
        s_StringTables[ s_nNumStringTables ].nMaxEntries = msg.max_entries();
        s_StringTables[ s_nNumStringTables ].nUserDataSize = msg.user_data_size();
        s_StringTables[ s_nNumStringTables ].nUserDataSizeBits = msg.user_data_size_bits();
        s_StringTables[ s_nNumStringTables ].nUserDataFixedSize = msg.user_data_fixed_size();
        s_nNumStringTables++;
    }
}

template <>
void PrintNetMessage< CSVCMsg_UpdateStringTable, svc_UpdateStringTable >( CDemoFileDump& Demo, const void *parseBuffer, int BufferSize )
{
    CSVCMsg_UpdateStringTable msg;

    if ( msg.ParseFromArray( parseBuffer, BufferSize ) )
    {
        CBitRead data( &msg.string_data()[ 0 ], msg.string_data().size() );

        if ( msg.table_id() < s_nNumStringTables && s_StringTables[ msg.table_id() ].nMaxEntries > msg.num_changed_entries() )
        {
            const StringTableData_t &table = s_StringTables[ msg.table_id() ];
            bool bIsUserInfo = !strcmp( table.szName, "userinfo" );
            if ( g_bDumpStringTables )
            {
                printf( "UpdateStringTable:%d(%s):%d:\n", msg.table_id(), table.szName, msg.num_changed_entries() );
            }
            ParseStringTableUpdate( data, msg.num_changed_entries(), table.nMaxEntries, table.nUserDataSize, table.nUserDataSizeBits, table.nUserDataFixedSize, bIsUserInfo );
        }
        else
        {
            printf( "Bad UpdateStringTable:%d:%d!\n", msg.table_id(), msg.num_changed_entries() );
        }
    }
}

template <>
void PrintNetMessage< CSVCMsg_SendTable, svc_SendTable >( CDemoFileDump& Demo, const void *parseBuffer, int BufferSize )
{
    CSVCMsg_SendTable msg;

    if ( msg.ParseFromArray( parseBuffer, BufferSize ) )
    {
        RecvTable_ReadInfos( msg );
    }
}

template <>
void PrintNetMessage< CSVCMsg_PacketEntities, svc_PacketEntities >( CDemoFileDump& Demo, const void *parseBuffer, int BufferSize )
{
    CSVCMsg_PacketEntities msg;

    if ( msg.ParseFromArray( parseBuffer, BufferSize ) )
    {
        CBitRead entityBitBuffer( &msg.entity_data()[ 0 ], msg.entity_data().size() );
        bool bAsDelta = msg.is_delta();
        int nHeaderCount = msg.updated_entries();
        int nBaseline = msg.baseline();
        bool bUpdateBaselines = msg.update_baseline();
        int nHeaderBase = -1;
        int nNewEntity = -1;
        int UpdateFlags = 0;

        (void) nBaseline;
        (void) bUpdateBaselines;

        UpdateType updateType = PreserveEnt;

        while ( updateType < Finished )
        {
            nHeaderCount--;

            bool bIsEntity = ( nHeaderCount >= 0 ) ? true : false;

            if ( bIsEntity  )
            {
                UpdateFlags = FHDR_ZERO;

                nNewEntity = nHeaderBase + 1 + entityBitBuffer.ReadUBitVar();
                nHeaderBase = nNewEntity;

                // leave pvs flag
                if ( entityBitBuffer.ReadOneBit() == 0 )
                {
                    // enter pvs flag
                    if ( entityBitBuffer.ReadOneBit() != 0 )
                    {
                        UpdateFlags |= FHDR_ENTERPVS;
                    }
                }
                else
                {
                    UpdateFlags |= FHDR_LEAVEPVS;

                    // Force delete flag
                    if ( entityBitBuffer.ReadOneBit() != 0 )
                    {
                        UpdateFlags |= FHDR_DELETE;
                    }
                }
            }

            for ( updateType = PreserveEnt; updateType == PreserveEnt; )
            {
                // Figure out what kind of an update this is.
                if ( !bIsEntity || nNewEntity > ENTITY_SENTINEL)
                {
                    updateType = Finished;
                }
                else
                {
                    if ( UpdateFlags & FHDR_ENTERPVS )
                    {
                        updateType = EnterPVS;
                    }
                    else if ( UpdateFlags & FHDR_LEAVEPVS )
                    {
                        updateType = LeavePVS;
                    }
                    else
                    {
                        updateType = DeltaEnt;
                    }
                }
              switch( updateType )
                {
                    case EnterPVS:
                        {
                            uint32 uClass = entityBitBuffer.ReadUBitLong( s_nServerClassBits );
                            uint32 uSerialNum = entityBitBuffer.ReadUBitLong( NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS );
                            if ( g_bDumpPacketEntities )
                            {
                                printf( "Entity Enters PVS: id:%d, class:%d, serial:%d\n", nNewEntity, uClass, uSerialNum );
                            }
                            EntityEntry *pEntity = AddEntity( nNewEntity, uClass, uSerialNum );
                            if ( !ReadNewEntity( entityBitBuffer, pEntity ) )
                            {
                                printf( "*****Error reading entity! Bailing on this PacketEntities!\n" );
                                return;
                            }
                        }
                        break;

                    case LeavePVS:
                        {
                            if ( !bAsDelta )  // Should never happen on a full update.
                            {
                                printf( "WARNING: LeavePVS on full update" );
                                updateType = Failed;    // break out
                                assert( 0 );
                            }
                            else
                            {
                                if ( g_bDumpPacketEntities )
                                {
                                    if ( UpdateFlags & FHDR_DELETE )
                                    {
                                        printf( "Entity leaves PVS and is deleted: id:%d\n", nNewEntity );
                                    }
                                    else
                                    {
                                        printf( "Entity leaves PVS: id:%d\n", nNewEntity );
                                    }
                                }
                                RemoveEntity( nNewEntity );
                            }
                        }
                        break;

                    case DeltaEnt:
                        {
                            EntityEntry *pEntity = FindEntity( nNewEntity );
                            if ( pEntity )
                            {
                                if ( g_bDumpPacketEntities )
                                {
                                    printf( "Entity Delta update: id:%d, class:%d, serial:%d\n", pEntity->m_nEntity, pEntity->m_uClass, pEntity->m_uSerialNum );
                                }
                                if ( !ReadNewEntity( entityBitBuffer, pEntity ) )
                                {
                                    printf( "*****Error reading entity! Bailing on this PacketEntities!\n" );
                                    return;
                                }
                            }
                            else
                            {
                                assert(0);
                            }
                        }
                        break;

                    case PreserveEnt:
                        {
                            if ( !bAsDelta )  // Should never happen on a full update.
                            {
                                printf( "WARNING: PreserveEnt on full update" );
                                updateType = Failed;    // break out
                                assert( 0 );
                            }
                            else
                            {
                                if ( nNewEntity >= MAX_EDICTS )
                                {
                                    printf( "PreserveEnt: nNewEntity == MAX_EDICTS" );
                                    assert( 0 );
                                }
                                else
                                {
                                    if ( g_bDumpPacketEntities )
                                    {
                                        printf( "PreserveEnt: id:%d\n", nNewEntity );
                                    }
                                }
                            }
                        }
                        break;

                    default:
                        break;
                }
            }
        }
    }
}


Demofile::~Demofile(void) { fileHandle.close(); }

void Demofile::read_header(void)
{
    fileHandle.read((char *)&header, sizeof(demoheader_t));
    if (!fileHandle) {
        throw std::ios_base::failure("unable to read header");
    }
    check_header();
}

void Demofile::check_header(void)
{
    if (strcmp(DEMO_HEADER_ID, header.demofilestamp)) {
        throw std::runtime_error(std::string("wrong demo header ID: ")
                                 + std::string(header.demofilestamp));
    }

    if (header.demoprotocol != DEMO_PROTOCOL) {
        throw std::runtime_error(std::string("wrong demo header protocol: ")
                                 + std::string(header.demofilestamp));
    }
}

void Demofile::read_cmd_header(DemoData *d)
{
    fileHandle.read((char *)&d->cmd, sizeof(unsigned char));
    if (d->cmd <= 0) {
        throw std::runtime_error("No end tag in demo file");
    }

    if (d->cmd > DEMO_LASTCMD) {
        throw std::runtime_error("Unknown command in demo file");
    }

    fileHandle.read((char *)&d->tick, sizeof(int32_t));
    fileHandle.read((char *)&d->playerSlot, sizeof(unsigned char));

    if (!fileHandle) {
        throw std::runtime_error("Couldn't read CMD header successfully");
    }
}

void Demofile::read_cmd_info(democmdinfo_t &info)
{
    fileHandle.read((char *)&info, sizeof(democmdinfo_t));

    if (!fileHandle) {
        throw std::runtime_error("Couldn't read CMD info successfully");
    }
}

void Demofile::read_blob(DemoData *cmd, bool skip)
{
    /* FIXME: if skip set, only use fseek to jump over a blob */
    fileHandle.read((char *)&cmd->blob_size, sizeof(int32_t));

    /* FIXME: take care of possible exceptions */
    cmd->blob = new char[cmd->blob_size];

    fileHandle.read(cmd->blob, cmd->blob_size);
    if (!fileHandle) {
        std::string reason;
        if (fileHandle.eofbit)
            reason = "reached eof";
        else if (fileHandle.badbit)
            reason = "memory shortage of ifstream except";
        else if (fileHandle.failbit)
            reason = "total crap";
        throw std::runtime_error(
            std::string("Couldn't read a blob successfully: ") + reason);
    }
}

void Demofile::print_game_details(void)
{
    std::cout << "ID: " << header.demofilestamp
              << "; PROTOCOL: " << header.demoprotocol << std::endl
              << "========================" << std::endl
              << "Server name:       " << header.servername << std::endl
              << "Client name:       " << header.clientname << std::endl
              << "Map name:          " << header.mapname << std::endl
              << "Game lenght (min): " << header.playback_time / 60 << std::endl
              << "------------------------" << std::endl
              << "networkprotocol:   " << header.networkprotocol << std::endl
              << "gamedirectory:     " << header.gamedirectory << std::endl
              << "playback_ticks:    " << header.playback_ticks << std::endl
              << "playback_frames:   " << header.playback_frames << std::endl
              << "signonlength:      " << header.signonlength << std::endl;
}

DemoData *Demofile::read_next_blob(void)
{
    DemoData *blob = new DemoData;
    blob->blob = NULL;

    read_cmd_header(blob);

    switch (blob->cmd) {
    case DEMO_SYNCTICK:
    case DEMO_STOP:
        break;

    case DEMO_CONSOLECMD:
        read_blob(blob);
        break;

    case DEMO_DATATABLES:
        read_blob(blob);
        break;

    case DEMO_STRINGTABLES:
        read_blob(blob);
        break;

    case DEMO_USERCMD:
        int32_t dummy; // FIXME: use fseek later to skip dummy data
        fileHandle.read((char *)&dummy, sizeof(int32_t));
        read_blob(blob);
        break;

    case DEMO_SIGNON:
    case DEMO_PACKET: {
        int32_t dummy_seq;
        democmdinfo_t demoinfo;
        read_cmd_info(demoinfo);

        // TODO: read sequence function
        fileHandle.read((char *)&dummy_seq, sizeof(int32_t));
        fileHandle.read((char *)&dummy_seq, sizeof(int32_t));

        read_blob(blob);
    } break;

    default:
        break;
    }

    return blob;
}

static std::string get_netmsg_name(int cmd_num)
{
    if (NET_Messages_IsValid(cmd_num)) {
        return NET_Messages_Name((NET_Messages)cmd_num);
    } else if (SVC_Messages_IsValid(cmd_num)) {
        return SVC_Messages_Name((SVC_Messages)cmd_num);
    }

    return "NETMSG_???";
}
