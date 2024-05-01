// This is the source code to a sample SfsDll shell, suitable for experimenting
// with the virtual filesystem provided by Bitvise SSH Server in an SSH connection.
// 
// To experiment with this sample, build it yourself, or use the pre-built
// SfsDllSample.exe. Run it from a terminal shell in an SSH Server connection.
///
// An application can use the SfsDll interface to access the current SSH user's
// virtual filesystem as configured for the user in SSH Server settings. The
// resources that an application can access this way are the same resources,
// in the same layout, as can be accessed by the user via SFTP or SCP.
//
// Copyright (C) 2015-2024 by Bitvise Limited.
//
// See SfsDll.h for license and more information.
//


// Win32
#include <Windows.h>

// SfsDll
#include "SfsDll.h"

// STL
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <exception>


using namespace std;



// String conversion (part I)

wstring N2W(char const* narrow, unsigned int codePage); // throw WinApiError
wstring N2W(string const& narrow, unsigned int codePage) { return N2W(narrow.c_str(), codePage); }

string W2N(wchar_t const* wide, unsigned int codePage); // throw WinApiError
string W2N(wstring const& wide, unsigned int codePage) { return W2N(wide.c_str(), codePage); }

unsigned int OutCP() { return GetConsoleOutputCP(); }
unsigned int InCP() { return GetConsoleCP(); }



// Exceptions

class WException : public exception
{
public:
	virtual wchar_t const* What() const = 0;
	char const* what() const
	{
		if (m_a.empty())
			m_a = W2N(What(), CP_ACP);
		return m_a.c_str();
	}

private:
	mutable string m_a;
};

class WStrException : public WException
{
public:
	WStrException(wchar_t const* z) : m_s(z) {}
	WStrException(wstring const& s) : m_s(s) {}

	wchar_t const* What() const { return m_s.c_str(); }

private:
	wstring m_s;
};


class WinApiError : public WException
{
public:
	WinApiError(wchar_t const* desc, wchar_t const* function, DWORD lastError = GetLastError())
	{
		wstringstream s;

		size_t descLen = desc ? wcslen(desc) : 0;
		if (descLen)
		{
			s << desc;
			wchar_t c = desc[descLen-1];
			if (c != L' ' && c != L'\n')
			{
				if (c == L'.' || c == L',' || c == L':' || c == L';')
					s << L" ";
				else
					s << L". ";
			}
		}

		s << function << L" failed with error code " << lastError << L".";
		m_s = s.str();
	}

	wchar_t const* What() const { return m_s.c_str(); }

private:
	wstring m_s;
};


class UsageError : public WStrException
{
public:
	UsageError(wchar_t const* z) : WStrException(z) {}
	UsageError(wstring const& s) : WStrException(s) {}
};

class UnexpectedParams : public UsageError
{
public:
	UnexpectedParams() : UsageError(L"Unexpected parameters present.") {}
};



// Automation

class NoCopy
{
private:
	NoCopy(NoCopy const& x);
	void operator= (NoCopy const& x);
public:
	NoCopy() {}
};


class AutoLocalFree : public NoCopy
{
public:
	AutoLocalFree(HLOCAL mem) : m_mem(mem) {}
	~AutoLocalFree() { LocalFree(m_mem); }

protected:
	HLOCAL m_mem;
};


class AutoSfsDllResponse : public NoCopy
{
public:
	AutoSfsDllResponse(SfsDllResponse const* r) : m_r(r) {}
	~AutoSfsDllResponse() { if (m_r) SfsDllFree(m_r); }

	SfsDllResponse const* operator->() const { return m_r; }
	operator SfsDllResponse const*() const { return m_r; }

private:
	SfsDllResponse const* m_r;
};



// Time conversion

bool UxtimeToFtime(time_t uxt, unsigned int ns, FILETIME& ft)
{
	if (uxt < 0)
		return false;

	if (ns >= 1000000000)
		return false;

	unsigned __int64 uxt64 = (unsigned __int64) uxt;
	unsigned __int64 ft64 = uxt64;
	ft64 += 11644473600;
	ft64 *= 10000000;
	ft64 += (ns / 100); 
	if (ft64 < uxt64) // if overflow in ft64
		return false;
	
	ft.dwHighDateTime = (DWORD) ((ft64 >> 32) & 0xFFFFFFFF);
	ft.dwLowDateTime = (DWORD) (ft64 & 0xFFFFFFFF);
	return true;
}

bool FtimeToUxtime(FILETIME ft, time_t& uxt, unsigned int* ns)
{
	unsigned __int64 ft64 = ft.dwHighDateTime;
	ft64 <<= 32;
	ft64 |= ft.dwLowDateTime;

	if (ns)
		*ns = (unsigned int) (ft64 % 10000000) * 100;

	unsigned __int64 uxt64 = ft64;
	uxt64 /= 10000000;
	uxt64 -= 11644473600;

	uxt = (time_t) uxt64;

	if (uxt64 > ft64 || uxt < 0 || uxt64 != (unsigned __int64) uxt)
		return false;
	else
		return true;
}


FILETIME UtcToLocal(FILETIME const& utcFileTime) // throw WinApiError
{
	SYSTEMTIME utcSystemTime;
	if (!FileTimeToSystemTime(&utcFileTime, &utcSystemTime))
		throw WinApiError(L"Converting UTC to local time failed:", L"FileTimeToSystemTime()");

	SYSTEMTIME localSystemTime;
	if (!SystemTimeToTzSpecificLocalTime(0, &utcSystemTime, &localSystemTime))
		throw WinApiError(L"Converting UTC to local time failed:", L"SystemTimeToTzSpecificLocalTime()");

	FILETIME localFileTime;
	if (!SystemTimeToFileTime(&localSystemTime, &localFileTime))
		throw WinApiError(L"Converting UTC to local time failed:", L"SystemTimeToFileTime()");

	return localFileTime;
}



// String conversion (part II)

wstring N2W(char const* narrow, unsigned int codePage) // throw WinApiError
{
	wstring wide;

	if (narrow && narrow[0])
	{
		DWORD flags;
		if (codePage == CP_UTF8 || codePage == CP_UTF7)
			flags = 0;
		else
			flags = MB_PRECOMPOSED | MB_ERR_INVALID_CHARS;

		int c = MultiByteToWideChar(codePage, flags, narrow, -1 , 0, 0);
		if (!c)
		{
			if (flags && GetLastError() == ERROR_INVALID_FLAGS)
				c = MultiByteToWideChar(codePage, flags = 0, narrow, -1, 0, 0);

			if (!c)
				throw WinApiError(L"Text conversion failed:", L"MultiByteToWideChar()");
		}

		wide.resize(c);

		c = MultiByteToWideChar(codePage, flags, narrow, -1, &wide[0], c);
		if (!c)
			throw WinApiError(L"Text conversion failed:", L"MultiByteToWideChar()");

		wide.resize(c-1);
	}

	return wide;
}

string W2N(wchar_t const* wide, unsigned int codePage) // throw WinApiError
{
	string narrow;

	if (wide && wide[0])
	{
		int c = WideCharToMultiByte(codePage, 0, wide, -1, 0, 0, 0, 0);
		if (!c)
			throw WinApiError(L"Text conversion failed:", L"WideCharToMultiByte()");

		narrow.resize(c);

		c = WideCharToMultiByte(codePage, 0, wide, -1, &narrow[0], c, 0, 0);
		if (!c)
			throw WinApiError(L"Text conversion failed:", L"WideCharToMultiByte()");

		narrow.resize(c-1);
	}

	return narrow;
}



// Parsers

template <class T>
T ParseNum(wchar_t const* str, T def = -1)
{
	size_t len = wcslen(str);
	bool useHex = len > 2 && str[0] == L'0' && str[1] == L'x';
	if (useHex)
		str += 2;

	wstringstream s;
	s << str;
	if (useHex)
		s >> hex;

	T n;
	s >> n;

	if (s.fail() || s.bad())
		return def;
	return n;
}

inline unsigned int ParseUInt(wchar_t const* str, unsigned int def = -1)
{
	return ParseNum(str, def);
}

inline unsigned __int64 ParseUInt64(wchar_t const* str, unsigned __int64 def = -1)
{
	return ParseNum(str, def);
}



// Describe

wstring DescribeStatusCode(unsigned int statusCode)
{
	switch (statusCode)
    {
	case SfsDllStatusCode::Ok:						return L"Ok";						// The operation completed successfully.
	case SfsDllStatusCode::Eof:						return L"Eof";						// Reached the end of the file.
    case SfsDllStatusCode::NoSuchFile:				return L"NoSuchFile";				// The file does not exist.
	case SfsDllStatusCode::PermissionDenied:		return L"PermissionDenied"; 		// Permission denied.
    case SfsDllStatusCode::Failure:					return L"Failure";					// The requested operation failed.
    case SfsDllStatusCode::BadMessage:				return L"BadMessage";				// A bad message received.
    case SfsDllStatusCode::NoConnection:			return L"NoConnection";				// There is no connection to the server. (For internal use.)
	case SfsDllStatusCode::ConnectionLost:			return L"ConnectionLost";			// The connection to the server was lost. (For internal use.)
    case SfsDllStatusCode::OpUnsupported:			return L"OpUnsupported";			// The requested operation is not supported.
    case SfsDllStatusCode::InvalidHandle:			return L"InvalidHandle";			// The handle is invalid.
    case SfsDllStatusCode::NoSuchPath:				return L"NoSuchPath";				// The file path does not exist or is invalid.
	case SfsDllStatusCode::FileAlreadyExists:		return L"FileAlreadyExists";		// The file already exists.
    case SfsDllStatusCode::WriteProtect:			return L"WriteProtect";				// The media is write protected.
    case SfsDllStatusCode::NoMedia:					return L"NoMedia";					// No media in drive.
	case SfsDllStatusCode::NoSpaceOnFilesystem:		return L"NoSpaceOnFilesystem";		// Insufficient free space on the filesystem.
    case SfsDllStatusCode::QuotaExceeded:			return L"QuotaExceeded";			// Not enough quota is available.
    case SfsDllStatusCode::UnknownPrincipal:		return L"UnknownPrincipal";			// A principal referenced by the request is unknown.
    case SfsDllStatusCode::LockConflict:			return L"LockConflict";				// The file could not be opened because it is locked by another process.
    case SfsDllStatusCode::DirNotEmpty:				return L"DirNotEmpty";				// The directory is not empty.
    case SfsDllStatusCode::NotADirectory:			return L"NotADirectory";			// The specified file is not a directory.
    case SfsDllStatusCode::InvalidFilename:			return L"InvalidFilename";			// The filename is not valid.
    case SfsDllStatusCode::LinkLoop:				return L"LinkLoop";					// Too many symbolic links encountered.
    case SfsDllStatusCode::CannotDelete:			return L"CannotDelete";				// The file cannot be deleted.
    case SfsDllStatusCode::InvalidParameter:		return L"InvalidParameter";			// The parameter is incorrect.
    case SfsDllStatusCode::FileIsADirectory:		return L"FileIsADirectory";			// The specified file is a directory
    case SfsDllStatusCode::ByteRangeLockConflict:	return L"ByteRangeLockConflict";	// Another process's mandatory byte-range lock overlaps with this request.
    case SfsDllStatusCode::ByteRangeLockRefused:	return L"ByteRangeLockRefused";		// A request for a byte range lock was refused.
	case SfsDllStatusCode::DeletePending:			return L"DeletePending";			// An operation was attempted on a file for which a delete operation is pending.
	case SfsDllStatusCode::FileCorrupt:				return L"FileCorrupt";				// The file is corrupt.
    case SfsDllStatusCode::OwnerInvalid:			return L"OwnerInvalid";				// The principal specified can not be assigned as an owner of a file.
    case SfsDllStatusCode::GroupInvalid:			return L"GroupInvalid";				// The principal specified can not be assigned as the primary group of a file.
    case SfsDllStatusCode::NoMatchingByteRangeLock:	return L"NoMatchingByteRangeLock";	// The requested operation could not be completed because the specifed byte range lock has not been granted.
	default:
		{
			wstringstream s;
			s << statusCode;
			return s.str();
		}
    }
}

wstring DescribeType(unsigned int type)
{
	switch (type)
	{
	case SfsDllType::Regular:		return L"Regular";
    case SfsDllType::Directory:		return L"Directory";
    case SfsDllType::Symlink:		return L"Symlink";
    case SfsDllType::Special:		return L"Special";
    case SfsDllType::Unknown:		return L"Unknown";
    case SfsDllType::Socket:		return L"Socket";
    case SfsDllType::CharDevice:	return L"CharDevice";
    case SfsDllType::BlockDevice:	return L"BlockDevice";
	case SfsDllType::Fifo:			return L"Fifo";
	default:
		{
			wstringstream s;
			s << type;
			return s.str();
		}
	}
}

wstring DescribeTextHint(unsigned char hint)
{
	switch (hint)
	{
	case SfsDllTextHint::KnownText:		return L"KnownText";
	case SfsDllTextHint::GuessedText:	return L"GuessedText";
	case SfsDllTextHint::KnownBinary:	return L"KnownBinary";
	case SfsDllTextHint::GuessedBinary:	return L"GuessedBinary";
	default:
		{
			wstringstream s;
			s << hint;
			return s.str();
		}
	}
}

wstring DescribeTime(unsigned __int64 time, unsigned int ns = 0)
{
	try
	{
		FILETIME ft;
		if (UxtimeToFtime((time_t) time, ns, ft))
		{
			ft = UtcToLocal(ft);

			SYSTEMTIME st;
			if (FileTimeToSystemTime(&ft, &st))
			{
				wstringstream s;
				s << setfill(L'0') << st.wYear
				  << L"-" << setw(2) << st.wMonth
				  << L"-" << setw(2) << st.wDay
				  << L" " << setw(2) << st.wHour
				  << L":" << setw(2) << st.wMinute
				  << L":" << setw(2) << st.wSecond
				  << L"." << setw(3) << st.wMilliseconds;

				return s.str();
			}
		}
	}
	catch (WinApiError const&)
	{
	}

	return L"[conversion failure]";
}


wstring DescribeData(unsigned int size, unsigned char const* ptr, unsigned int indent = 0)
{
	wstring indentStr(indent, L' ');

	wstringstream s;
	s << indentStr << L"      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   0123456789ABCDEF" << endl;
	s << setfill(L'0') << hex;

	for (unsigned int i = 0; i < size; i += 16)
	{
		s << indentStr << setw(4) << (i & 0xFFFF) << L"  ";

		for (unsigned int j = 0; j < 16; ++j)
		{
			if ((i + j) >= size)
				s << L"   ";
			else
				s << setw(2) << (unsigned) ptr[i + j] << L" ";
		}

		s << L" ";
		for (unsigned int j = 0; j < 16 && (i + j) < size; ++j)
		{
			wchar_t c[2] = { ptr[i + j], L'\0' };
			if (c[0] < 32 || c[0] >= 127)
				c[0] = '.';
			s << c;
		}

		s << endl;
	}

	return s.str();
}

wstring DescribeAttrs(SfsDllAttrs const* attrs, unsigned int indent = 0)
{
	wstring indentStr(indent, L' ');

	wstringstream s;
	s << indentStr << L"ValidAttrFlags: 0x" << hex << attrs->m_validAttrFlags << dec << endl
	  << indentStr << L"Type: " << DescribeType(attrs->m_type) << endl;

	if (attrs->m_validAttrFlags & SfsDllAttr::Size)
		s << indentStr << L"Size: " << attrs->m_size << endl;
	if (attrs->m_validAttrFlags & SfsDllAttr::AllocSize)
		s << indentStr << L"AllocSize: " << attrs->m_allocSize << endl;

	if (attrs->m_validAttrFlags & SfsDllAttr::OwnerGroup)
	{
		s << indentStr << L"Owner: " << attrs->m_owner << endl;
		s << indentStr << L"Group: " << attrs->m_group << endl;
	}

	if (attrs->m_validAttrFlags & SfsDllAttr::Permissions)
		s << indentStr << L"Permissions: 0x" << hex << attrs->m_permissions << dec << endl;

	bool subsec = !!(attrs->m_validAttrFlags & SfsDllAttr::Subseconds);

	if (attrs->m_validAttrFlags & SfsDllAttr::AccessTime)
		s << indentStr << L"AccessTime: " << DescribeTime(attrs->m_accessTime, subsec ? attrs->m_accessTimeNs : 0) << endl;
	if (attrs->m_validAttrFlags & SfsDllAttr::CreateTime)
		s << indentStr << L"CreateTime: " << DescribeTime(attrs->m_createTime, subsec ? attrs->m_createTimeNs : 0) << endl;
	if (attrs->m_validAttrFlags & SfsDllAttr::ModifyTime)
		s << indentStr << L"ModifyTime: " << DescribeTime(attrs->m_modifyTime, subsec ? attrs->m_modifyTimeNs : 0) << endl;
	if (attrs->m_validAttrFlags & SfsDllAttr::CTime)
		s << indentStr << L"CTime: " << DescribeTime(attrs->m_cTime, subsec ? attrs->m_cTimeNs : 0) << endl;

	if (attrs->m_validAttrFlags & SfsDllAttr::Acl)
	{
		SfsDllAcl const& acl = attrs->m_acl;
		s << indentStr << L"ACL" << endl
		  << indentStr << L"  Flags: 0x" << hex << acl.m_flags << dec << endl
		  << indentStr << L"  AceCount: " << acl.m_aceCount << endl;

		for (unsigned int i = 0; i < acl.m_aceCount; ++i)
		{
			SfsDllAce const& ace = acl.m_aceArray[i];
			s << indentStr << L"  ACE[" << i << L"]" << endl
			  << indentStr << L"    Type: " << ace.m_type << endl
			  << indentStr << L"    Flags: 0x" << hex << ace.m_flags << dec << endl
			  << indentStr << L"    Mask: 0x" << hex << ace.m_mask << dec << endl
			  << indentStr << L"    Who: " << hex << ace.m_who << dec << endl;
		}
	}

	if (attrs->m_validAttrFlags & SfsDllAttr::Bits)
	{
		s << indentStr << L"AttrBits: 0x" << hex << attrs->m_attrBits << dec << endl;
		s << indentStr << L"AttrBitsValid: 0x" << hex << attrs->m_attrBitsValid << dec << endl;
	}

	if (attrs->m_validAttrFlags & SfsDllAttr::TextHint)
		s << indentStr << L"TextHint: " << DescribeTextHint(attrs->m_textHint) << endl;

	if (attrs->m_validAttrFlags & SfsDllAttr::MimeType)
		s << indentStr << L"MimeType: " << attrs->m_mimeType << endl;

	if (attrs->m_validAttrFlags & SfsDllAttr::LinkCount)
		s << indentStr << L"LinkCount: " << attrs->m_linkCount << endl;

	return s.str();
}


wstring Describe(SfsDllStatus const* status)
{
	wstringstream s;
	s << L"SfsDllStatus" << endl
	  << L"  StatusCode: " << DescribeStatusCode(status->m_statusCode) << endl;

	if (status->m_errorMessage && status->m_errorMessage[0])
		s << L"  ErrorMessage: " << status->m_errorMessage << endl;

	if (status->m_languageTag && status->m_languageTag[0])
		s << L"  LanguageTag: " << status->m_languageTag << endl;

	return s.str();
}

wstring Describe(SfsDllHandle const* handle)
{
	wstringstream s;
	s << L"SfsDllHandle" << endl
	  << L"  Handle: " << handle->m_handle << endl
	  << L"  CreatedNewFile: " << handle->m_createdNewFile << endl;

	return s.str();
}

wstring Describe(SfsDllData const* data)
{
	wstringstream s;
	s << L"SfsDllData" << endl
	  << L"  DataSize: " << data->m_dataSize << endl;

	if (data->m_dataSize)
		s << DescribeData(data->m_dataSize, data->m_dataPtr, 4);

	s << L"  EndOfFile: " << data->m_endOfFile << endl;

	return s.str();
};


wstring Describe(SfsDllNames const* names)
{
	wstringstream s;
	s << L"SfsDllNames" << endl
	  << L"  NameCount: " << names->m_nameCount << endl;

	for (unsigned int i = 0; i < names->m_nameCount; ++i)
	{
		SfsDllName const& name = names->m_nameArray[i];
		s << L"  Name[" << i << L"]" << endl
		  << L"    FileName: " << name.m_fileName << endl
		  << L"    Attrs" << endl
		  << DescribeAttrs(&name.m_attrs, 6);
	}

	s << L"  EndOfList: " << names->m_endOfList << endl;

	return s.str();
}

wstring Describe(SfsDllAttrs const* attrs)
{
	wstringstream s;
	s << L"SfsDllAttrs" << endl
	  << DescribeAttrs(attrs, 2);

	return s.str();
}

wstring Describe(SfsDllName const* name)
{
	wstringstream s;
	s << L"SfsDllName" << endl
	  << L"  FileName: " << name->m_fileName << endl
	  << L"  Attrs" << endl
	  << DescribeAttrs(&name->m_attrs, 4);

	return s.str();
}

wstring Describe(SfsDllCheckFileReply const* reply)
{
	wstringstream s;
	s << L"SfsDllCheckFileReply" << endl
	  << L"  HashAlgUsed: " << reply->m_hashAlgUsed << endl
	  << L"  HashDataSize: " << reply->m_hashDataSize << endl;

	if (reply->m_hashDataSize)
		s << DescribeData(reply->m_hashDataSize, reply->m_hashDataPtr, 4);

	s << endl;

	return s.str();
}

wstring Describe(SfsDllSpaceAvailReply const* reply)
{
	wstringstream s;
	s << L"SfsDllSpaceAvailReply" << endl
	  << L"  BytesOnDevice: " << reply->m_bytesOnDevice << endl
	  << L"  UnusedBytesOnDevice: " << reply->m_unusedBytesOnDevice << endl
	  << L"  BytesAvailableToUser: " << reply->m_bytesAvailableToUser << endl
	  << L"  UnusedBytesAvailableToUser: " << reply->m_unusedBytesAvailableToUser << endl
	  << L"  BytesPerAllocationUnit: " << reply->m_bytesPerAllocationUnit << endl;

	return s.str();
}

wstring Describe(SfsDllPosixPermReply const* reply)
{
	wstringstream s;
	s.fill('0');

	s << L"SfsDllPosixPermReply" << endl
		<< L"  PosixPermDir: " << oct << setw(4) << reply->m_posixPermDir << endl
		<< L"  PosixPermFile: " << oct << setw(4) << reply->m_posixPermFile << endl;

	return s.str();
}

wstring Describe(SfsDllClientVersionReply const* reply)
{
	wstring versionSanitized;
	{
		size_t const length = wcslen(reply->m_versionUnsanitized);
		versionSanitized.assign(reply->m_versionUnsanitized, min(length, 1000));
		for (wstring::iterator it = versionSanitized.begin(); it != versionSanitized.end(); ++it)
			if (*it < 32 || *it >= 127)
				*it = L'?';

		if (versionSanitized.size() != length)
		{
			versionSanitized.resize(versionSanitized.size()-3);
			versionSanitized.append(L"...", 3);
		}
	}

	wstringstream s;
	s << L"SfsDllClientVersionReply" << endl
	  << L"  Version: " << versionSanitized << endl;

	return s.str();
}


wstring Describe(SfsDllResponse const* response)
{
	switch (response->m_type)
	{
	case SfsDllResponseType::Status:
		return Describe((SfsDllStatus const*) response->m_content);
	case SfsDllResponseType::Handle:
		return Describe((SfsDllHandle const*) response->m_content);
	case SfsDllResponseType::Data:
		return Describe((SfsDllData const*) response->m_content);
	case SfsDllResponseType::Names:
		return Describe((SfsDllNames const*) response->m_content);
	case SfsDllResponseType::Attrs:
		return Describe((SfsDllAttrs const*) response->m_content);
	case SfsDllResponseType::Name:
		return Describe((SfsDllName const*) response->m_content);
	case SfsDllResponseType::CheckFileReply:
		return Describe((SfsDllCheckFileReply const*) response->m_content);
	case SfsDllResponseType::SpaceAvailReply:
		return Describe((SfsDllSpaceAvailReply const*) response->m_content);
	case SfsDllResponseType::PosixPermReply:
		return Describe((SfsDllPosixPermReply const*) response->m_content);
	case SfsDllResponseType::ClientVersionReply:
		return Describe((SfsDllClientVersionReply const*) response->m_content);
	default:
		throw WStrException(L"Describe(): Unrecognized response type.");
	}
}



// Handlers

wstring FormatException(SfsDllException const* ex)
{
	if (ex->m_type == SfsDllExceptionType::WinApi)
		return WinApiError(ex->m_desc, ex->m_winApiError.m_function, ex->m_winApiError.m_lastError).What();
	else if (ex->m_type != SfsDllExceptionType::Flow)
		return ex->m_desc;
	else
	{
		wstringstream s;
		s << L"Error in component: " << ex->m_flowError.m_component
		  << L", class: " << ex->m_flowError.m_cls
		  << L", code: " << ex->m_flowError.m_code
		  << L", description: " << ex->m_desc;

		return s.str();
	}
}

void __cdecl ExceptionHandler(void*, SfsDllException const* ex)
{
	if (ex->m_type == SfsDllExceptionType::WinApi)
		throw WinApiError(ex->m_desc, ex->m_winApiError.m_function, ex->m_winApiError.m_lastError);
	else if (ex->m_type != SfsDllExceptionType::Flow)
		throw WStrException(ex->m_desc);
	else
		throw WStrException(FormatException(ex));
}

void __cdecl EventHandler(void*, SfsDllEvent const* ev)
{
	if (ev->m_type == SfsDllEventType::ServerDisconnect)
		cout << "Event: ServerDisconnect" << endl;
	else if (ev->m_type == SfsDllEventType::TerminalException)
	{
		wstring s = L"Event: TerminalException: " + FormatException(&ev->m_terminalException);
		cout << W2N(s, OutCP()) << endl;
	}
}



// Main

int main()
{
	try
	{
		cout << "SfsDllVersion: " << SfsDllVersion() << endl;

		SfsDllHandlers handlers = { ExceptionHandler, 0, EventHandler, 0 };
		SfsDllInitialize(&handlers);

		while (true)
		{
			cout << "sfs> ";

			char buffer[4*1024] = { 0 };
			cin.getline(buffer, sizeof(buffer) - 1);
			if (cin.fail() || cin.bad() || (cin.eof() && !buffer[0]))
				break;

			int argc;
			wchar_t** argv = CommandLineToArgvW(N2W(buffer, InCP()).c_str(), &argc);
			AutoLocalFree autoLocalFree(argv);

			try
			{
				if (!argc)
					throw UsageError(L"");

				wstring instr = argv[0];
				CharLowerBuffW(&instr[0], (DWORD) instr.size());

				if (instr == L"list" || instr == L"ls" || instr == L"dir")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: directory");
					if (argc != 2)
						throw UnexpectedParams();

					SfsDllOpenDir openDir = { argv[1] };
					SfsDllRequest request = { SfsDllRequestType::OpenDir, &openDir };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					wstringstream s;

					if (response->m_type == SfsDllResponseType::Status)
						s << L"Error opening directory." << endl << Describe(response);
					else
					{
						unsigned int handle = ((SfsDllHandle const*) response->m_content)->m_handle;

						while (true)
						{
							SfsDllReadDir readDir = { handle };
							SfsDllRequest request = { SfsDllRequestType::ReadDir, &readDir };
							AutoSfsDllResponse response = SfsDllIssue(&request);

							if (response->m_type == SfsDllResponseType::Status)
							{
								SfsDllStatus const* status = (SfsDllStatus const*) response->m_content;
								if (status->m_statusCode != SfsDllStatusCode::Eof)
									s << L"Error reading directory." << endl << Describe(status);

								break;
							}
							else
							{
								SfsDllNames const* names = (SfsDllNames const*) response->m_content;
								for (unsigned int i = 0; i < names->m_nameCount; ++i)
								{
									s << names->m_nameArray[i].m_fileName;
									if (names->m_nameArray[i].m_attrs.m_type == SfsDllType::Directory)
										s << L"/";
									s << endl;
								}

								if (names->m_endOfList)
									break;
							}
						}

						SfsDllClose close = { handle };
						SfsDllRequest request = { SfsDllRequestType::Close, &close };
						AutoSfsDllResponse response = SfsDllIssue(&request);
					}

					cout << W2N(s.str(), OutCP()) << endl;
				}
				else if (instr == L"move" || instr == L"mv" || instr == L"ren")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: source-path");
					if (argc < 3)
						throw UsageError(L"Missing parameter: target-path");

					unsigned int flags = 0;
					for (int i = 3; i < argc; ++i)
					{
						wstring s = argv[i];
						CharLowerBuffW(&s[0], (DWORD) s.size());

						if (s == L"-o" || s == L"-overwrite")
							flags |= SfsDllRenameFlag::Overwrite;
						else if (s == L"-a" || s == L"-atomic")
							flags |= SfsDllRenameFlag::Atomic;
						else if (s == L"-n" || s == L"-native")
							flags |= SfsDllRenameFlag::Native;
						else
							throw UnexpectedParams();
					}

					SfsDllRename rename = { argv[1], argv[2], flags };
					SfsDllRequest request = { SfsDllRequestType::Rename, &rename };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"setsize")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: file");
					if (argc < 3)
						throw UsageError(L"Missing parameter: size");
					if (argc != 3)
						throw UnexpectedParams();

					SfsDllAttrs attrs = { SfsDllAttr::Size, SfsDllType::Unknown, ParseUInt64(argv[2]) };
					SfsDllSetStat setStat = { argv[1], attrs };
					SfsDllRequest request = { SfsDllRequestType::SetStat, &setStat };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"remove" || instr == L"rm" || instr == L"del")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: file");

					SfsDllRemove remove = { argv[1] };
					SfsDllRequest request = { SfsDllRequestType::Remove, &remove };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"mkdir" || instr == L"md")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: directory");

					SfsDllAttrs attrs = { 0 };
					SfsDllMkDir mkDir = { argv[1], attrs };
					SfsDllRequest request = { SfsDllRequestType::MkDir, &mkDir  };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"rmdir" || instr == L"rd")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: directory");

					SfsDllRmDir rmDir = { argv[1] };
					SfsDllRequest request = { SfsDllRequestType::RmDir, &rmDir };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"stat")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: path");

					bool followSymlink = true;
					unsigned int f = SfsDllAttr::Size | SfsDllAttr::AllocSize
						| SfsDllAttr::OwnerGroup | SfsDllAttr::Permissions | SfsDllAttr::Acl
						| SfsDllAttr::AccessTime | SfsDllAttr::CreateTime | SfsDllAttr::ModifyTime | SfsDllAttr::CTime | SfsDllAttr::Subseconds
						| SfsDllAttr::Bits | SfsDllAttr::TextHint | SfsDllAttr::MimeType  | SfsDllAttr::LinkCount;
					for (int i = 2; i < argc; ++i)
					{
						wstring s = argv[i];
						CharLowerBuffW(&s[0], (DWORD) s.size());

						if (s == L"-n" || s == L"-no-follow-symlink")
							followSymlink = false;
						else
						{
							if (s.size() > 3)
								s.resize(3);

							if (s == L"-f=")
								f = ParseNum(argv[i] + 3, f);
							else
								throw UnexpectedParams();
						}
					}

					SfsDllStat stat = { argv[1], f, followSymlink };
					SfsDllRequest request = { SfsDllRequestType::Stat, &stat };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"space")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: directory");

					SfsDllSpaceAvail space = { argv[1] };
					SfsDllRequest request = { SfsDllRequestType::SpaceAvail, &space };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"posixperm")
				{
					SfsDllRequest request = { SfsDllRequestType::PosixPerm, 0 };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"clientversion")
				{
					if (SfsDllVersion() < 2)
						throw UsageError(L"SfsDllClientVersion requires SfsDllVersion 2 or newer");
					
					SfsDllRequest request = { SfsDllRequestType::ClientVersion, 0 };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"changepassword")
				{
					if (SfsDllVersion() < 3)
						throw UsageError(L"SfsDllClientVersion requires SfsDllVersion 3 or newer");
					
					if (argc < 2)
						throw UsageError(L"Missing parameter: current-password.");
					if (argc < 3)
						throw UsageError(L"Missing parameter: new-password.");
					if (argc != 3)
						throw UnexpectedParams();

					SfsDllChangePassword changePassword = { argv[1], argv[2] };
					SfsDllRequest request = { SfsDllRequestType::ChangePassword, &changePassword };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"copy" || instr == L"cp")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: source-file");
					if (argc < 3)
						throw UsageError(L"Missing parameter: target-file");

					bool overwrite = false;
					for (int i = 3; i < argc; ++i)
					{
						wstring s = argv[i];
						CharLowerBuffW(&s[0], (DWORD) s.size());

						if (s == L"-o" || s == L"-overwrite")
							overwrite = true;
						else
							throw UnexpectedParams();
					}

					SfsDllFileCopy copy = { argv[1], argv[2], overwrite };
					SfsDllRequest request = { SfsDllRequestType::FileCopy, &copy };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"hopen")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: file");

					unsigned int d = 0x201FF; // ReadAcl | [Read, WriteAttrs]
					unsigned int f = SfsDllOpenFlag::OpenOrCreate;
					for (int i = 2; i < argc; ++i)
					{
						wstring s = argv[i];
						if (s.size() > 3)
							s.resize(3);

						CharLowerBuffW(&s[0], (DWORD) s.size());

						if (s == L"-d=")
							d = ParseNum(argv[i] + 3, d);
						else if (s == L"-f=")
							f = ParseNum(argv[i] + 3, f);
						else
							throw UnexpectedParams();
					}

					SfsDllAttrs attrs = { 0 };
					SfsDllOpen open = { argv[1], d, f, attrs };
					SfsDllRequest request = { SfsDllRequestType::Open, &open };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"hopendir")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: directory");
					if (argc != 2)
						throw UnexpectedParams();

					SfsDllOpenDir openDir = { argv[1] };
					SfsDllRequest request = { SfsDllRequestType::OpenDir, &openDir };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"hclose")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: handle");
					if (argc != 2)
						throw UnexpectedParams();

					SfsDllClose close;
					close.m_handle = ParseNum<unsigned int>(argv[1]);
					SfsDllRequest request = { SfsDllRequestType::Close, &close };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"hreaddir")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: handle.");
					if (argc != 2)
						throw UnexpectedParams();

					SfsDllReadDir readDir = { ParseUInt(argv[1]) };
					SfsDllRequest request = { SfsDllRequestType::ReadDir, &readDir };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"hread")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: handle");
					if (argc < 3)
						throw UsageError(L"Missing parameter: offset");
					if (argc < 4)
						throw UsageError(L"Missing parameter: length");
					if (argc != 4)
						throw UnexpectedParams();

					SfsDllRead read = {	ParseUInt(argv[1]), ParseUInt64(argv[2], 0), ParseUInt(argv[3], 0) };
					SfsDllRequest request = { SfsDllRequestType::Read, &read };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"hwrite")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: handle");
					if (argc < 3)
						throw UsageError(L"Missing parameter: offset");
					if (argc < 4)
						throw UsageError(L"Missing parameter: data");

					bool hexEncodedData = false;
					for (int i = 4; i < argc; ++i)
					{
						wstring s = argv[i];
						CharLowerBuffW(&s[0], (DWORD) s.size());

						if (s == L"-h" || s == L"-hex-encoded-data")
							hexEncodedData = true;
						else
							throw UnexpectedParams();
					}

					basic_string<unsigned char> data;
					if (!hexEncodedData)
					{
						string s = W2N(argv[3], CP_UTF8);
						data.assign((unsigned char const*) s.data(), s.size());
					}
					else
					{
						wstring hex = argv[3];
						CharLowerBuffW(&hex[0], (DWORD) hex.size());

						data.reserve(hex.size() / 2);

						bool newPair = true;
						for (wstring::iterator it = hex.begin(); it != hex.end(); ++it)
						{
							wchar_t base = 0;
							wchar_t add = 0;
							if (*it >= L'0' && *it <= L'9')
								base = L'0';
							else if (*it >= 'a' && *it <= 'f')
							{
								base = L'a';
								add = 10;
							}

							if (!base)
								newPair = true;
							else if (newPair)
							{
								data.push_back((unsigned char) (*it - base + add));
								newPair = false;
							}
							else
							{
								data[data.size()-1] <<= 4;
								data[data.size()-1] |= ((unsigned char) (*it - base + add));
								newPair = true;
							}
						}
					}

					SfsDllWrite write = { ParseUInt(argv[1]), ParseUInt64(argv[2], 0), (unsigned int) data.size(), data.data() };
					SfsDllRequest request = { SfsDllRequestType::Write, &write };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"hcopy")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: read-handle");
					if (argc < 3)
						throw UsageError(L"Missing parameter: read-offset");
					if (argc < 4)
						throw UsageError(L"Missing parameter: read-length");
					if (argc < 5)
						throw UsageError(L"Missing parameter: write-handle");
					if (argc < 6)
						throw UsageError(L"Missing parameter: write-offset");
					if (argc != 6)
						throw UnexpectedParams();

					SfsDllDataCopy copy = { ParseUInt(argv[1]), ParseUInt64(argv[2], 0), ParseUInt(argv[3], 0), ParseUInt(argv[4]), ParseUInt64(argv[5], 0) };
					SfsDllRequest request = { SfsDllRequestType::DataCopy, &copy };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"hstat")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: handle");

					unsigned int f = SfsDllAttr::Size | SfsDllAttr::AllocSize
						| SfsDllAttr::OwnerGroup | SfsDllAttr::Permissions | SfsDllAttr::Acl
						| SfsDllAttr::AccessTime | SfsDllAttr::CreateTime | SfsDllAttr::ModifyTime | SfsDllAttr::CTime | SfsDllAttr::Subseconds
						| SfsDllAttr::Bits | SfsDllAttr::TextHint | SfsDllAttr::MimeType  | SfsDllAttr::LinkCount;
					for (int i = 2; i < argc; ++i)
					{
						wstring s = argv[i];
						if (s.size() > 3)
							s.resize(3);

						CharLowerBuffW(&s[0], (DWORD) s.size());

						if (s == L"-f=")
							f = ParseNum(argv[i] + 3, f);
						else
							throw UnexpectedParams();
					}

					SfsDllFStat stat = { ParseUInt(argv[1]), f };
					SfsDllRequest request = { SfsDllRequestType::FStat, &stat };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"hsetsize")
				{
					if (argc < 2)
						throw UsageError(L"Missing parameter: handle");
					if (argc < 3)
						throw UsageError(L"Missing parameter: size");
					if (argc != 3)
						throw UnexpectedParams();

					SfsDllAttrs attrs = { SfsDllAttr::Size, SfsDllType::Unknown, ParseUInt64(argv[2]) };
					SfsDllFSetStat setStat = { ParseUInt(argv[1]), attrs };
					SfsDllRequest request = { SfsDllRequestType::FSetStat, &setStat };
					AutoSfsDllResponse response = SfsDllIssue(&request);

					cout << W2N(Describe(response), OutCP()) << endl;
				}
				else if (instr == L"quit" || instr == L"exit" || instr == L"bye")
				{
					break;
				}
				else
				{
					if (instr == L"help")
						throw UsageError(L"");
					else
						throw UsageError(L"Unrecognized instruction");
				}
			}
			catch (UsageError const& e)
			{
				wchar_t const* what = e.What();
				if (what[0])
					cout << W2N(what, OutCP()) << endl << endl;
				else
					cout << "Supported instructions:" << endl
						 << "list directory" << endl
						 << "move source-path target-path [-overwrite] [-atomic] [-native]" << endl
						 << "stat path [-f=flags] [-no-follow-symlink]" << endl
						 << "setsize file size" << endl
						 << "remove file" << endl
						 << "mkdir directory" << endl
						 << "rmdir directory" << endl
						 << "space directory" << endl
						 << "posixperm" << endl
						 << "clientversion" << endl
						 << "changepassword current-password new-password" << endl
						 << "copy source-file target-file [-overwrite]" << endl
						 << "hopen file [-d=desired-access] [-f=flags]" << endl
						 << "hopendir directory" << endl
						 << "hclose handle" << endl
						 << "hreaddir handle" << endl
						 << "hread handle offset length" << endl
						 << "hwrite handle offset data [-hex-encoded-data]" << endl
						 << "hcopy read-handle read-offset read-length write-handle write-offset" << endl
						 << "hstat handle [-f=flags]" << endl
						 << "hsetsize handle size" << endl
						 << "quit" << endl << endl;
			}
		}
	}
	catch (WException const& e)
	{
		cout << W2N(e.What(), OutCP()) << endl;
	}
	catch (std::exception const& e)
	{
		wstring what = N2W(e.what(), CP_ACP);
		cout << W2N(what, OutCP()) << endl;
	}

	return 0;
};
