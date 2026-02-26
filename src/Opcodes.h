/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/// \addtogroup u2w
/// @{
/// \file

#ifndef _OPCODES_H
#define _OPCODES_H

#include "Define.h"
#include <string>
#include <array>

enum Opcodes : uint16
{
#define X(n, v) n = v,
#include "OpcodeList.inc"
#undef X
};

inline const char* OpcodeToString(uint16_t opcode)
{
    static const std::array<const char*, 0x520> kNames = []() {
        std::array<const char*, 0x520> t{};
#define X(n, v) t[v] = #n;
#include "OpcodeList.inc"
#undef X
        return t;
    }();
    return (opcode < 0x520 && kNames[opcode]) ? kNames[opcode] : "UNKNOWN";
}

enum OpcodeMisc : uint16
{
    NUM_OPCODE_HANDLERS = NUM_MSG_TYPES,
    NULL_OPCODE = 0x0000
};

typedef Opcodes OpcodeClient;
typedef Opcodes OpcodeServer;

/// Player state
enum SessionStatus
{
    STATUS_AUTHED = 0,                                      // Player authenticated (_player == NULL, m_playerRecentlyLogout = false or will be reset before handler call, m_GUID have garbage)
    STATUS_LOGGEDIN,                                        // Player in game (_player != NULL, m_GUID == _player->GetGUID(), inWorld())
    STATUS_TRANSFER,                                        // Player transferring to another map (_player != NULL, m_GUID == _player->GetGUID(), !inWorld())
    STATUS_LOGGEDIN_OR_RECENTLY_LOGGOUT,                    // _player != NULL or _player == NULL && m_playerRecentlyLogout && m_playerLogout, m_GUID store last _player guid)
    STATUS_NEVER,                                           // Opcode not accepted from client (deprecated or server side only)
    STATUS_UNHANDLED                                        // Opcode not handled yet
};

enum PacketProcessing
{
    PROCESS_INPLACE = 0,                                    //process packet whenever we receive it - mostly for non-handled or non-implemented packets
    PROCESS_THREADUNSAFE,                                   //packet is not thread-safe - process it in World::UpdateSessions()
    PROCESS_THREADSAFE                                      //packet is thread-safe - process it in Map::Update()
};

class WorldSession;
class WorldPacket;

class OpcodeHandler
{
public:
    OpcodeHandler(char const* name, SessionStatus status) : Name(name), Status(status) { }
    virtual ~OpcodeHandler() { }

    char const* Name;
    SessionStatus Status;
};

class ClientOpcodeHandler : public OpcodeHandler
{
public:
    ClientOpcodeHandler(char const* name, SessionStatus status, PacketProcessing processing)
        : OpcodeHandler(name, status), ProcessingPlace(processing) { }

    virtual void Call(WorldSession* session, WorldPacket& packet) const = 0;

    PacketProcessing ProcessingPlace;
};

class ServerOpcodeHandler : public OpcodeHandler
{
public:
    ServerOpcodeHandler(char const* name, SessionStatus status)
        : OpcodeHandler(name, status) { }
};

class OpcodeTable
{
    public:
        OpcodeTable();

        OpcodeTable(OpcodeTable const&) = delete;
        OpcodeTable& operator=(OpcodeTable const&) = delete;

        ~OpcodeTable();

        void Initialize();

        ClientOpcodeHandler const* operator[](Opcodes index) const
        {
            return _internalTableClient[index];
        }

    private:
        template<typename Handler, Handler HandlerFunction>
        void ValidateAndSetClientOpcode(OpcodeClient opcode, char const* name, SessionStatus status, PacketProcessing processing);

        void ValidateAndSetServerOpcode(OpcodeServer opcode, char const* name, SessionStatus status);

        ClientOpcodeHandler* _internalTableClient[NUM_OPCODE_HANDLERS];
};

extern OpcodeTable opcodeTable;

/// Lookup opcode name for human understandable logging
std::string GetOpcodeNameForLogging(Opcodes opcode);

#endif
/// @}
