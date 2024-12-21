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

#include "Esp32ConnectionChecker.h"
#include "Esp32IcmpPing.h"
#include <WiFi.h>

bool Esp32ConnectionChecker::_connected = false;

/// @brief
/// @param arg
void Esp32ConnectionChecker::ConnectionCheckTask(void *arg)
{
	_connected = false;
	// Hard code Google DNS
	IPAddress google(8, 8, 4, 4);
	// Set recv timeout to 1/2 second and Count to 4
	Esp32IcmpPing pingClient(google, 4, 2000);
	PingResults results;
	for (;;)
	{
		if (pingClient.ping(results))
		{
			Serial.println("Connected");
			_connected = true;
		}
		else
		{
			Serial.println("Not Connected");
			_connected = false;
		}
		delay(10000);
	}
}