// (c) Copyright 2024 Fatlab Software Pty Ltd.
//
// ICMP Ping library for the ESP32
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

#include "Esp32IcmpPing.h"
#include "IcmpPacket.h"
#include <FixedString.h>

#include <WiFi.h>

#include "lwip/ip.h"
#include "lwip/sockets.h"

#include <cmath>
#include <sys/time.h>

/// @brief
/// @param sock_fd
/// @param seq_num
/// @return
bool Esp32IcmpPing::Send(uint32_t ip4, int sock_fd, const uint16_t seq_num)
{
	const IcmpEchoRequest request(seq_num);
	// Target address
	sockaddr_in to;
	memset(&to, 0, sizeof(to));
	to.sin_len = sizeof(to);
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = ip4;
	return sendto(sock_fd, request.Data(), request.Size(), 0, reinterpret_cast<sockaddr *>(&to), sizeof(to)) > 0;
}

/// @brief
/// @param sock_fd
/// @param seq_num
/// @param elapsed
/// @return
bool Esp32IcmpPing::Receive(const int sock_fd, const uint16_t seq_num, float &elapsedMs, bool &canContinue)
{
	// Recv 2 headers 8 + 20 == 28 bytes
	constexpr mem_size_t echo_recv_byte_hdr = sizeof(ip_hdr) + sizeof(icmp_echo_hdr);
	constexpr mem_size_t min_echo_recv_byte_count = 64;
	elapsedMs = 0.0f;
	canContinue = false;
	// Register begin time
	timeval begin;
	gettimeofday(&begin, nullptr);
	// Recv
	sockaddr_in from;
	socklen_t from_len = sizeof(from);
	unsigned char echo_packet[min_echo_recv_byte_count];
	const auto len = recvfrom(sock_fd, echo_packet, sizeof(echo_packet), 0,
							  reinterpret_cast<sockaddr *>(&from), &from_len);
	if (len < 0)
	{
		auto e = errno;
		if (e == EAGAIN || e == EWOULDBLOCK)
		{
			canContinue = true;
			return ErrorLn("Timed out", errno);
		}
		else
			return ErrorLn("Bad receive", errno);
	}
	if (len == 0)
		return ErrorLn("Connection closed");
	if (len < echo_recv_byte_hdr)
		return ErrorLn("Response too small");

	/// Get from IP address
	// ip4_addr_t from_addr;
	// inet_addr_to_ip4addr(&from_addr, &from.sin_addr);
	//  Get echo
	const auto ipHeaderBytes = IPH_HL(reinterpret_cast<ip_hdr *>(echo_packet)) * sizeof(uint32_t);
	const IcmpEchoResponse echoResponse(echo_packet + ipHeaderBytes, len - ipHeaderBytes);

	if (!echoResponse.IsValid(seq_num))
		return ErrorLn("Invalid response");
	// Register end time
	timeval end;
	gettimeofday(&end, nullptr);
	// Get elapsed time in milliseconds
	auto micros_begin = begin.tv_sec * 1000000ul;
	micros_begin += begin.tv_usec;
	auto micros_end = end.tv_sec * 1000000ul;
	micros_end += end.tv_usec;
	elapsedMs = static_cast<float>(micros_end - micros_begin) / static_cast<float>(1000.0);
	canContinue = true;
	if (elapsedMs <= 0.0)
		return ErrorLn("Bad time calc");
	return true;
}

/// @brief
/// @param str
/// @param errorNo
/// @return
bool Esp32IcmpPing::ErrorLn(const char *str, int errorNo)
{
	if (_printer == nullptr)
		return false;
	_printer->print(str);
	_printer->print("- Error no:");
	_printer->print(errorNo);
	_printer->println();
	return false;
}

/// @brief
/// @param sock_fd
/// @return
bool Esp32IcmpPing::CreateAndSetUpSocket(int &sock_fd)
{
	// Create socket
	sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock_fd < 0)
		return ErrorLn("Failed to create socket");
	// Setup socket
	timeval tout;
	// Timeout
	tout.tv_sec = Options().ReceiveTimeoutSeconds();
	tout.tv_usec = Options().ReceiveTimeoutMicros();

	// Set receive time out
	if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tout, sizeof(tout)) < 0)
	{
		closesocket(sock_fd);
		return ErrorLn("Failed to set timeout");
	}
	return true;
}

/// @brief
/// @param result
/// @param printer
/// @return
bool Esp32IcmpPing::ping(PingResults &result, Print *printer)
{
	result = PingResults();
	if (_inPing)
		return ErrorLn("Already in Ping!");
	_inPing = true;
	auto ret = CallPing(result, printer);
	_inPing = false;
	return ret;
}

/// @brief
/// @param result
/// @param printer
/// @return
bool Esp32IcmpPing::CallPing(PingResults &result, Print *printer)
{
	if (printer != nullptr)
		_printer = printer;
	// Extract a valid host address
	uint32_t ip4 = 0u;
	if (!Options().GetAddress(ip4, _printer))
		return false;
	// Check valid
	if (!Options().IsValid())
		return ErrorLn("Invalid Options");
	int sock_fd = -1;
	if (!CreateAndSetUpSocket(sock_fd))
		return false;
	// Track data
	uint8_t transmitted = 0u;
	uint8_t received = 0u;
	auto time_elapsed_ms = 0u;
	const auto ping_started_time = millis();
	// Calculations
	float min_time_ms = 1.E+9f; // FLT_MAX;
	float max_time_ms = 0.0f;
	// Calc SD
	float times_ms[PingOptions::MAX_COUNT];
	float mean_total_ms = 0.0f;

	for (uint16_t seq_num = 1; seq_num <= Options().Count(); ++seq_num)
	{
		// OutputLn("Sending echo request...");
		if (!Send(ip4, sock_fd, seq_num))
		{
			ErrorLn("Failed to send", errno);
			break;
		}
		transmitted++;
		// OutputLn("Receiving echo response...");
		bool canContinue = false;
		if (Receive(sock_fd, seq_num, times_ms[received], canContinue))
		{
			// Update statistics
			// Mean and variance are computed in an incremental way
			if (times_ms[received] < min_time_ms)
				min_time_ms = times_ms[received];
			if (times_ms[received] > max_time_ms)
				max_time_ms = times_ms[received];
			mean_total_ms += times_ms[received];
			received++;
		}
		if (!canContinue)
			break; //done

		time_elapsed_ms = millis() - ping_started_time;
		if (time_elapsed_ms > Options().TotalTimeoutMs())
		{
			if (seq_num < Options().Count())
				ErrorLn("Timed out overall");
			break;
		}
		yield(); // Allow other code to run
	}
	closesocket(sock_fd);

	float mean_ms = received > 0u ? mean_total_ms / static_cast<float>(received) : 0.0f;
	float var_total_ms = 0.0f;
	for (auto i = 0u; i < received; ++i)
	{
		auto diff = times_ms[i] - mean_ms;
		var_total_ms += diff * diff;
	}
	float sd_ms = var_total_ms > 0.0f ? sqrt(var_total_ms / static_cast<float>(received)) : 0.0f;

	result.SetResults(transmitted, received, 
					static_cast<uint16_t>(time_elapsed_ms),
					min_time_ms, 
					max_time_ms, 
					mean_ms, 
					sd_ms);
	// Return true if at least one ping had a "pong"
	return received > 0u;
}

/// @brief
/// @param printer
bool PingOptions::GetAddress(uint32_t &ip4, Print *printer) const
{
	ip4 = 0u;
	if (_host.length() > 0)
	{
		IPAddress remote_addr;
		if (!WiFi.hostByName(_host.c_str(), remote_addr))
		{
			if (printer != nullptr)
				printer->printf("Cannot resolve host: <%s>\r\n", _host.c_str());
			return false;
		}
		ip4 = (uint32_t)remote_addr;
	}
	else if (_ip4 != 0u)
	{
		ip4 = _ip4;
	}
	else if (printer != nullptr)
		printer->println("No address set");

	return ip4 != 0u;
}

/// @brief
/// @param printer
void PingOptions::PrintState(Print *printer) const
{
	if (printer == nullptr)
		return;
	printer->print("Address: ");
	if (_host.length() > 0)
	{
		printer->println(_host);
	}
	else
	{
		ip4_addr_t ip4Addr;
		ip4Addr.addr = _ip4;
		FixedString<32> buf = inet_ntoa(ip4Addr);
		printer->println(buf.c_str());
	}
	printer->printf("Count: %u\r\n", (unsigned int)Count());
	printer->printf("Timeout Recv: %u ms\r\n", (unsigned int)ReceiveTimeoutMs());
	printer->printf("Timeout Total: %u ms\r\n", (unsigned int)TotalTimeoutMs());
}

/// @brief
/// @param printer
void PingResults::PrintState(Print *printer) const
{
	if (printer == nullptr)
		return;
	printer->print(ResultString(false));
	printer->printf("Min response time %.2f ms\r\n", MinTimeMs());
	printer->printf("Max response time %.2f ms\r\n", MaxTimeMs());
	printer->printf("Ave. response time %.2f ms\r\n", AveTimeMs());
	printer->printf("Std Dev. in response time %.2f ms\r\n", StdDevTimeMs());
	printer->printf("Total time taken %u ms\r\n", (unsigned int)TotalTimeMs());
}

/// @brief
/// @return
String PingResults::ResultString(bool includeTimes) const
{
	FixedString<128> sf;
	sf.format("Packets: Sent = %u, Received = %u, Lost = %u (%u%% loss)\n",
			  (unsigned int)Transmitted(),
			  (unsigned int)Received(),
			  (unsigned int)TimeoutCount(),
			  (unsigned int)PercentTransmitted());
	String s = sf.c_str();
	if (!includeTimes)
		return s;
	// Allow for limit printing floats
	sf.format("Times: Min = %ums, Max = %ums, Ave = %ums, SD = %ums\n",
			  (unsigned int)MinTimeMs(),
			  (unsigned int)MaxTimeMs(),
			  (unsigned int)AveTimeMs(),
			  (unsigned int)StdDevTimeMs());
	s += sf.c_str();
	return s;
}