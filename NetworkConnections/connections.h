#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

class PortState
{
public:
	enum State
	{
		// Taken from http://msdn.microsoft.com/en-us/library/windows/desktop/bb485761%28v=vs.85%29.aspx
		MIB_TCP_STATE_CLOSED = 1,
		MIB_TCP_STATE_LISTEN = 2,
		MIB_TCP_STATE_SYN_SENT = 3,
		MIB_TCP_STATE_SYN_RCVD = 4,
		MIB_TCP_STATE_ESTAB = 5,
		MIB_TCP_STATE_FIN_WAIT1 = 6,
		MIB_TCP_STATE_FIN_WAIT2 = 7,
		MIB_TCP_STATE_CLOSE_WAIT = 8,
		MIB_TCP_STATE_CLOSING = 9,
		MIB_TCP_STATE_LAST_ACK = 10,
		MIB_TCP_STATE_TIME_WAIT = 11,
		MIB_TCP_STATE_DELETE_TCB = 12
	};

	unsigned long port;
	State state;

	std::string toString() const;
};

class ProgramConnection
{
public:

public:
	ProgramConnection() : pid(0) {}
	ProgramConnection(unsigned long pid);

	unsigned long pid;
	std::wstring filePath;
	std::vector<PortState> ports;

public:
	std::string toString() const;
};

class GeoAddress
{
public:
	unsigned long address;

	const char* city;
	const char* country;

	std::set<unsigned long> pids;

	bool operator < (const GeoAddress& other) const { return address < other.address; }

public:
	GeoAddress() : address(0), city(NULL), country(NULL) {}
	
	std::string getAddressString() const;
	std::string toString() const;
};

class GeoCoord
{
public:
	float longitude;
	float latitude;

public:
	GeoCoord() : longitude(1e-16), latitude(1e-16) {}

	bool isUndefined() const { return longitude < 1e-15; }
	virtual std::string toString() const;
};

class GeoConnection : public GeoCoord
{
public:
	std::set<GeoAddress> addresses;

public:
	GeoConnection() : GeoCoord() {}
	std::string toString() const;
};

class GeoConnections
{
public:
	std::vector<GeoConnection> connections;
	std::map<unsigned long, ProgramConnection> pidToProgram;

	GeoAddress localAddress;
	GeoCoord localCoord;

	GeoConnections(const GeoConnections& other)
		: connections(other.connections), pidToProgram(other.pidToProgram), localAddress(other.localAddress), localCoord(other.localCoord) {}

	GeoConnections() {}

	int update();
	int updateConnections();
	int updateLocal();
	std::string getDescription(const GeoConnection& to) const;
};

int InitConnections(const char* geoIpData);
void CloseConnections();

bool Tracert(const std::string& host, std::vector<std::string>& outIps, int maxHops = 30, int timeout = -1);


//===================================
// MIT License
//
// Copyright (c) 2014 by Gherman Alin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//===================================
