// (c) Copyright 2024 Fatlab Software Pty Ltd.
//
// ICMP Ping library for the ESP32
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110 - 1301  USA
#pragma once

#include <Arduino.h>
#include <cstdint>

/// <summary>
/// The ICMP Ping Options
/// </summary>
class PingOptions
{
public:
	constexpr static uint8_t DEFAULT_COUNT = 4;
	constexpr static uint8_t MAX_COUNT = 10; // No more than 10 calls
	constexpr static uint16_t DEFAULT_RECV_TIMEOUT_MS = 1000;
	constexpr static uint16_t DEFAULT_TOTAL_TIMEOUT_MS = 0; // None - will be calculated
	constexpr static uint16_t FIXED_MESSAGE_BYTE_COUNT = 32;

private:
	String _host;			  // Host string - either this or _ip must be valid
	uint32_t _ip4;			  // Raw IP address We assume already in network order
	uint8_t _count;			  // How many times to ping the target address
	uint16_t _recvTimeoutMs;  // Socket receive tiemout per call
	uint16_t _totalTimeoutMs; // Drop out after this time even if not finished
private:
	/// @brief Calc timeout total from other fields
	/// @return
	uint32_t CalcTotalTimeoutMs() const { return Count() * ReceiveTimeoutMs(); }
	/// @brief
	/// @param ip4
	/// @param cnt
	/// @param recvTimeoutMs
	/// @param totalTimeoutMs
	explicit PingOptions(const uint32_t ip4,
						 const char *host,
						 const uint8_t cnt,
						 const uint16_t recvTimeoutMs,
						 const uint16_t totalTimeoutMs)
		: _ip4(ip4),
		  _host(host),
		  _count(cnt > MAX_COUNT ? MAX_COUNT : cnt),
		  _recvTimeoutMs(recvTimeoutMs > 0 ? recvTimeoutMs : DEFAULT_RECV_TIMEOUT_MS),
		  _totalTimeoutMs(totalTimeoutMs)
	{
	}

public:
	/// @brief
	/// @param ip4
	/// @param cnt
	/// @param recvTimeoutMs
	/// @param totalTimeoutMs
	explicit PingOptions(const uint32_t ip4,
						 const uint8_t cnt = DEFAULT_COUNT,
						 const uint16_t recvTimeoutMs = DEFAULT_RECV_TIMEOUT_MS,
						 const uint16_t totalTimeoutMs = DEFAULT_TOTAL_TIMEOUT_MS)
		: PingOptions(ip4, "", cnt, recvTimeoutMs, totalTimeoutMs)
	{
	}
	explicit PingOptions(const char *host,
						 const uint8_t cnt = DEFAULT_COUNT,
						 const uint16_t recvTimeoutMs = DEFAULT_RECV_TIMEOUT_MS,
						 const uint16_t totalTimeoutMs = DEFAULT_TOTAL_TIMEOUT_MS)
		: PingOptions(0u, host, cnt, recvTimeoutMs, totalTimeoutMs)
	{
	}

public:
	bool GetAddress(uint32_t &ip4, Print *printer = nullptr) const;
	uint8_t Count() const { return _count; }
	uint16_t ReceiveTimeoutMs() const { return _recvTimeoutMs; }

	uint16_t ReceiveTimeoutSeconds() const { return ReceiveTimeoutMs() / 1000; }
	long ReceiveTimeoutMicros() const { return ReceiveTimeoutMs() % 1000 * 1000; }
	uint32_t TotalTimeoutMs() const
	{
		return _totalTimeoutMs == 0 ? CalcTotalTimeoutMs() : _totalTimeoutMs;
	}

	/// @brief
	/// @return
	bool IsValid() const
	{
		return (_ip4 != 0u || _host.length() > 0) &&
			   Count() > 0u && Count() <= MAX_COUNT &&
			   ReceiveTimeoutMs() > 0u &&
			   (_totalTimeoutMs == 0u || _totalTimeoutMs >= CalcTotalTimeoutMs());
	}

	/// @brief
	/// @param
	void PrintState(Print *) const;
};

/// <summary>
/// The ICMP Ping Results
/// </summary>
class PingResults
{
private:
	uint8_t _transmitted_count;
	uint8_t _received_count;
	uint16_t _total_timeMs;
	float _min_timeMs;
	float _max_timeMs;
	float _avg_timeMs;
	float _sd_timeMs;

public:
	/// @brief
	explicit PingResults()
		: _transmitted_count(0u), _received_count(0u),
		  _total_timeMs(0u), _min_timeMs(0.0f), _max_timeMs(0.0f),
		  _avg_timeMs(0.0f), _sd_timeMs(0.0f) {}

public:
	uint16_t Transmitted() const { return _transmitted_count; }
	uint16_t Received() const { return _received_count; }
	uint16_t TimeoutCount() const // number of pings which failed
	{
		return Transmitted() > Received() ? Transmitted() - Received() : 0u;
	}

	/// @brief
	/// @return
	float PercentTransmitted() const
	{
		if (Transmitted() == 0 || !IsValid())
			return 0.0f;
		return static_cast<float>(TimeoutCount()) / Transmitted() * 100.0f;
	}
	uint16_t TotalTimeMs() const { return _total_timeMs; }
	float MinTimeMs() const { return _min_timeMs; }
	float MaxTimeMs() const { return _max_timeMs; }
	float AveTimeMs() const { return _avg_timeMs; }
	float StdDevTimeMs() const { return _sd_timeMs; }

	/// @brief
	/// @return
	bool IsValid() const
	{
		return Transmitted() >= Received() &&
			   TotalTimeMs() > 0u &&
			   MaxTimeMs() >= MinTimeMs() &&
			   AveTimeMs() > 0.0f;
	}

	/// @brief
	/// @param transmitted
	/// @param received
	/// @param totalMs
	/// @param minMs
	/// @param maxMs
	/// @param meanMs
	/// @param varMs
	void SetResults(
		const uint8_t transmitted, const uint8_t received,
		const uint16_t totalMs,
		const float minMs, const float maxMs,
		const float meanMs, const float sdMs)
	{
		_transmitted_count = transmitted; // Number of pings
		_received_count = received;		  // number of pings which succeeded
		_total_timeMs = totalMs;		  // Time consumed for all pings; it takes into account also timeout pings
		_min_timeMs = minMs;
		_max_timeMs = maxMs;
		_avg_timeMs = meanMs; // Average time for the pings
		_sd_timeMs = sdMs;
	}

	/// @brief
	/// @param
	void PrintState(Print *) const;

	/// @brief
	/// @return
	String ResultString(bool includeTimes = false) const;
};

/// <summary>
///
/// </summary>
class Esp32IcmpPing
{
private:
	PingOptions _pingOptions;
	Print *_printer;
	bool _inPing;

private:
	/// @brief Optional Message/Error handling
	/// @param str
	void OutputLn(const char *str)
	{
		if (_printer != nullptr)
			_printer->println(str);
	}
	bool ErrorLn(const char *str)
	{
		OutputLn(str);
		return false;
	}
	bool ErrorLn(const char *str, int errorNo);

	/// @brief
	/// @param sock_fd
	/// @return
	bool CreateAndSetUpSocket(int &sock_fd);

	/// @brief
	/// @param ip4
	/// @param sock_fd
	/// @param ping_seq_num
	/// @return
	bool Send(uint32_t ip4, int sock_fd, uint16_t ping_seq_num);

	/// @brief
	/// @param sock_fd
	/// @param ping_seq_num
	/// @param elapsed
	/// @return
	bool Receive(int sock_fd, uint16_t ping_seq_num, float &elapsed, bool &timedOut);

	/// @brief Do the Ping
	/// @param result
	/// @param printer
	/// @return
	bool CallPing(PingResults &result, Print *printer = nullptr);

public:
	/// @brief
	/// @param pingOptions
	/// @param printer
	explicit Esp32IcmpPing(const PingOptions &pingOptions, Print *printer = nullptr)
		: _pingOptions(pingOptions), _printer(printer), _inPing(false) {}

	/// @brief
	/// @param dest
	/// @param count
	/// @param recvTimeout
	/// @param totalTimeout
	/// @param timeBetweenPings
	explicit Esp32IcmpPing(const uint32_t ip4,
						   const uint8_t count = PingOptions::DEFAULT_COUNT,
						   const uint16_t recvTimeoutMs = PingOptions::DEFAULT_RECV_TIMEOUT_MS,
						   const uint16_t totalTimeoutMs = PingOptions::DEFAULT_TOTAL_TIMEOUT_MS)
		: Esp32IcmpPing(PingOptions(ip4, count, recvTimeoutMs, totalTimeoutMs)) {}

	/// @brief
	/// @param host
	/// @param count
	/// @param recvTimeout
	/// @param totalTimeout
	/// @param timeBetweenPings
	explicit Esp32IcmpPing(const char *host,
						   uint8_t count = PingOptions::DEFAULT_COUNT,
						   uint16_t recvTimeoutMs = PingOptions::DEFAULT_RECV_TIMEOUT_MS,
						   uint16_t totalTimeoutMs = PingOptions::DEFAULT_TOTAL_TIMEOUT_MS)
		: Esp32IcmpPing(PingOptions(host, count, recvTimeoutMs, totalTimeoutMs)) {}

public:
	const PingOptions &Options() const { return _pingOptions; }

	/// @brief Do the Ping
	/// @param result
	/// @param printer
	/// @return
	bool ping(PingResults &result, Print *printer = nullptr);
	bool ping(Print *printer = nullptr)
	{
		PingResults result;
		if (!ping(result, printer))
			return false;
		result.PrintState(printer);
		return true;
	}
};
