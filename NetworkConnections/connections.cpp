#include <map>
#include <cctype>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "connections.h"
#include "process.h"
#include "GeoIP.h"
#include "GeoIPCity.h"
#include "common.h"

// Need to link with Iphlpapi.lib and Ws2_32.lib
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
/* Note: could also use malloc() and free() */

extern "C" void InitPreadCls();


GeoIP *gi;

inline std::string ipToStr(unsigned long ip)
{
	struct in_addr IpAddr;
	char str[INET6_ADDRSTRLEN];

	IpAddr.S_un.S_addr = ip;
	inet_ntop(AF_INET, &(IpAddr), str, INET6_ADDRSTRLEN);
	return std::string(str);
}

inline unsigned long strToIP(const char* strIp)
{
	struct in_addr IpAddr;

	// store this IP address in sa:
	inet_pton(AF_INET, strIp, &(IpAddr));
	return IpAddr.S_un.S_addr;
}

inline std::string programToShort(std::wstring wFilePath)
{
	std::string filePath = ws2s(wFilePath);
	if (filePath.size() > 0)
	{
		size_t idx = filePath.find_last_of("\\");
		if (idx < 0)
		{
			filePath.find_last_of("/");
		}
		if (idx >= 0)
		{
			filePath = filePath.substr(idx + 1);
			idx = filePath.find_last_of('.');
			if (idx >= 0)
			{
				filePath = filePath.substr(0, idx);
			}
		}
	}
	return filePath;
}

std::string PortState::toString() const
{
	std::string str = "Port:";
	str.append(std::to_string(port));
	str.append("\nTCP State: ");
	switch (state)
	{
	case MIB_TCP_STATE_CLOSED:
		str.append("CLOSED");
		break;
	case MIB_TCP_STATE_LISTEN:
		str.append("LISTEN\n");
		break;
	case MIB_TCP_STATE_SYN_SENT:
		str.append("SYN-SENT");
		break;
	case MIB_TCP_STATE_SYN_RCVD:
		str.append("SYN-RECEIVED");
		break;
	case MIB_TCP_STATE_ESTAB:
		str.append("ESTABLISHED");
		break;
	case MIB_TCP_STATE_FIN_WAIT1:
		str.append("FIN-WAIT-1");
		break;
	case MIB_TCP_STATE_FIN_WAIT2:
		str.append("FIN-WAIT-2");
		break;
	case MIB_TCP_STATE_CLOSE_WAIT:
		str.append("CLOSE-WAIT");
		break;
	case MIB_TCP_STATE_CLOSING:
		str.append("CLOSING");
		break;
	case MIB_TCP_STATE_LAST_ACK:
		str.append("LAST-ACK");
		break;
	case MIB_TCP_STATE_TIME_WAIT:
		str.append("TIME-WAIT");
		break;
	case MIB_TCP_STATE_DELETE_TCB:
		str.append("DELETE-TCB");
		break;
	default:
		str.append("UNKNOWN STATE");
		break;
	}

	return str;
}

// ---- ProgramConnection ----
ProgramConnection::ProgramConnection(unsigned long pid)
{
	this->pid = pid;
	this->filePath = GetProcessFileName(pid);
}

std::string ProgramConnection::toString() const
{
	std::string str = ws2s(filePath);
	size_t n = ports.size();
	for (size_t i = 0; i < n; i++)
	{
		str.append("\n");
		str.append(ports[i].toString());			
	}
	return str;
}


// ---- GeoAddress ----
std::string GeoAddress::getAddressString() const
{
	return ipToStr(address);
}

std::string GeoAddress::toString() const
{
	std::string str;

	str.append("TCP Addr: ");
	str.append(getAddressString());

	if (country != NULL)
	{
		str.append("\nTCP Country: ");
		str.append(country);
	}
	if (city != NULL)
	{
		str.append("\nTCP City: ");
		str.append(city);
	}

	return str;
}

// ---- GeoCoord ----
std::string GeoCoord::toString() const
{
	std::string str;
	str.append("longitude: ");
	str.append(std::to_string(longitude));
	str.append("\nlatitude: ");
	str.append(std::to_string(latitude));
	return str;
}

// ---- GeoConnection ----
std::string GeoConnection::toString() const
{
	std::string str = GeoCoord::toString();
	for (auto it = addresses.begin(); it != addresses.end(); it++)
	{
		str.append("\n");
		str.append(it->toString());
	}
	return str;
}

int GeoConnections::updateConnections()
{
	connections.clear();
	pidToProgram.clear();

	// Declare and initialize variables
	PMIB_TCPTABLE2 pTcpTable;
	ULONG ulSize = 0;
	DWORD dwRetVal = 0;

	int i;

	pTcpTable = (MIB_TCPTABLE2 *)MALLOC(sizeof (MIB_TCPTABLE2));
	if (pTcpTable == NULL)
	{
		// Error allocating memory
		return 1;
	}

	ulSize = sizeof (MIB_TCPTABLE);
	// Make an initial call to GetTcpTable2 to
	// get the necessary size into the ulSize variable
	if ((dwRetVal = GetTcpTable2(pTcpTable, &ulSize, TRUE)) == ERROR_INSUFFICIENT_BUFFER)
	{
		FREE(pTcpTable);
		pTcpTable = (MIB_TCPTABLE2 *)MALLOC(ulSize);
		if (pTcpTable == NULL)
		{
			// Error allocating memory
			return 1;
		}
	}

	GeoCoord invalidCoords;
	std::map<std::pair<float, float>, GeoConnection> ipToConn;

	GeoConnection connectionInfo;
	// Make a second call to GetTcpTable2 to get
	// the actual data we require
	if ((dwRetVal = GetTcpTable2(pTcpTable, &ulSize, TRUE)) == NO_ERROR)
	{
		for (i = 0; i < (int)pTcpTable->dwNumEntries; i++)
		{
			PortState::State state = (PortState::State) pTcpTable->table[i].dwState;

			unsigned long localAddr = (u_long)pTcpTable->table[i].dwLocalAddr;
			unsigned long localPort = ntohs((u_short)pTcpTable->table[i].dwLocalPort);
			unsigned long remoteAddr = (u_long)pTcpTable->table[i].dwRemoteAddr;
			unsigned long remotePort = ntohs((u_short)pTcpTable->table[i].dwRemotePort);

			unsigned long owningPid = pTcpTable->table[i].dwOwningPid;
			float latitude = invalidCoords.latitude;
			float longitude = invalidCoords.longitude;
			GeoAddress addr;
			if (gi != NULL)
			{
				std::string ip = ipToStr(remoteAddr);
				GeoIPRecord *gir = GeoIP_record_by_addr(gi, ip.c_str());
				if (gir != NULL)
				{
					addr.country = gir->country_name;
					addr.city = gir->city;
					latitude = gir->latitude;
					longitude = gir->longitude;
					addr.pids.insert(owningPid);
				}
			}

			std::pair<float, float> coords(longitude, latitude);
			auto it = ipToConn.find(coords);
			if (it == ipToConn.end())
			{
				// ip not found, store it
				GeoConnection conn;
				conn.longitude = longitude;
				conn.latitude = latitude;

				ipToConn[coords] = conn;
				it = ipToConn.find(coords);
			}

			for (auto itAddr = it->second.addresses.begin(); itAddr != it->second.addresses.end(); itAddr++)
			{
				if (itAddr->address == remoteAddr)
				{
					addr = *itAddr;
					addr.pids.insert(owningPid);
					it->second.addresses.erase(itAddr);
					break;
				}
			}

			it->second.addresses.insert(addr);

			// ip found, retrive it
			auto programToIt = pidToProgram.find(owningPid);
			if (programToIt == pidToProgram.end())
			{
				// add the program
				ProgramConnection pc(owningPid);
				pidToProgram[owningPid] = pc;
				programToIt = pidToProgram.find(owningPid);
			}

			PortState ps;
			ps.state = state;
			ps.port = remotePort;
			programToIt->second.ports.push_back(ps);
		}
	}
	else
	{
		// "GetTcpTable2 failed with " + dwRetVal
		FREE(pTcpTable);
		return 2;
	}

	if (pTcpTable != NULL)
	{
		FREE(pTcpTable);
		pTcpTable = NULL;
	}

	// Populate the connections
	for (auto it = ipToConn.begin(); it != ipToConn.end(); it++)
	{
		connections.push_back(it->second);
	}

	// OK
	return 0;
}

int GeoConnections::updateLocal()
{
	std::vector<std::string> localTrace;
	Tracert("google.com", localTrace);
	for (size_t i = 0; i < localTrace.size() - 1; i++)
	{
		const char* ip = localTrace[i].c_str();
		localAddress.address = strToIP(ip);
		GeoIPRecord *gir = GeoIP_record_by_addr(gi, ip);
		if (gir != NULL)
		{
			localAddress.country = gir->country_name;
			localAddress.city = gir->city;
			localCoord.latitude = gir->latitude;
			localCoord.longitude = gir->longitude;
		}

		if (!localCoord.isUndefined())
		{
			return 0;
		}
	}
	return -1;
}

int GeoConnections::update()
{
	int r1 = updateConnections();
	int r2 = 0;

	if (localCoord.isUndefined())
	{
		r2 = updateLocal();
	}
	return r1 & r2;
}

std::string GeoConnections::getDescription(const GeoConnection& to) const
{
	std::string desc = std::to_string(to.longitude) + ", " + std::to_string(to.latitude);
	for (auto it = to.addresses.begin(); it != to.addresses.end(); it++)
	{
		desc.append("\n");
		desc.append(it->getAddressString());
		if (it->country != NULL)
		{
			desc.append("\n");
			desc.append(it->country);
		}
		if (it->city != NULL)
		{
			desc.append("\n");
			desc.append(it->city);
		}
		for (auto itPid = it->pids.begin(); itPid != it->pids.end(); itPid++)
		{
			auto itPro = pidToProgram.find(*itPid);
			if (itPro != pidToProgram.end())
			{
				std::string s = programToShort(itPro->second.filePath);
				desc.append("\n");
				if (s.length() > 0)
					desc.append(s);
				else
				{
					desc.append("PID: ");
					desc.append(std::to_string(itPro->second.pid));
				}
			}
		}
	}
	return desc;
}

class InitCs
{
public:
	InitCs()
	{
		InitPreadCls();
	}
};
InitCs _ginitCS;

void CloseConnections()
{
	if (gi != NULL)
	{
		GeoIP_delete(gi);
		gi = NULL;
	}
}

int InitConnections(const char* geoIpData)
{
	CloseConnections();
	gi = GeoIP_open(geoIpData, GEOIP_MEMORY_CACHE);
	if (gi == NULL)
		return -1;

	return 0;
}

bool Tracert(const std::string& host, std::vector<std::string>& outIps, int maxHops, int timeout)
{
	std::string cmd = "tracert -d";
	if (maxHops > 0 && maxHops < 100)
	{
		cmd.append(" -h ");
		cmd.append(std::to_string(maxHops));
	}
	if (timeout > 0 && timeout < 100000)
	{
		cmd.append(" -w ");
		cmd.append(std::to_string(timeout));
	}
	cmd.append(" ");
	cmd.append(host);

	FILE* pipe = _popen(cmd.c_str(), "r");
	if (pipe)
	{
		int lineIndex = 0;
		char buffer[1024];
		while (!feof(pipe))
		{
			if (fgets(buffer, 1024, pipe) != NULL)
			{
				// Extract the IP address if any
				if (lineIndex >= 4)
				{
					std::string line(buffer);
					std::string ip;
					size_t n = line.size();
					int ipStart = -1;
					int pointCnt = 0;
					for (size_t i = 0; i < n; i++)
					{
						if (ipStart >= 0)
						{
							// do we already have an IP ?
							if (line[i] == '.')
							{
								if (pointCnt < 3)
									pointCnt++;
								else
								{
									ipStart = -1;
									pointCnt = 0;
								}
							}
							else if (!std::isdigit(line[i]))
							{
								if (i - ipStart >= 7 && pointCnt == 3)
								{
									ip = line.substr(ipStart, i - ipStart);
									break;
								}
								else
								{
									pointCnt = 0;
									ipStart = -1;
								}
							}
						}
						else if (line[i] == ' ' && i < n - 1 && std::isdigit(line[i + 1]))
						{
							// might be an IP
							ipStart = i + 1;
						}
					}
					if (ip.length() > 0)
					{
						outIps.push_back(ip);
					}
				}

				lineIndex++;
			}
		}
		_pclose(pipe);
		return true;
	}
	return false;
}


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
