// (c) Copyright 2024 Fatlab Software Pty Ltd.
//
//This library is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 2.1 of the License, or (at your option) any later version.
//
//This library is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the GNU
//Lesser General Public License for more details.
//
//You should have received a copy of the GNU Lesser General Public
//License along with this library; if not, write to the Free Software
//Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110 - 1301  USA

#pragma once
#include <cstdint>
#include <cstring>

#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"

/// @brief 
//All ICMP v4 packets have:
//	an eight - byte header and
//	variable - sized data section.
//	The first four bytes of the header have fixed format
//u8_t		type
//u8_t		code
//u16_t		chk sum
// while the last four bytes depend on the type and code of the ICMP packet.[3]

class IcmpPacket
{
public:
	//Fix on 32 bytes of echo data - NB. Linux default is 56
	constexpr static mem_size_t echo_data_byte_count = PingOptions::FIXED_MESSAGE_BYTE_COUNT;
private:
	unsigned char* _data;
	uint16_t _size;

protected:
	icmp_echo_hdr* Header() { return reinterpret_cast<icmp_echo_hdr*>(_data); }
	const icmp_echo_hdr* Header()const { return reinterpret_cast<const icmp_echo_hdr*>(_data); }
	void Zero() { memset(_data, 0, _size); }

public:
	const unsigned char* Data()const { return _data; }
	uint16_t Size() const { return _size; }

public:
	//  ################################################################
	//  ###################  ICMP   HEADER      ########################
	//  ################################################################
	//	0                   1                   2                   3
	//	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//	| Type          | Code          | Checksum                      |
	//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//	| ....................................................          |
	//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	IcmpPacket(unsigned char* data, const uint16_t size)
		:_data(data), _size(size) {  }
};

// For ECHO packet last four bytes are ident and seq number
//u16_t		ident
//u16_t		seq no
class IcmpEchoRequest : public IcmpPacket
{
public:
	//8 + 32 == 40
	constexpr static mem_size_t echo_byte_count = sizeof(icmp_echo_hdr) + echo_data_byte_count;
	//Our fixed ID
	constexpr static uint16_t PING_ID = 0xABAB;

private:
	unsigned char _echo_data[echo_byte_count];

public:
	//  ################################################################
	//  ###################  ICMP HEADER - ECHO ########################
	//  ################################################################
	//	0                   1                   2                   3
	//	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//	| Type          | Code          | Checksum                      |
	//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//	| ident                         |         seq number            |
	//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	//	| Internet Header + 64 bits of Original Data Datagram           |
	//	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	explicit IcmpEchoRequest(const uint16_t ping_seq_num)
		:IcmpPacket(_echo_data, echo_byte_count)
	{
		Zero();
		ICMPH_TYPE_SET(Header(), ICMP_ECHO); //Echo request
		Header()->id = PING_ID;
		Header()->seqno = htons(ping_seq_num);
		// fill the additional data buffer with some data
		for (auto i = 0u; i < echo_data_byte_count; i++)
			_echo_data[sizeof(icmp_echo_hdr) + i] = static_cast<unsigned char>(i);
		Header()->chksum = inet_chksum(Data(), Size());
	}
};

// For ECHO response packet last four bytes are ident and seq number
//u16_t		ident
//u16_t		seq no
class IcmpEchoResponse : public IcmpPacket
{
public:
	explicit IcmpEchoResponse(unsigned char* data, const uint16_t size)
		:IcmpPacket(data, size)
	{
	}
	/// @brief 
	/// @param ping_seq_num 
	/// @return 
	bool IsValid(const uint16_t ping_seq_num)const
	{
		return  Size() >= sizeof(icmp_echo_hdr) &&
			Header()->type == ICMP_ER &&
			Header()->code == 0u &&
			Header()->id == IcmpEchoRequest::PING_ID &&
			Header()->seqno == htons(ping_seq_num);
	}
};

