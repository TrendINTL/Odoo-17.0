// Bitvise SSH Server Status utility
// Copyright (C) 2011-2024 by Bitvise Limited
//
// This utility serves both:
//
// - to allow interactive and automated processes to use it from a command
//   line to enumerate users currently logged into Bitvise SSH Server, and
//   investigate their activity;
//
//   and
//
// - to serve as an example of how to communicate with Bitvise SSH Server,
//   using the Bitvise SSH Server Remote Control (BSSRC) protocol, from a 
//   custom program.
//
// The BSSRC protocol is used by the local Bitvise SSH Control Panel:
//   - to populate the Activity tab;
//   - to populate and manage the Connections tab, including blocked IPs.
//
// Additionally, this protocol is used by the Remote Bitvise SSH Control
// Panel, which is included with Bitvise SSH Client, to populate and remotely
// manage the Server tab, including host keys, settings, and password cache.
//
// In principle, BssStat can be extended to provide the same functionality
// as the Bitvise SSH Control Panels do.
//
// The BSSRC protocol is supported by Bitvise SSH Server (formerly known as
// WinSSHD) 5.06 or newer. The server must be running for this program to work.
//
// The BSSRC protocol is explicitly made public by the publishing of this
// program's source code. In future versions, we expect to change the
// interface in ways which retain backwards compatibility.
//
// This program is intended to be as self-contained as possible, so that users
// can literally lift the code and use it in their C++ programs without
// modification.
//
// A license is hereby granted free of charge for anyone to use, modify, and
// integrate this code for the purposes of interfacing with Bitvise SSH Server
// to extract information, or to control it in an automated manner. This 
// license does not however grant a right to use Bitvise SSH Server; this must
// be obtained separately.
//
// Your application will need to run as an administrator or as Local System in
// order to communicate with Bitvise SSH Server.
//
// This program will compile without changes using Visual Studio 2005.


// Disable off-by-default warnings
#pragma warning (disable: 4668)  // L4: 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning (disable: 4710)  // L4: 'function' : function not inlined
#pragma warning (disable: 4820)  // L4: 'bytes' bytes padding added after construct 'member_name'


// Windows
#include <windows.h>

// STL
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>

// CRT
#include <limits.h>


using namespace std;


typedef unsigned short word16;
typedef unsigned int word32;
typedef unsigned __int64 word64;
typedef unsigned char byte;

typedef basic_string<byte> bytes;



// Exceptions

class StrException : public exception
{
public:
	StrException(char const* z) : m_what(z) {}
	StrException(string const& s) : m_what(s) {}
	
	char const* what() const { return m_what.c_str(); }

protected:
	StrException() = delete;
	string m_what;
};


class UsageErr : public StrException
{
public:
	UsageErr(char const* what) : StrException(what) {}
	UsageErr() : StrException(
		"Bitvise SSH Server status utility\n"
		"Copyright (C) 2011-2024 by Bitvise Limited\n"
		"\n"
		"Usage: BssStat (-s | -d <ConnectionID> | -i | -u <BlockedIP[/SigBits]> | -r | -v)\n"
		"       [-n <InstanceName>]\n"
		"\n"
		"-s   Display connections with channels\n"
		"-d   Disconnect connection by its ID\n"
		"-i   Display temporarily blocked IPs\n"
		"-u   Removes a temporary block on the specified IP or subnet\n"
		"-r   Force log rollover (requires Bitvise SSH Server 8.xx or newer)\n"
		"-v   Displays SSH Server version\n"
		"-n   Specify a different BvSshServer instance name (default: where BssStat is installed)\n"
		"\n"
		"Examples: \n"
		"  BssStat -s\n"
		"    Displays connections with channels for default instance.\n"
		"\n"
		"  BssStat -d 1001\n"
		"    Disconnects connection with ID 1001 for default instance.\n"
		"\n"
		"  BssStat -i -n BvSshServer-XY\n"
		"    Displays blocked IPs for instance 'BvSshServer-XY'.\n"
		"\n"
		"  BssStat -u 10.10.10.0/24\n"
		"    Removes a temporary block on IP addresses starting with 10.10.10.*") {}
};


class ApiErr : public StrException
{
public:
	ApiErr(char const* description, char const* funcName, DWORD errCode = GetLastError()) : StrException("")
	{ 
		ostringstream os;
		os << description << " " << funcName << " failed with error " << errCode << ".";
		m_what = os.str();
	}
};

class DecodeErr : public StrException
{
public:
	DecodeErr() : StrException("Decoding packet failed. Premature end of data") {}
};


class BvSshServerErr : public StrException
{
public:
	BvSshServerErr(char const* errorDesc) : StrException("Operation failed. ") { m_what += errorDesc; }
};



// AutoHandle 

class AutoHandle
{
public:
	AutoHandle() : m_handle(0) {}
	AutoHandle(HANDLE handle) : m_handle(handle) {}
	virtual ~AutoHandle() { Close(); }
	
	void Set(HANDLE h) { Close(); m_handle = h; }
	void Close() { if (Valid())	{ CloseHandle(m_handle); m_handle = 0; } }

	bool Valid() const { return m_handle != 0 && m_handle != INVALID_HANDLE_VALUE; }
	HANDLE Get() const { return m_handle; }
	operator HANDLE() const { return m_handle; }

private:
	AutoHandle(AutoHandle const&);
	void operator= (AutoHandle const& x);

	HANDLE m_handle;
};



// String manipulation

wstring W2L(wstring s) // wide to lower
{
	for (wstring::iterator it = s.begin(); it != s.end(); ++it)
		if (*it >= L'A' && *it <= L'Z')
			*it += L'a' - L'A';
	return s;
}

wstring N2W(string const& narrow, unsigned int codePage) // narrow to wide
{
	wstring wide;

	if (narrow.size())
	{
		DWORD flags;
		if (codePage == CP_UTF8 || codePage == CP_UTF7)
			flags = 0;
		else
			flags = MB_PRECOMPOSED | MB_ERR_INVALID_CHARS;

		if (narrow.size() > INT_MAX)
			throw StrException("Text conversion failed: Narrow string is too big.");

		int narrowSize = (int) narrow.size();
		
		int c = MultiByteToWideChar(codePage, flags, narrow.data(), narrowSize , 0, 0);
		if (!c)
		{
			if (flags && GetLastError() == ERROR_INVALID_FLAGS)
				c = MultiByteToWideChar(codePage, flags = 0, narrow.data(), narrowSize, 0, 0);
			
			if (!c)	
				throw ApiErr("Text conversion failed", "MultiByteToWideChar()");
		}

		wide.resize((size_t)c);

		c = MultiByteToWideChar(codePage, flags, narrow.data(), narrowSize, &wide[0], c);
		if (!c)
			throw ApiErr("Text conversion failed", "MultiByteToWideChar()");

		wide.resize((size_t)c);
	}

	return wide;
}

string W2N(wstring const& wide, unsigned int codePage) // wide to narrow
{
	string narrow;

	if (wide.size())
	{
		if (narrow.size() > INT_MAX)
			throw StrException("Text conversion failed: Wide string is too big.");

		int wideSize = (int) wide.size();

		int c = WideCharToMultiByte(codePage, 0, wide.data(), wideSize, 0, 0, 0, 0);
		if (!c)
			throw ApiErr("Text conversion failed", "WideCharToMultiByte()");

		narrow.resize((size_t)c);

		c = WideCharToMultiByte(codePage, 0, wide.data(), wideSize, &narrow[0], c, 0, 0);
		if (!c)
			throw ApiErr("Text conversion failed", "WideCharToMultiByte()");

		narrow.resize((size_t)c);
	}

	return narrow;
}



// Data structures

enum AddressType
{
	AT_IP4				= 1,
	AT_IP6				= 2,
	AT_DNSNAME			= 3,
};

enum ChannelType
{
	CT_SESSION			= 0,
	CT_CLTSIDE_C2S		= 1,
	CT_CLTSIDE_S2C		= 2,
	CT_BV_SRVSIDE_C2S	= 3,
	CT_BV_SRVSIDE_S2C	= 4,
	CT_BV_BSSRC			= 5,
	CT_BV_CONF_SYNC		= 6,
	CT_AUTH_AGENT		= 7,
};

enum AccountType
{ 
	AT_WINDOWS	= 1,
	AT_VIRTUAL	= 2,
	AT_BSSACCT  = 3
};

enum StartupType
{
	ST_AUTOMATIC	= 1,
	ST_MANUAL		= 2,
	ST_DISABLED		= 3,
	ST_UNKNOWN		= 4
};

enum ObfsStatus
{
	OS_OK					= 0,
	OS_BAD_OBFS_KEYWORD		= 1,
	OS_OBFS_PROTOCOL_ERROR	= 2,
	OS_NO_OBFS_DETECTED		= 3
};

enum BssRcPackets
{
	// Basic types
	//
	// byte      Encoded as a single byte.
	//
	// bool      Encoded as a single byte, zero if false, non-zero if true.
	// 
	// word16    Encoded as 2 bytes, network order (most significant byte first).
	//
	// word32    Encoded as 4 bytes, network order (most significant byte first).
	//
	// word64    Encoded as 8 bytes, network order (most significant byte first).
	//           When encoding time, represented as Windows FILETIME.
	//
	// string    Encoded as word32 representing length, followed by [length] bytes.
	//
	// utf8str   A string, with contents interpreted as UTF-8.
	//
	// address:
	//   byte    addressType
	//   if addressType == AT_IP4:
	//     byte[4]   ipv4Address
	//   else if addressType == AT_IP6:
	//     byte[16]  ipv6Address
	//     word32    scopeId
	//   else if addressType == AT_DNSNAME:
	//     string    dnsName
	//   word16  port


	//
	// BSSRC request messages
	//
	// Most request types consist of the packet type byte only.
	//
	
	BSSRC_INIT							=   0,
	BSSRC_STOP_SERVER					=   1,
	BSSRC_RESTART_SERVER				=   2,
	BSSRC_QUERY_ACTIVATION				=   3,
	
	BSSRC_SET_ACTIVATION_CODE			=   4,
	
		// byte    BSSRC_SET_ACTIVATION_CODE
		// utf8str activationCodeHex

	BSSRC_SUBSCRIBE_CONNECTIONS			=   5,
	BSSRC_UNSUBSCRIBE_CONNECTIONS		=   6,

	BSSRC_DISCONNECT_CONNECTIONS		=   7,

		// byte    BSSRC_DISCONNECT_CONNECTIONS
		// word32  nrConnections
		// Repeat nrConnections times:
		//   word64   connectionId

	BSSRC_LIST_CHANNELS					=   8,
	BSSRC_LIST_CHANNELS_DIFF			=   9,
	BSSRC_QUERY_BLOCKED_IP_COUNT		=  10,
	BSSRC_LIST_BLOCKED_IPS				=  11,
	BSSRC_LIST_BLOCKED_IPS_DIFF			=  12,
	
	BSSRC_ADD_BLOCKED_IP				=  13,
	
		// byte    BSSRC_ADD_BLOCKED_IP
		// address ipAddress
		// word64  blockDuration
		// utf8str comment
		// bool    permanently
		// word32  subnetBits (for ipAddresss)	>= 8.11

	BSSRC_REMOVE_BLOCKED_IPS			=  14,
	
		// byte    BSSRC_REMOVE_BLOCKED_IPS
		// word32  nrBlockedIps
		// Repeat nrBlockedIps times:
		//   address ipAddress
		// Repeat nrBlockedIps times:
		//   word32 subnetBits (for ipAddress)	>= 8.11
	
	BSSRC_QUERY_CACHED_PWD_COUNT		=  15,
	BSSRC_LIST_CACHED_PWDS				=  16,
	BSSRC_LIST_CACHED_PWDS_DIFF			=  17,

	BSSRC_ADD_CACHED_PWD				=  18,
	
		// byte    BSSRC_ADD_CACHED_PWD
		// utf8str userName
		// utf8str password
		// bool    hideUserName

	BSSRC_REMOVE_CACHED_PWDS			=  19,
	
		// byte    BSSRC_REMOVE_CACHED_PWDS
		// word32  nrCachedPwds
		// Repeat nrCachedPwds times:
		//   utf8str userName
	
	BSSRC_HIDE_CACHED_PWDS				=  20,
	
		// byte    BSSRC_HIDE_CACHED_PWDS
		// word32  nrCachedPwds
		// Repeat nrCachedPwds times:
		//   utf8str userName
	
	BSSRC_QUERY_EMPLOYED_KEYS			=  21,
	BSSRC_LOCK_KEYPAIRS					=  22,
	BSSRC_LIST_KEYPAIRS					=  23,

	BSSRC_ADD_KEYPAIR					=  24,
	
		// byte    BSSRC_ADD_KEYPAIR
		// bool    autoEmploy
		// string  keypair
		//
		// Keypair format:
		//
		// byte    version
		// utf8str comment
		// string  publicKey
		// string  privateKey
		// bool    passphraseProtected
		// byte    extendedVersion
		// word64  dateTime
		// byte[?] extended
	
	BSSRC_REMOVE_KEYPAIRS				=  25,
	
		// byte    BSSRC_REMOVE_KEYPAIRS
		// word32  nrKeypairs
		// Repeat nrKeypairs times:
		//   word32  removeKeypairIndex
	
	BSSRC_EMPLOY_KEYPAIR				=  26,
	
		// byte    BSSRC_EMPLOY_KEYPAIR
		// word32  employKeypairIndex
	
	BSSRC_DISMISS_KEYPAIR				=  27,

		// byte    BSSRC_EMPLOY_KEYPAIR
		// word32  dismissKeypairIndex

	BSSRC_SET_KEYPAIR_COMMENT			=  28,
	
		// byte    BSSRC_SET_KEYPAIR_COMMENT
		// word32  keypairIndex
		// utf8str comment	
	
	BSSRC_UNLOCK_KEYPAIRS				=  29,
	BSSRC_LOCK_SETTINGS					=  30,
	BSSRC_GET_SETTINGS					=  31,

	BSSRC_SET_SETTINGS					=  32,
	
		// Beyond the scope of this document to describe.
	
	BSSRC_UNLOCK_SETTINGS				=  33,


	// Bitvise SSH Server versions >= 5.22

	BSSRC_QUERY_STARTUP_TYPE			=  34,

	BSSRC_SET_STARTUP_TYPE				=  35,
	
		// byte    BSSRC_SET_STARTUP_TYPE
		// byte    startupType


	// Bitvise SSH Server versions >= 5.23

	BSSRC_REMOVE_ALL_HIDDEN_PWDS		=  36,
	BSSRC_RESET_SETTINGS				=  37,


	// Bitvise SSH Server versions >= 6.00

	BSSRC_QUERY_INSTANCE_TYPE			=  38,
	BSSRC_SET_INSTANCE_TYPE_SETTINGS	=  39,
	BSSRC_GET_INSTANCE_TYPE_SETTINGS	=  40,

		// Beyond the scope of this document to describe.


	// Bitvise SSH Server >= 7.12

	BSSRC_LOCK_INSTANCE_TYPE_SETTINGS	=  50,
	BSSRC_UNLOCK_INSTANCE_TYPE_SETTINGS =  51,
	BSSRC_IMPORT_SETTINGS				=  52,
	BSSRC_MODIFY_BLOCKED_IP				=  53,

	// Bitvise SSH Server >= 7.21

	BSSRC_SUBSCRIBE_DELEGATED_SETTINGS		= 54,
	BSSRC_UNSUBSCRIBE_DELEGATED_SETTINGS	= 55,
	BSSRC_ADD_VIRTUAL_ACCOUNT				= 56,
	BSSRC_EDIT_VIRTUAL_ACCOUNT				= 57,
	BSSRC_REMOVE_VIRTUAL_ACCOUNT			= 58,

		// Beyond the scope of this document to describe.

	BSSRC_QUERY_SIGNATURE_ALGS				= 59,
	BSSRC_START_FOLLOWER_CONNECTION_NOW		= 60,

	// Bitvise SSH Server >= 8.11

	BSSRC_QUERY_EMPLOYED_CERTIFICATE		= 61,
	BSSRC_LOCK_CERTIFICATES					= 62,
	BSSRC_LIST_CERTIFICATES					= 63,

	BSSRC_ADD_CERTIFICATE					= 64,
	
		// byte    BSSRC_ADD_CERTIFICATE
		// bool    autoEmploy
		// string  certificate
		//
		// Certificate format:
		//
		// byte    version
		// byte    extendedVersion
		// word64  dateTime
		// utf8str comment
		// string  pfxData
		// byte[?] extended

	BSSRC_REMOVE_CERTIFICATES				= 65,
	
		// byte    BSSRC_REMOVE_CERTIFICATES
		// word32  nrCertificates
		// Repeat nrCertificates times:
		//   word32  removeCertificateIndex
	
	BSSRC_EMPLOY_CERTIFICATE				= 66,
	
		// byte    BSSRC_EMPLOY_CERTIFICATE
		// word32  employCertificateIndex
	
	BSSRC_DISMISS_CERTIFICATE				= 67,

		// byte    BSSRC_DISMISS_CERTIFICATE
		// word32  dismissCertificateIndex

	BSSRC_SET_CERTIFICATE_COMMENT			= 68,
	
		// byte    BSSRC_SET_CERTIFICATE_COMMENT
		// word32  certificateIndex
		// utf8str comment	
	
	BSSRC_IMPORT_CERTIFICATE_CONTEXT		= 69,
	
		// byte    BSSRC_IMPORT_CERTIFICATE_CONTEXT
		// string  data	

	BSSRC_SET_CERTIFICATE_NAME				= 70,
	
		// byte    BSSRC_SET_CERTIFICATE_NAME
		// word32  certificateIndex
		// utf8str commonName
		// utf8str subjectAltNames	>= 9.12
	
	BSSRC_UNLOCK_CERTIFICATES				= 71,
	BSSRC_CHECK_FOR_UPDATES					= 72,
	BSSRC_DOWNLOAD_AND_START_UPDATE			= 73,

		// byte    BSSRC_DOWNLOAD_AND_START_UPDATE
		// utf8str version

	BSSRC_MODIFY_MULTIPLE_BLOCKED_IPS		= 74,

		// byte    BSSRC_MODIFY_MULTIPLE_BLOCKED_IPS
		// word32  nrOfEntries
		// Repeat nrOfEntries times:
		//   address ipAddress
		//   word32 subnetBits (for ipAddress)
		// bool	   modifyDuration
		// if modifyDuration == true
		//    bool    permanently
		//       if permanently == false
		//          word64  blockDuration
		// bool    modifyComment
		// if modifyComment == true
		//    utf8str comment
		

	BSSRC_FORCE_LOG_ROLLOVER				= 75,

	BSSRC_QUERY_TOTAL_LOGGED_EVENTS			= 76,
	BSSRC_RESET_TOTAL_LOGGED_EVENTS			= 77,

	BSSRC_QUERY_IPBLOCK_SUBNET_BITS			= 78,

	// Bitvise SSH Server >= 9.12

	BSSRC_LIST_QUEUED_EMAILS				= 79,
	BSSRC_CLEANUP_MESSAGE_QUEUE				= 80,
	BSSRC_SEND_TEST_EMAIL					= 81,

		// byte    BSSRC_SEND_TEST_EMAIL
		// utf8str toAddress

	BSSRC_START_EMAIL_SERVICE				= 82,
	BSSRC_STOP_EMAIL_SERVICE				= 83,

	BSSRC_DELETE_EMAILS						= 84,

		// byte    BSSRC_DELETE_EMAILS
		// word32  nrOfEntries
		// Repeat nrOfEntries times:
		//   word64 index of ObjId
		//   word64 unique id of ObjId

	BSSRC_SEND_EMAILS_NOW					= 85,

		// byte    BSSRC_SEND_EMAILS_NOW
		// word32  nrOfEntries
		// Repeat nrOfEntries times:
		//   word64 index of ObjId
		//   word64 unique id of ObjId

	BSSRC_LIST_WIN_SESSIONS					= 86,
	BSSRC_MARK_ALL_WIN_SESSIONS_STALE		= 87,
	BSSRC_MARK_WIN_SESSIONS_STALE			= 88,

		// byte		BSSRC_MARK_WIN_SESSIONS_STALE
		// word32	nrOfEntries
		// Repeat nrOfEntries times:
		//   byte	service type  ( Ssh = 0, Ftp = 1, BssTask = 2 )
		//   word64	winSessionId

	BSSRC_QUERY_WIN_SESSIONS_COUNT			= 89,
	BSSRC_QUERY_VIRT_USERS_ACCT_INFO		= 90,


	//
	// BSSRC response messages
	//
	
	BSSRC_VERSION							= 100,

		// byte    BSSRC_VERSION
		// utf8str serverVersion
		// utf8str serverVersionExtendedInfo
		// utf8str lastCfgFormatChangeVersion
		// utf8str instanceName
		// word32  windowsPlatformId
		// word32  windowsMinorVersion
		// word32  windowsMajorVersion 
		// bool    bvLsaOperational
		// if bvLsaOperational is false
		//   word32  bvLsaNotOperationalReasonMsgId	  >= 6.41
		//   utf8str bvLsaNotOperationalReasonMsgText >= 6.41
		// word32  windowsServicePack >= 6.41
		// bool    isWinDomainController >= 7.21
		// --- optional data following ---
		// bool    isInstDirInsecure >= 8.42
		// if isInstDirInsecure is true
		//   word32  instDirWarningMsgId >= 8.42
		//   utf8str instDirWarningMsgText >= 8.42
		//   utf8str instDirWarningMsgMoreDetails >= 8.42

	BSSRC_STATUS							= 101,
	
		// byte    BSSRC_STATUS
		// bool    errorOccurred
		// if errorOccurred is true:
		//   utf8str errorDescription
		
	BSSRC_ACTIVATION_STATUS					= 102,
	
		// byte    BSSRC_ACTIVATION_STATUS
		// bool    evaluation
		// if evaluation:
		//   bool    clockProblematic
		//   bool    expired
		//   bool    nearingExpiration
		//   word32  daysRemaining
		// else:
		//   byte    licenseType
		//   utf8str licensedTo
		//   word64  upgradeExpiry
		//   utf8str seller
		//   word32  ordernr
	
	BSSRC_ACTIVATION						= 103,
	
		// byte    BSSRC_ACTIVATION
		// byte    activationResult
		// word32  orderNumber
	
	BSSRC_CONNECTIONS						= 104,
	
		// byte    BSSRC_CONNECTIONS
		// word64  packetTime
		// word32  nrConnections
		// repeat nrConnections times:
		//   word64  connectionId
		//   address remoteAddress
		//   word64  acceptTime
		//   byte    obfsStatus          >= 6.22; changed from 'bool obfsFailure' to 'byte obfsStatus' in 7.21; enum ObfsStatus
		//   utf8str clientVersionString
		//   byte    accountType         enum AccountType
		//   utf8str accountName
		// repeat nrConnections times:   >= 7.12; extension 1
		//   byte    serviceType         0=SSH, 1=FTP
		//   bool    kexMismatch         
		// repeat nrConnections times:   >= 8.11; extension 2
		//   byte    fromMonitorIp       0=Unknown, 1=No, 2=Yes
	
	BSSRC_CHANNELS							= 105,
	
		// byte    BSSRC_CHANNELS
		// word32  nrConnections
		// repeat nrConnections times:
		//   word64  connectionId
		//   word32  nrChannels
		//   repeat nrChannels times:
		//     byte    channelType
		//     word32  channelNumber
		//     word64  openTime
		//     word64  bytesSent
		//     word64  bytesReceived
		//     if channelType == CT_SESSION:
		//       utf8str channelInfo
		//     if channelType is one of CT_CLTSIDE_C2S, CT_CLTSIDE_S2C, CT_BV_SRVSIDE_C2S, CT_BV_SRVSIDE_S2C:
		//       address serverForwardingAddress
		//       address serverCorrespondentAddress
		//       if channelType != CT_CLTSIDE_S2C:
		//         address clientCorrespondentAddress
	
	BSSRC_CHANNELS_DIFF						= 106,
	
		// Not used.
	
	BSSRC_BLOCKED_IP_COUNT					= 107,
	
		// byte    BSSRC_BLOCKED_IP_COUNT
		// word32  nrBlockedIps

	// Note: Simce version 8.11, 'nrBlockedIps' refers to the number of entries in blocked IPs list, which is not neceserrily the same as number of blocked addresses.
	
	BSSRC_BLOCKED_IPS						= 108,
	
		// byte    BSSRC_BLOCKED_IPS
		// word32  nrBlockedIps
		// repeat nrBlockedIps times:
		//   address ipAddress
		//   word64  entryBlockedTime
		//   word64  entryBlockedDuration
		//   utf8str entryComment
		// Repeat nrBlockedIps times:
		//   word32 subnetBits (for ipAddress)	>= 8.11
	
	BSSRC_BLOCKED_IPS_DIFF					= 109,
	
		// Not used.
	
	BSSRC_CACHED_PWD_COUNT					= 110,
	
		// byte    BSSRC_CACHED_PWD_COUNT
		// word32  nrHiddenEntries
		// word32  nrRevealedEntries
	
	BSSRC_CACHED_PWDS						= 111,
	
		// byte    BSSRC_CACHED_PWDS
		// word32  nrHiddenEntries
		// word32  nrRevealedEntries
		// repeat nrRevealedEntries times:
		//   utf8str revealedEntry
	
	BSSRC_CACHED_PWDS_DIFF					= 112,
	
		// Not used.
	
	BSSRC_EMPLOYED_KEYS						= 113,
	
		// byte    BSSRC_EMPLOYED_KEYS
		// word32  nrEmployedKeys
		// repeat nrEmployedKeys times:
		//   string  employedKeypair
		//
		// See BSSRC_ADD_KEYPAIR for keypair format.

	BSSRC_EMPLOYED_KEYS_UNCHANGED			= 114,

		// byte    BSSRC_EMPLOYED_KEYS_UNCHANGED
			
	BSSRC_KEYPAIRS							= 115,
	
		// byte    BSSRC_KEYPAIRS
		// word32  maxEmployedKeys
		// word32  employedDsaKey
		// word32  employedRsaKey
		// word32  employedEcdsaSecp256k1Key
		// word32  employedEcdsaNistp256Key
		// word32  employedEcdsaNistp384Key
		// word32  employedEcdsaNistp521Key
		// word32  employedEd25519Key
		// word32  nrKeypairs
		// Repeat nrKeypairs times:
		//   string  keypair
		//
		// See BSSRC_ADD_KEYPAIR for keypair format.
	
	BSSRC_SETTINGS							= 116,
	
		// Beyond the scope of this document to describe.


	// Bitvise SSH Server versions >= 5.22
		
	BSSRC_STARTUP_TYPE						= 117,
	
		// byte    BSSRC_STARTUP_TYPE
		// byte    startupType
	

	// Bitvise SSH Server versions >= 6.00

	BSSRC_INSTANCE_TYPE						= 118,

		// Beyond the scope of this document to describe.
		
	BSSRC_INSTANCE_TYPE_SETTINGS			= 119,
	

	// Bitvise SSH Server versions >= 7.21

	BSSRC_SUBSCRIBE_DELEGATED_SETTINGS_REPLY = 120,
	BSSRC_MODIFY_DELEGATED_SETTINGS_REPLY	= 121,

		// Beyond the scope of this document to describe.

	BSSRC_SIGNATURE_ALGS					= 122,

		// Beyond the scope of this document to describe.


	BSSRC_EMPLOYED_CERTIFICATE				= 123,

	// byte    BSSRC_EMPLOYED_CERTIFICATE
	// string  employedCertificate
	//
	// See BSSRC_ADD_CERTIFICATE for keypair format.

	BSSRC_EMPLOYED_CERTIFICATE_UNCHANGED	= 124,

	// byte    BSSRC_EMPLOYED_CERTIFICATE_UNCHANGED

	BSSRC_CERTIFICATES						= 125,

	// byte    BSSRC_CERTIFICATES
	// word32  employedCertificate
	// word32  nrCertificates
	// Repeat nrCertificates times:
	//   string  certificate
	//
	// See BSSRC_ADD_CERTIFICATE for keypair format.

	BSSRC_TOTAL_LOGGED_EVENTS				= 126,

	// word64  nrLogFileErrors
	// word64  nrLogFileWarnings
	// word64  nrEventLogErrors
	// word64  nrEventLogWarnings
	// word64  lastTotalsCounterReset (datetime)

	BSSRC_IPBLOCK_SUBNET_BITS				= 127,

	// word32  subnetBitsIPv4
	// word32  subnetBitsIPv6

	// Bitvise SSH Server >= 9.12
	BSSRC_QUEUED_EMAILS_LIST				= 128,

		// Beyond the scope of this document to describe.

	BSSRC_DELETE_EMAILS_RESULT				= 129,

		// byte    BSSRC_DELETE_EMAILS_RESULT
		// word32  nrDeleted
		// bool    nrFailed
		// if nrFailed > 0:
		//   utf8str    firstErrorMessage;

	BSSRC_WIN_SESSIONS						= 130,
	BSSRC_WIN_SESSIONS_UNCHANGED			= 131,

		// Beyond the scope of this document to describe.

	BSSRC_WIN_SESSIONS_COUNT				= 132,

		// word64 nrWindowsSessions

	BRRRC_VIRTUSERS_ACCT_INFO				= 133,

		// byte		BRRRC_VIRTUSERS_ACCT_INFO
		// byte		accountStatus (NotYetCreated = 0, CannotCreateOnDC = 1, Managing = 2)
		// if accountStatus == Managing:
		//   utf8str accountName



	//
	// BSSRC messages pushed by the server
	//
	
	BSSRC_CONNECTION_REJECT					= 200,
	
		// byte    BSSRC_CONNECTION_REJECT
		// word64  packetTime
		// word64  connectionId
		// address remoteAddress
		// utf8str rejectReason
		// byte    serviceType         >= 7.12; 0=SSH, 1=FTP
	
	BSSRC_CONNECTION_ACCEPT					= 201,
	
		// byte    BSSRC_CONNECTION_ACCEPT
		// word64  packetTime
		// word64  connectionId
		// address remoteAddress
		// byte    serviceType         >= 7.12; 0=SSH, 1=FTP
		// byte    fromMonitorIp       >= 8.11; 0=Unknown, 1=No, 2=Yes
	
	BSSRC_CLIENT_VERSION					= 202,
	
		// byte    BSSRC_CLIENT_VERSION
		// word64  packetTime
		// word64  connectionId
		// utf8str clientVersionString
	
	BSSRC_AUTH_FAILURE						= 203,
	
		// byte    BSSRC_AUTH_FAILURE
		// word64  packetTime
		// word64  connectionId
		// utf8str userName
		// utf8str methodDescription
		// bool    partialSuccess
		// utf8str failureDescription
	
	BSSRC_AUTH_SUCCESS						= 204,
	
		// byte    BSSRC_AUTH_SUCCESS
		// word64  packetTime
		// word64  connectionId
		// utf8str userName
		// utf8str methodDescription
		// byte    accountType         enum AccountType
		// utf8str accountName
	
	BSSRC_FILE_TRANSFER						= 205,
	
		// byte    BSSRC_FILE_TRANSFER
		// word64  packetTime
		// word64  connectionId
		// utf8str path
		// word64  openTime
		// word64  bytesRead
		// word64  bytesWritten
		// word64  bytesAppended
		// word64  readRangeBeginning
		// word64  readRangeEnd
		// word64  writeRangeBeginning
		// word64  writeRangeEnd
		// bool    createdNewFile      >= 6.41
		// bool    resizedFile         >= 6.41
		// byte    endedBy             >= 6.41; 0=Unknown, 1=Client, 2=Cleanup
		// word64  startSize           >= 8.11; set to -1 (max size) if not available
		// word64  finalSize           >= 8.11; set to -1 (max size) if not available
	
	BSSRC_CONNECTION_TERM					= 206,
	
		// byte    BSSRC_CONNECTION_TERM
		// word64  packetTime
		// word64  connectionId
		// byte    fromMonitorIp       >= 8.11; 0=Unknown, 1=No, 2=Yes
		// utf8str disconnetReason     >= 9.12
	
	BSSRC_CONNECTION_OVERFLOW				= 207,
	
		// byte    BSSRC_CONNECTION_OVERFLOW
		// word64  packetTime


	// Bitvise SSH Server versions >= 5.50

	BSSRC_AUTH_DISCARD						= 208,
	
		// byte    BSSRC_AUTH_DISCARD
		// word64  packetTime
		// word64  connectionId
		// utf8str userName
		// utf8str methodDescription
		// utf8str discardDescription


	// Bitvise SSH Server versions >= 6.00

	BSSRC_FOLLOWER_CONNECT_SUCCESS			= 209,
	BSSRC_FOLLOWER_CONNECTION_FAIL			= 210,
	BSSRC_FOLLOWER_CONNECTION_DISCONNECT	= 211,
	BSSRC_FOLLOWER_REQUEST_FAIL				= 212,
	BSSRC_FOLLOWER_RESP_PROCESSING_FAIL		= 213,
	BSSRC_FOLLOWER_CONFIG_FILE_LOCKED		= 214,
	BSSRC_FOLLOWER_SETTINGS_VERS_MISMATCH	= 215,
	BSSRC_FOLLOWER_CFG_PART_SYNC_DISABLED	= 216,
	BSSRC_FOLLOWER_CFG_PART_SYNC_SUCCESS	= 217,

		// Beyond the scope of this document to describe.


	// Bitvise SSH Server versions >= 6.05

	BSSRC_FOLLOWER_REFRESH_SCHEDULED		= 218,

		// byte    BSSRC_FOLLOWER_REFRESH_SCHEDULED
		// word64  packetTime
		// word64  refreshTime
		

	// Bitvise SSH Server versions >= 6.21

	BSSRC_WARNING							= 219,

		// byte    BSSRC_WARNING
		// word64  warningTime
		// word32  warningId
		// utf8str message


	// Bitvise SSH Server versions >= 6.22

	BSSRC_OBFS_FAILURE						= 228,

		// byte    BSSRC_OBFS_FAILURE
		// word64  packetTime
		// word64  connectionId
		// byte    obfsStatus	>= 7.21; ObfsStatus
		

	// Bitvise SSH Server versions >= 7.12

	BSSRC_KEX_MISMATCH						= 230,
	
		// byte    BSSRC_KEX_MISMATCH
		// word64  packetTime
		// word64  connectionId
		// word64  kexNr		1 - Initial key exchange
		// byte    algKind		0=Kex, 1=HostKey, 2=CipherIn, 3=CipherOut, 4=MacIn, 5=MacOut, 6=ComprIn, 7=ComprOut
		// utf8str clientAlgs
		// utf8str serverAlgs
		// utf8str disabledAlgs - algorithms disabled on the server

	// Bitvise SSH Server versions >= 7.21

	BSSRC_REMOTE_ADMIN_ACCESS				= 231,

		// byte    BSSRC_REMOTE_ADMIN_ACCESS
		// byte    type;    1=Full, 2=Delegated, 3=No
		// if type == 2 (delegated):
		//   bool    connectionTabAccess;
		//   bool    activityTabAccess;
		//   bool    manageHostKeys;

	BSSRC_DELEGATED_SETTINGS				= 232,

		// Beyond the scope of this document to describe.


	// Bitvise SSH Server >= 8.11

	BSSRC_CONNECTIONS_UNSUBSCRIBED			= 233,

		// byte    BSSRC_CONNECTIONS_UNSUBSCRIBED
		// word64  packetTime

	BSSRC_CHECK_FOR_UPDATES_RESULT			= 234,

		// Beyond the scope of this document to describe.

	BSSRC_DOWNLOAD_AND_START_UPDATE_RESULT	= 235,

		// byte    BSSRC_DOWNLOAD_AND_START_UPDATE_RESULT
		// word64  packetTime
		// bool    autoStarted
		// bool    errorOccurred
		// if errorOccurred:
		//   utf8str    errorMessage;

	BSSRC_FOLLOWER_NEWER_MASTER_VERSION		= 236,

		// byte    BSSRC_FOLLOWER_NEWER_MASTER_VERSION
		// word64  packetTime

	BSSRC_FOLLOWER_VERSION_PROCESSING_FAILED = 237,

		// byte    BSSRC_FOLLOWER_VERSION_PROCESSING_FAILED
		// word64  packetTime
		// utf8str errorDescription

	BSSRC_FOLLOWER_DOWNLOAD_INSTALLER_STARTED	= 238,

		// byte    BSSRC_FOLLOWER_DOWNLOAD_INSTALLER_STARTED
		// word64  packetTime
		// byte    downloadMethod		0=DownloadFromMaster, 1=DownloadFromURL

	BSSRC_FOLLOWER_DOWNLOAD_INSTALLER_FAILED	= 239,

		// byte    BSSRC_FOLLOWER_DOWNLOAD_INSTALLER_FAILED
		// word64  packetTime
		// byte    downloadMethod		0=DownloadFromMaster, 1=DownloadFromURL
		// utf8str errorDescription
		// word64  refreshTime

	BSSRC_FOLLOWER_INSTALLER_STARTE			= 240,

		// byte    BSSRC_FOLLOWER_INSTALLER_STARTED
		// word64  packetTime
		// bool    downgrade

	BSSRC_FOLLOWER_INSTALLER_FAILED			= 241,

		// byte    BSSRC_FOLLOWER_INSTALLER_FAILED
		// word64  packetTime
		// bool    downgrade
		// utf8str errorDescription
		// word64  refreshTime

	BSSRC_START_UPDATE_INITIATED			= 242,

		// byte    BSSRC_START_UPDATE_INITIATED
		// word64  packetTime
		// bool    autoUpdate
		// utf8str updateVersion

	BSSRC_START_UPDATE_SUCCESS				= 243,

		// byte    BSSRC_START_UPDATE_SUCCESS
		// word64  packetTime
		// bool    autoUpdate
		// utf8str updateVersion

	BSSRC_START_UPDATE_FAILURE				= 244,

		// byte    BSSRC_START_UPDATE_FAILURE
		// word64  packetTime
		// bool    autoUpdate
		// utf8str updateVersion
		// utf8str description

	// Bitvise SSH Server >= 9.12

	BSSRC_QUEUED_EMAILS						= 245,

		// byte    BSSRC_QUEUED_EMAILS
		// word64  packetTime
		// word64  nrQueuedMessages

	BSSRC_EMAIL_SENT						= 246,

		// byte    BSSRC_EMAIL_SENT
		// word64  packetTime
		// utf8str domain
		// utf8str mailbox
		// utf8str subject

	BSSRC_EMAIL_NOT_SENT					= 247,

		// byte    BSSRC_EMAIL_NOT_SENT
		// word64  packetTime
		// utf8str domain
		// utf8str mailbox
		// utf8str subject
		// utf8str reason
		// bool    final

	BSSRC_MESSAGE_QUEUE_CLEANUP				= 248,

		// byte    BSSRC_MESSAGE_QUEUE_CLEANUP
		// word64  packetTime
		// word64  nrRemovedMsgs

	BSSRC_EMAIL_STATUS						= 249,

		// byte    BSSRC_EMAIL_STATUS
		// word64  packetTime
		// bool    configured
		// word32  status

	BSSRC_EMAIL_FAILURE						= 250,

		// byte    BSSRC_EMAIL_FAILURE
		// word64  packetTime
		// utf8str description

	BSSRC_EMAIL_DELETED						= 251,

		// byte    BSSRC_EMAIL_DELETED
		// utf8str description
		// word64  packetTime
		// utf8str domain
		// word64  nrMailboxes
		// Repeat nrMailboxes times:
		//   utf8str  mailbox

};

struct Address
{
	byte	m_addrType;
	bytes	m_addrData;	// See DescribeAddress() for more info about this blob.
};

struct AddressPort : public Address
{
	word16	m_port;
};

struct Channel
{
	byte			m_channelType;		// ChannelType
	word32			m_channelNum;
	word64			m_openTime;			// FILETIME in UTC
	word64			m_bytesSent;
	word64			m_bytesRecv;
	// if m_channelType == CT_SESSION
	wstring			m_sessionInfo;		// e.g. "SFTP", "bvterm", etc.
	// if m_channelType == CT_CLTSIDE_S2C, CT_CLTSIDE_C2S, CT_BV_SRVSIDE_C2S, CT_BV_SRVSIDE_S2C
	AddressPort		m_serverAddr;
	AddressPort		m_serverCorespAddr;
	// if m_channelType == CT_CLTSIDE_C2S, CT_BV_SRVSIDE_C2S, CT_BV_SRVSIDE_S2C
	AddressPort		m_clientCorespAddr;
};

struct Connection
{
	word64			m_connectionId;
	AddressPort		m_remoteAddr;
	word64			m_acceptTime;		// UTC file time
	byte			m_obfsStatus;		// added in 6.22 and changed from 'bool obfsStatus' to 'byte obfsStatus' in 7.21; enum ObfsStatus, 0xFF for unknown failure
	bool			m_kexMismatch;		// added in 7.12
	wstring			m_clientVersion;	// empty if not yet received
	byte			m_accountType;		// enum AccountType; 0 if not yet authenticated
	wstring			m_accountName;		// empty if not yet authenticated
	vector<Channel> m_channels;
};

struct BlockedIp
{
	Address		m_ipAddress;
	word32		m_subnetBits;		// >= 8.11
	word64		m_blockTime;		// FILETIME in UTC
	word64		m_blockDuration;	// relative in FILETIME
	wstring		m_comment;
};



// Encoding 

void EncodeByte(bytes& out, byte v)
{
	out.append(1, v);
}

void EncodeBoolean(bytes& out, bool v)
{
	if (v)
		EncodeByte(out, 0x01);
	else
		EncodeByte(out, 0x00);
}

void EncodeWord16(bytes& out, word16 v)
{
	out.reserve(out.size() + 2);
	out.append(1, (byte) ((v >>  8) & 0xff));
	out.append(1, (byte) ((v      ) & 0xff));
}

void EncodeWord32(bytes& out, word32 v)
{
	out.reserve(out.size() + 4);
	out.append(1, (byte) ((v >> 24) & 0xff));
	out.append(1, (byte) ((v >> 16) & 0xff));
	out.append(1, (byte) ((v >>  8) & 0xff));
	out.append(1, (byte) ((v      ) & 0xff));
}

void EncodeWord64(bytes& out, word64 v)
{
	out.reserve(out.size() + 8);
	out.append(1, (byte) ((v >> 56) & 0xff));
	out.append(1, (byte) ((v >> 48) & 0xff));
	out.append(1, (byte) ((v >> 40) & 0xff));
	out.append(1, (byte) ((v >> 32) & 0xff));
	out.append(1, (byte) ((v >> 24) & 0xff));
	out.append(1, (byte) ((v >> 16) & 0xff));
	out.append(1, (byte) ((v >>  8) & 0xff));
	out.append(1, (byte) ((v      ) & 0xff));
}

void EncodeString(bytes& out, wstring const& v)
{	
	string narrow(W2N(v, CP_UTF8)); 
	word32 narrowSize = narrow.size();
	
	out.reserve(narrowSize + 4);
	EncodeWord32(out, narrowSize);
	out.append((byte const*) narrow.data(), narrowSize);
}

void EncodeAddress(bytes& out, Address const& v)
{
	EncodeByte(out, v.m_addrType);
	
	if (v.m_addrType == AT_IP4)
	{
		out.reserve(out.size() + 4);
		for (word32 n = 0; n < 4; ++n)
		{
			if (n < v.m_addrData.size())
				out.push_back(v.m_addrData[n]);
			else
				out.push_back(0);
		}
	}
	else if (v.m_addrType == AT_IP6)
	{
		out.reserve(out.size() + 20);
		for (word32 n = 0; n < 20; ++n)
		{
			if (n < v.m_addrData.size())
				out.push_back(v.m_addrData[n]);
			else
				out.push_back(0);
		}
	}
	else
	{
		out.reserve(out.size() + v.m_addrData.size() + 4);
		EncodeWord32(out, v.m_addrData.size());
		out.append(v.m_addrData);
	}
}

void EncodeAddressPort(bytes& out, AddressPort const& v)
{
	EncodeAddress(out, v);
	EncodeWord16(out, v.m_port);
}



// Decoding 

void DecodeByte(bytes& in, byte& v)
{
	if (in.size() < 1)
		throw DecodeErr();
	
	v = in[0];
	in.erase(0, 1);
}

void DecodeBoolean(bytes& in, bool& v)
{
	byte b;
	DecodeByte(in, b);
	v = (b != 0);
}

void DecodeWord16(bytes& in, word16& v)
{
	if (in.size() < 2)
		throw DecodeErr();

	v = (((word32) in[0]) <<  8) |
		(((word32) in[1])      );

	in.erase(0, 2);
}

void DecodeWord32(bytes& in, word32& v)
{
	if (in.size() < 4)
		throw DecodeErr();

	v = (((word32) in[0]) << 24) |
		(((word32) in[1]) << 16) |
		(((word32) in[2]) <<  8) |
		(((word32) in[3])      );

	in.erase(0, 4);
}

void DecodeWord64(bytes& in, word64& v)
{
	if (in.size() < 8)
		throw DecodeErr();

	v = (((word64) in[0]) << 56) |
		(((word64) in[1]) << 48) |
		(((word64) in[2]) << 40) |
		(((word64) in[3]) << 32) |
		(((word64) in[4]) << 24) |
		(((word64) in[5]) << 16) |
		(((word64) in[6]) <<  8) |
		(((word64) in[7])      );

	in.erase(0, 8);
}

void DecodeString(bytes& in, wstring& v)
{
	word32 narrowSize;
	DecodeWord32(in, narrowSize);
	if (in.size() < narrowSize)
		throw DecodeErr();

	string narrow((char const*) in.data(), narrowSize);
	in.erase(0, narrowSize);

	v = N2W(narrow, CP_UTF8);
}

void DecodeAddress(bytes& in, Address& v)
{
	DecodeByte(in, v.m_addrType);
	
	word32 size;
	if (v.m_addrType == AT_IP4)
		size = 4;
	else if (v.m_addrType == AT_IP6)
		size = 20;
	else
		DecodeWord32(in, size);

	if (in.size() < size)
		throw DecodeErr();

	v.m_addrData.assign(in, 0, size);
	in.erase(0, size);
}

void DecodeAddressPort(bytes& in, AddressPort& v)
{
	DecodeAddress(in, v);
	DecodeWord16(in, v.m_port);
}



// Descriptions

wstring DescribeAddress(Address const& x)
{
	wostringstream wos;
	if (x.m_addrType == AT_IP4)
	{
		for (unsigned int i = 0; i < 4; ++i)
		{
			byte part = 0;
			if (x.m_addrData.size() > i)
				part = x.m_addrData[i];

			if (i) wos << L".";
			wos << part;
		}		
	}
	else if (x.m_addrType == AT_IP6)
	{
		wos << L"[";
	
		// IPv6
		for (unsigned int i = 0; i < 16; i += 2)
		{
			word16 part = 0;
			if (x.m_addrData.size() > i)
			{
				part = (word16)(((word16) x.m_addrData[i]) << 8);
				if (x.m_addrData.size() > (i + 1))
					part |= x.m_addrData[i + 1];
			}

			if (i) 
				wos << L":";
			wos << hex << part;
		}

		// ScopeId
		word32 scopeId = 0;
		for (unsigned int i = 0; i < 4; ++i)
		{
			scopeId <<= 8;
			if (x.m_addrData.size() > i + 16)
				scopeId |= x.m_addrData[i + 16];
		}

		if (scopeId)
			wos << L"%" << hex << scopeId;

		wos << L"]";
	}
	else
	{
		string utf8DnsName((char const*) x.m_addrData.data(), x.m_addrData.size());
		wos << N2W(utf8DnsName, CP_UTF8);
	}
		
	return wos.str();
}

wstring DescribeAddressPort(AddressPort const& x)
{
	wostringstream wos;
	wos << DescribeAddress(x) << L":" << x.m_port;
	return wos.str();
}

wstring DescribeTime(word64 x)
{
	FILETIME ft;
	CopyMemory(&ft, &x, sizeof(ft));

	SYSTEMTIME st;
	if (!FileTimeToSystemTime(&ft, &st))
		throw ApiErr("Error converting file time.", "FileTimeToSystemTime()");

	wostringstream wos;
	wos.fill(L'0');
	wos << setw(4) << st.wYear   << L"-"
		<< setw(2) << st.wMonth  << L"-"
		<< setw(2) << st.wDay    << L" "
		<< setw(2) << st.wHour   << L":" 
		<< setw(2) << st.wMinute << L":"
		<< setw(2) << st.wSecond;

	return wos.str();
}

wchar_t const* DescribeAccountType(byte x)
{
	switch (x)
	{
	case AT_WINDOWS: return L"Windows";
	case AT_VIRTUAL: return L"Virtual";
	case AT_BSSACCT: return L"BvSshServer";
	default:		 return L"";
	}
}

wchar_t const* DescribeChannelType(byte x, wstring const& sessionInfo)
{
	switch (x)
	{
	case CT_SESSION:		return sessionInfo.c_str();
	case CT_CLTSIDE_C2S:	return L"client-side C2S forwarding";
	case CT_CLTSIDE_S2C:	return L"client-side S2C forwarding";
	case CT_BV_SRVSIDE_C2S:	return L"server-side C2S forwarding";
	case CT_BV_SRVSIDE_S2C:	return L"server-side S2C forwarding";
	case CT_BV_BSSRC:		return L"Bitvise SSH Server remote control";
	case CT_BV_CONF_SYNC:	return L"Bitvise SSH Server configuration synchronization";
	case CT_AUTH_AGENT:		return L"Authentication agent forwarding";
	default:				return L"";
	}
}



// NormalizeInstanceNames

bool NormalizeInstanceNames(wstring const& instanceName, wstring& newInstanceName, wstring& oldInstanceName)
{	
	wstring newPrefix = L"BvSshServer-";
	wstring newLongPrefix = L"Bitvise SSH Server - ";
	wstring oldPrefix = L"WinSSHD-";

	wstring suffix; // right of prefix
	if (instanceName.size() > newPrefix.size() && W2L(instanceName.substr(0, newPrefix.size())) == W2L(newPrefix))
		suffix = instanceName.substr(newPrefix.size());
	else if (instanceName.size() > oldPrefix.size() && W2L(instanceName.substr(0, oldPrefix.size())) == W2L(oldPrefix))
		suffix = instanceName.substr(oldPrefix.size());
	else if (instanceName.size() > newLongPrefix.size() && W2L(instanceName.substr(0, newLongPrefix.size())) == W2L(newLongPrefix))
		suffix = instanceName.substr(newLongPrefix.size());

	for (wstring::size_type pos = 0; pos < suffix.size(); )
	{
		wchar_t c = suffix[pos];
		if ((c >= L'0' && c <= L'9') ||
			(c >= L'A' && c <= L'Z') ||
			(c >= L'a' && c <= L'z') ||
			(c == L'-' || c == L'_'))
			++pos;
		else
			suffix.erase(pos, 1);
	}

	if (suffix.empty())
		return false;
	
	newInstanceName = newPrefix + suffix;
	oldInstanceName = oldPrefix + suffix;
	return true;
}


// GetDefaultInstanceNames

void GetDefaultInstanceNames(wstring& newInstanceName, wstring& oldInstanceName)
{
	newInstanceName = L"BvSshServer";
	oldInstanceName = L"WinSSHD";

	// Get the absolute path of BssStat.exe.

	wstring path;
	path.resize(_MAX_PATH);
	while (true)
	{
		DWORD result = GetModuleFileNameW(0, &path[0], (DWORD) path.size());
		if (!result)
			throw ApiErr("Retrieving default BvSshServer instance failed.", "GetModuleFileName()");
		else if (result == (DWORD) path.size())
			path.resize(path.size() * 2);
		else
		{
			path.resize(result);
			break;
		}
	}

	// Extract the topmost directory name.

	wstring::size_type end = path.find_last_of(L'\\');
	if (end == wstring::npos || end == 0)
		return;

	wstring::size_type beg = path.find_last_of(L'\\', end - 1);
	if (beg == wstring::npos)
		return;

	wstring instanceName = path.substr(beg + 1, end - beg - 1);
	NormalizeInstanceNames(instanceName, newInstanceName, oldInstanceName);
}




// main

int wmain(int argc, wchar_t** argv)
{
	try
	{
		// Process parameters

		bool showVersion = false;
		bool listConnections = false;
		bool listBlockedIps = false;
		bool disconnectConnection = false;
		bool unblockIp = false;
		bool forceLogRollover = false;
		wstring instanceName;
		word64 connectionId = 0;
		Address blockedIp;
		word32 subnetBits = 0;

		bool nextArgIsInstanceName = false;
		bool nextArgIsConnectionId = false;
		bool nextArgIsBlockedIp = false;

		for (int i = 1; i < argc; ++i)
		{
			if (nextArgIsInstanceName)
			{
				instanceName = argv[i];
				nextArgIsInstanceName = false;
			}
			else if (nextArgIsConnectionId)
			{
				wstringstream str;
				str << argv[i]; 
				str >> connectionId;
				nextArgIsConnectionId = false;
			}
			else if (nextArgIsBlockedIp)
			{
				wstring s = argv[i];
				wstringstream str;
				word32 defaultSubnetbits;
				wchar_t sepCh;

				if (s.find_first_not_of(L"0123456789./") == wstring::npos)
				{
					blockedIp.m_addrType = AT_IP4;
					blockedIp.m_addrData.resize(4, 0);

					str.str(s);
					for (unsigned int j = 0; j < 4; ++j)
					{
						if (j)
						{
							wchar_t dot;
							str >> dot;
							if (str.fail() || dot != '.')
								throw UsageErr("Invalid BlockedIP parameter value.");
						}

						word16 part;
						str >> part;
						if (str.fail() || part > 255)
							throw UsageErr("Invalid BlockedIP parameter value.");

						blockedIp.m_addrData[j] = (byte)part;
					}

					str >> sepCh;
					defaultSubnetbits = 32;
				}
				else if (s.find_first_not_of(L"0123456789abcdefABCDEF:[]%/") == wstring::npos)
				{
					blockedIp.m_addrType = AT_IP6;
					blockedIp.m_addrData.resize(20, 0);

					if (s.size() && s[0] == L'[')
					{
						s = s.substr(1);
						if (s.size() && s[s.size() - 1] == L']')
							s.substr(0, s.size() - 1);
					}

					str.str(s);
					for (unsigned int j = 0; j < 8; ++j)
					{
						if (j)
						{
							wchar_t col;
							str >> col;
							if (str.fail() || col != ':')
								throw UsageErr("Invalid BlockedIP parameter value.");
						}

						word16 part;
						str >> hex >> part;
						if (str.fail())
							throw UsageErr("Invalid BlockedIP parameter value.");

						blockedIp.m_addrData[2 * j] = (byte)((part >> 8) & 0xFF);
						blockedIp.m_addrData[2 * j + 1] = (byte)(part & 0xFF);
					}

					str >> sepCh;
					if (!str.fail() && sepCh == L'%')
					{
						word32 scopeId;
						str >> hex >> scopeId;
						if (!str.fail())
						{
							unsigned int pos = 16;
							blockedIp.m_addrData[pos++] = (byte)((scopeId >> 24) & 0xff);
							blockedIp.m_addrData[pos++] = (byte)((scopeId >> 16) & 0xff);
							blockedIp.m_addrData[pos++] = (byte)((scopeId >> 8) & 0xff);
							blockedIp.m_addrData[pos++] = (byte)((scopeId) & 0xff);
							str >> sepCh;
						}
					}

					defaultSubnetbits = 128;
				}
				else
					throw UsageErr("Invalid BlockedIP parameter value.");

				if (str.fail() || sepCh != L'/')
					subnetBits = defaultSubnetbits;
				else
				{
					str >> dec >> subnetBits;
					if (str.fail())
						throw UsageErr("Invalid BlockedIP parameter value.");
				}				

				nextArgIsBlockedIp = false;
			}
			else if (argv[i][0] == L'/' || argv[i][0] == L'-')
			{
				wstring arg(W2L(argv[i] + 1));

				if (arg == L"v")
					showVersion = true;
				else if (arg == L"s")
					listConnections = true;
				else if (arg == L"i")
					listBlockedIps = true;
				else if (arg == L"d")
				{
					disconnectConnection = true;
					nextArgIsConnectionId = true;
				}
				else if (arg == L"u")
				{
					unblockIp = true;
					nextArgIsBlockedIp = true;
				}
				else if (arg == L"r")
					forceLogRollover = true;
				else if (arg == L"n")
					nextArgIsInstanceName = true;
				else if (arg == L"?" || arg == L"h" || arg == L"help")
					throw UsageErr();
				else
					throw UsageErr("Unrecognized parameter.");
			}
			else
				throw UsageErr("Unrecognized parameter.");
		}

		if (nextArgIsInstanceName)
			throw UsageErr("Missing InstanceName parameter value.");
		if (nextArgIsConnectionId)
			throw UsageErr("Missing ConnectionID parameter value.");
		if (nextArgIsBlockedIp)
			throw UsageErr("Missing BlockedIP parameter value.");

		unsigned int mainParamCount = 0;
		if (showVersion)			++mainParamCount;
		if (listConnections)		++mainParamCount;
		if (listBlockedIps)			++mainParamCount;
		if (disconnectConnection)	++mainParamCount;
		if (unblockIp)				++mainParamCount;
		if (forceLogRollover)		++mainParamCount;

		if (mainParamCount == 0)
			throw UsageErr();
		else if (mainParamCount > 1)
			throw UsageErr("The -s, -d, -i, -u, -r, and -v parameters are to be used exclusively.");	

		wstring newInstanceName, oldInstanceName;
		if (instanceName.empty())
			GetDefaultInstanceNames(newInstanceName, oldInstanceName);
		else if (!NormalizeInstanceNames(instanceName, newInstanceName, oldInstanceName))
			throw UsageErr("Invalid InstanceName parameter value.");

		// Connect to BvSshServer control pipe
		
		AutoHandle pipe;
 
		wstring pipeNames[4]; // BvSshServer pipe name has 4 variations
		pipeNames[0] = L"\\\\.\\pipe\\" + newInstanceName + L"LocalCtrlPipe";		// BvSshServer 5.50 or newer
		pipeNames[1] = L"\\\\.\\pipe\\" + oldInstanceName + L"LocalWrcPipe";		// WinSSHD 5.23 - 5.26 (inclusive)
		pipeNames[2] = L"\\\\.\\pipe\\" + oldInstanceName + L"LocalWrcPipe1.01";	// WinSSHD 5.22
		pipeNames[3] = L"\\\\.\\pipe\\" + oldInstanceName + L"LocalWrcPipe1";		// WinSSHD 5.06 - 5.21 (inclusive)
	
		DWORD pipeError = ERROR_FILE_NOT_FOUND;
		for (int nameIdx = 0; nameIdx < 4 && !pipe.Valid() && pipeError == ERROR_FILE_NOT_FOUND; ++nameIdx)
		{
			for (int connectTry = 0; !pipe.Valid(); ++connectTry)
			{
				pipe.Set(CreateFileW(pipeNames[nameIdx].c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, NULL));
				if (!pipe.Valid())
				{
					pipeError = GetLastError();
					if (pipeError == ERROR_FILE_NOT_FOUND ||
						pipeError != ERROR_PIPE_BUSY || 
						connectTry > 9)
						break;
					
					WaitNamedPipe(pipeNames[nameIdx].c_str(), 500);
				}
			}
		}

		if (!pipe.Valid())
			throw ApiErr("Connecting to BvSshServer failed.", "CreateFile()", pipeError);

		// Send requests to BvSshServer
		
		bytes out;
		
		EncodeWord32(out, 1); // BSSRC_INIT packet is 1 byte long
		EncodeByte(out, BSSRC_INIT);
				
		if (listConnections)
		{
			EncodeWord32(out, 1); // BSSRC_SUBSCRIBE_CONNECTIONS packet is 1 byte long
			EncodeByte(out, BSSRC_SUBSCRIBE_CONNECTIONS);
			EncodeWord32(out, 1); // BSSRC_UNSUBSCRIBE_CONNECTIONS packet is 1 byte long
			EncodeByte(out, BSSRC_UNSUBSCRIBE_CONNECTIONS);
			EncodeWord32(out, 1); // BSSRC_LIST_CHANNELS packet is 1 byte long
			EncodeByte(out, BSSRC_LIST_CHANNELS);
		}
		else if (listBlockedIps)
		{
			EncodeWord32(out, 1); // BSSRC_LIST_BLOCKED_IPS packet is 1 byte long
			EncodeByte(out, BSSRC_LIST_BLOCKED_IPS);
		}
		else if (disconnectConnection)
		{
			bytes packet;
			EncodeByte(packet, BSSRC_DISCONNECT_CONNECTIONS);
			EncodeWord32(packet, 1); // Number of connections to disconnect
			EncodeWord64(packet, connectionId);

			EncodeWord32(out, packet.size());
			out.append(packet);
		}
		else if (unblockIp)
		{
			bytes packet;
			EncodeByte(packet, BSSRC_REMOVE_BLOCKED_IPS);
			EncodeWord32(packet, 1); // Number of IPs to unblock
			EncodeAddress(packet, blockedIp);
			EncodeWord32(packet, subnetBits);

			EncodeWord32(out, packet.size());
			out.append(packet);
		}
		else if (forceLogRollover)
		{
			EncodeWord32(out, 1);
			EncodeByte(out, BSSRC_FORCE_LOG_ROLLOVER); // Requires BvSshServer 8.11 or newer
		}
		
		DWORD written;
		if (!WriteFile(pipe, out.data(), out.size(), &written, 0))
			throw ApiErr("Sending requests to BvSshServer failed.", "WriteFile()");	

		// Read and process responses from BvSshServer

		bytes in;
		bool done = false;
		bool havePacketLen = false;
		word32 packetLen = 0;	
		vector<Connection> connections;
		vector<BlockedIp> blockedIps;
		wstring serverVersion;
		wstring serverVersionExtInfo;

		while (!done)
		{
			DWORD read = 32*1024;
			size_t origSize = in.size();
			in.resize(origSize + read);

			if (!ReadFile(pipe, &in[origSize], read, &read, 0))
				throw ApiErr("Reading responses from BvSshServer failed.", "ReadFile()");
			
			in.resize(origSize + read);
			
			while (!done)
			{
				if (!havePacketLen && in.size() >= 4)
				{
					DecodeWord32(in, packetLen);
					havePacketLen = true;
				}
				
				if (!havePacketLen || in.size() < packetLen)
					break;

				bytes packet(in, 0, packetLen);
				in.erase(0, packetLen);
				havePacketLen = false;

				byte type;
				DecodeByte(packet, type);
				if (type == BSSRC_STATUS)
				{
					bool error;
					DecodeBoolean(packet, error);
					if (!error)
					{
						if (disconnectConnection || unblockIp || forceLogRollover)
							done = true;
					}
					else
					{
						wstring errorDesc;
						DecodeString(packet, errorDesc);
						throw BvSshServerErr(W2N(errorDesc, CP_ACP).c_str());
					}
				}
				else if (type == BSSRC_VERSION)
				{
					DecodeString(packet, serverVersion);
					if (serverVersion >= L"5.22")
						DecodeString(packet, serverVersionExtInfo);

					if (showVersion)
						done = true;
				}
				else if (type == BSSRC_CONNECTIONS)
				{
					bool const obfsFailureAvail = serverVersion >= L"6.22";
					bool const obfsStatusAvail = serverVersion >= L"7.21";
										
					word64 packetTime;
					DecodeWord64(packet, packetTime);

					word32 nrConnections;
					DecodeWord32(packet, nrConnections);

					connections.resize(nrConnections);
					for (word32 i = 0; i < nrConnections; ++i)
					{
						Connection& c = connections[i];
						c.m_obfsStatus = OS_OK;
						c.m_kexMismatch = false;

						DecodeWord64(packet, c.m_connectionId);
						DecodeAddressPort(packet, c.m_remoteAddr);
						DecodeWord64(packet, c.m_acceptTime);
						if (obfsFailureAvail)
						{
							DecodeByte(packet, c.m_obfsStatus);
							if (!obfsStatusAvail && c.m_obfsStatus)
								c.m_obfsStatus = 0xFF; // setting unknown obfuscation failure
						}
						DecodeString(packet, c.m_clientVersion);
						DecodeByte(packet, c.m_accountType);
						DecodeString(packet, c.m_accountName);
					}
					if (serverVersion >= L"7.12") // extension 1 is available
					{
						for (word32 i = 0; i < nrConnections; ++i)
						{
							byte serviceType;
							DecodeByte(packet, serviceType); // ignoring it
							DecodeBoolean(packet, connections[i].m_kexMismatch);
						}
					}
					if (serverVersion >= L"8.11") // extension 2 is available
					{
						for (word32 i = 0; i < nrConnections; ++i)
						{
							byte fromMonitorIp;
							DecodeByte(packet, fromMonitorIp); // ignoring it
						}
					}
				}
				else if (type == BSSRC_CHANNELS)
				{
					word32 nrConnections;
					DecodeWord32(packet, nrConnections);

					for (word32 i = 0; i < nrConnections; ++i)
					{
						word64 currConnectionId;
						DecodeWord64(packet, currConnectionId);
						
						Connection* c = 0;
						for (size_t j = 0; !c && j < connections.size(); ++j)
							if (connections[j].m_connectionId == currConnectionId)
								c = &connections[j];
						
						word32 nrChannels;
						DecodeWord32(packet, nrChannels);
						
						if (c)
							c->m_channels.reserve(nrChannels);

						for (word32 k = 0; k < nrChannels; ++k)
						{
							Channel ch;
							DecodeByte(packet, ch.m_channelType);
							DecodeWord32(packet, ch.m_channelNum);
							DecodeWord64(packet, ch.m_openTime);
							DecodeWord64(packet, ch.m_bytesSent);
							DecodeWord64(packet, ch.m_bytesRecv);
							
							if (ch.m_channelType == CT_SESSION)
								DecodeString(packet, ch.m_sessionInfo);
							else if (ch.m_channelType == CT_CLTSIDE_C2S		||
									 ch.m_channelType == CT_CLTSIDE_S2C		||
									 ch.m_channelType == CT_BV_SRVSIDE_C2S	||
									 ch.m_channelType == CT_BV_SRVSIDE_S2C)
							{
								DecodeAddressPort(packet, ch.m_serverAddr);
								DecodeAddressPort(packet, ch.m_serverCorespAddr);

								if (ch.m_channelType != CT_CLTSIDE_S2C)
									DecodeAddressPort(packet, ch.m_clientCorespAddr);
							}

							if (c)
								c->m_channels.push_back(ch);
						}
					}

					if (listConnections)
						done = true;
				}
				else if (type == BSSRC_BLOCKED_IPS)
				{
					word32 nrBlockedIps;
					DecodeWord32(packet, nrBlockedIps);

					blockedIps.resize(nrBlockedIps);

					word32 i;
					for (i = 0; i < nrBlockedIps; ++i)
					{
						BlockedIp& b = blockedIps[i];
						DecodeAddress(packet, b.m_ipAddress);
						DecodeWord64(packet, b.m_blockTime);
						DecodeWord64(packet, b.m_blockDuration);
						DecodeString(packet, b.m_comment);
						b.m_subnetBits = b.m_ipAddress.m_addrType == AT_IP4 ? 32u : 128u;
					}

					if (packet.size())	// extension, requires sever version 8.11
					{
						for (i = 0; i < nrBlockedIps; ++i)
							DecodeWord32(packet, blockedIps[i].m_subnetBits);
					}

					if (listBlockedIps)
						done = true;
				}
			}
		}

		pipe.Close();

		// Process results

		if (disconnectConnection || unblockIp || forceLogRollover)
			wcout << L"Operation completed successfully." << endl;
		else if (showVersion)
		{
			wcout << L"Bitvise SSH Server " << serverVersion;
			if (serverVersionExtInfo.size())
				wcout << L" " << serverVersionExtInfo;
			wcout << endl;
		}
		else if (listBlockedIps)
		{
			wcout << L"Blocked IP count: " << blockedIps.size() << endl;

			for (size_t i = 0; i < blockedIps.size(); ++i)
			{
				BlockedIp const& b = blockedIps[i];

				wcout << endl
					  << L"IP address:        " << DescribeAddress(b.m_ipAddress) << L'/' << b.m_subnetBits << endl
					  << L"Blocked since:     " << DescribeTime(b.m_blockTime) << endl
					  << L"Blocked until:     " << DescribeTime(b.m_blockTime + b.m_blockDuration) << endl
					  << L"Comment:           " << b.m_comment << endl;
			}
		}
		else // if (listConnections)
		{
			wcout << L"Connection count: " << connections.size() << endl;

			for (size_t i = 0; i < connections.size(); ++i)
			{
				Connection const& c = connections[i];

				wcout << endl
					  << L"Connection ID:     " << c.m_connectionId << endl
					  << L"Remote address:    " << DescribeAddressPort(c.m_remoteAddr) << endl
					  << L"Connect time:      " << DescribeTime(c.m_acceptTime) << endl
					  << L"Client version:    " << c.m_clientVersion << endl
					  << L"Account:           " << c.m_accountName << endl
					  << L"Account type:      " << DescribeAccountType(c.m_accountType) << endl
					  << L"Channel count:     " << c.m_channels.size() << endl;

				for (size_t j = 0; j < c.m_channels.size(); ++j)
				{
					Channel const& ch = c.m_channels[j];
				
					wcout << L"+ Channel type:    " << DescribeChannelType(ch.m_channelType, ch.m_sessionInfo) << endl
						  << L"  Channel number:  " << ch.m_channelNum << endl
						  << L"  Open time:       " << DescribeTime(ch.m_openTime) << endl;

					if (ch.m_channelType == CT_CLTSIDE_C2S		||
						ch.m_channelType == CT_CLTSIDE_S2C		||
						ch.m_channelType == CT_BV_SRVSIDE_C2S	||
						ch.m_channelType == CT_BV_SRVSIDE_S2C)
					{
						wcout << L"  Server address:  " << DescribeAddressPort(ch.m_serverAddr) << endl
							  << L"  Server corresp:  " << DescribeAddressPort(ch.m_serverCorespAddr) << endl;

						if (ch.m_channelType != CT_CLTSIDE_S2C)
							  wcout << L"  Client corresp:  " << DescribeAddressPort(ch.m_clientCorespAddr) << endl;
					}

				    wcout << L"  Bytes sent:      " << ch.m_bytesSent << endl
						  << L"  Bytes received:  " << ch.m_bytesRecv << endl;
				}
			}
		}
	}
	catch (UsageErr const& e)
	{
		wcout.flush();
		cout << e.what() << endl;
		return 3;
	}
	catch (exception const& e)
	{
		wcout.flush();
		cout << e.what() << endl;
		return 1;
	}

	return 0;
}
