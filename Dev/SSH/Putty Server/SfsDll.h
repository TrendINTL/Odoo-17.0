// This C++ header defines the interface for a third-party application to
// interact with Bitvise SSH Server's virtual filesystem when run in a
// terminal shell, or as an exec request, under an SSH Server connection.
//
// An application can use this interface to access the current SSH user's
// virtual filesystem as configured for the user in SSH Server settings. The
// resources that an application can access this way are the same resources,
// in the same layout, as can be accessed by the user via SFTP or SCP.
//
// A 32-bit application should link with SfsDll32.lib, and will have a
// dependency on SfsDll32.dll, which is included with Bitvise SSH Server.
// A 64-bit application should link with SfsDll64.lib, and will have a
// dependency on SfsDll64.dll.
//
// If your application targets Windows XP or Windows Server 2003, you must
// reference SfsDll as a static dependency. SfsDll uses implicit thread local
// storage, which makes it unsafe to delay-load on XP/2003 using LoadLibrary.
//
// This header file, interface, and associated samples are:
//
// Copyright (C) 2015-2024 by Bitvise Limited
//
// You may use and modify this header file and associated samples free of
// charge for the purpose of implementing a program compatible with Bitvise
// SSH Server's virtual filesystem. You may freely distribute original
// versions of this header file and associated samples. You may freely
// distribute modified versions, as long as you include this preamble and
// license without modification, and make it clear that the code is modified.
// You may freely use and distribute compiled code that is built with our
// original or modified header file and/or samples. If you develop a program
// that uses this interface, you may set your own terms for other parties'
// use of such a program. Use of your program, whether for free or paid-for,
// does not convey to anyone the right to use Bitvise SSH Server: this must
// be licensed independently from Bitvise.
//
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
//

#pragma once

#ifdef COMPILING_SFSDLL
#define SFSDLL_FUNC extern "C" __declspec(dllexport)
#else
#define SFSDLL_FUNC extern "C" __declspec(dllimport)
#ifdef _WIN64
#pragma comment(lib, "SfsDll64.lib") 
#else
#pragma comment(lib, "SfsDll32.lib") 
#endif
#endif


// SFS protocol: enumerations

struct SfsDllAceFlag { enum {
    FileInherit				= 0x00000001,
    DirectoryInherit		= 0x00000002,
    NoPropagateInherit		= 0x00000004,
    InheritOnly				= 0x00000008,
    SuccessfulAccess		= 0x00000010,
    FailedAccess			= 0x00000020,
    IdentifierGroup			= 0x00000040,
	Inherited				= 0x01000000, }; };

struct SfsDllAceMask { enum {
    Read					= 0x00000001,
    Write					= 0x00000002,
    Append					= 0x00000004,
    ReadNamedAttrs			= 0x00000008,
    WriteNamedAttrs			= 0x00000010,
    Execute					= 0x00000020,
    DeleteChild				= 0x00000040,
    ReadAttrs				= 0x00000080,
    WriteAttrs				= 0x00000100,
    Delete					= 0x00010000,
    ReadAcl					= 0x00020000,
    WriteAcl				= 0x00040000,
    WriteOwner				= 0x00080000,
    Synchronize				= 0x00100000,
	AccessAuditAlarm		= 0x01000000, }; };

struct SfsDllAceType { enum {
	AccessAllowed			= 0,
	AccessDenied			= 1,
	SystemAudit				= 2,
	SystemAlarm				= 3, }; };

struct SfsDllAclCap { enum {
    Allow					= 0x0001,
    Deny					= 0x0002,
    Audit					= 0x0004,
    Alarm					= 0x0008,
    InheritAccess			= 0x0010,
	InheritAuditAlarm		= 0x0020, }; };

struct SfsDllAclFlag { enum {
    ControlIncluded			= 0x0001,
    ControlPresent			= 0x0002,
    ControlInherited		= 0x0004,
    AuditAlarmIncluded		= 0x0010,
	AuditAlarmInherited		= 0x0020, }; };

struct SfsDllAttr { enum {
    Size					= 0x000001,
    Permissions				= 0x000004,
    AccessTime				= 0x000008,
    CreateTime				= 0x000010,
    ModifyTime				= 0x000020,
    Acl						= 0x000040,
    OwnerGroup				= 0x000080,
    Subseconds				= 0x000100,
    Bits					= 0x000200,
    AllocSize				= 0x000400,
    TextHint				= 0x000800,
    MimeType				= 0x001000,
    LinkCount				= 0x002000,
	CTime					= 0x008000, }; };

struct SfsDllType { enum {
    Regular					= 1,
    Directory				= 2,
    Symlink					= 3,
    Special					= 4,
    Unknown					= 5,
    Socket					= 6,
    CharDevice				= 7,
    BlockDevice				= 8,
	Fifo					= 9, }; };

struct SfsDllAttrBit { enum {
    ReadOnly				= 0x0001,
    System					= 0x0002,
    Hidden					= 0x0004,
    CaseInsensitive			= 0x0008,
    Archive					= 0x0010,
    Encrypted				= 0x0020,
    Compressed				= 0x0040,
    Sparse					= 0x0080,
    AppendOnly				= 0x0100,
    Immutable				= 0x0200,
	Sync					= 0x0400 }; };

struct SfsDllTextHint { enum {
    KnownText				= 0,
    GuessedText				= 1,
    KnownBinary				= 2,
	GuessedBinary			= 3, }; };

struct SfsDllOpenFlag { enum {
	AccessDispositionMask	= 0x00000007,
		CreateNew				= 0,	// AccessDisposition
		CreateTruncate			= 1,	// AccessDisposition	
		OpenExisting			= 2,	// AccessDisposition
		OpenOrCreate			= 3,	// AccessDisposition
		TruncateExisting		= 4,	// AccessDisposition
    Append					= 0x00000008,
    AppendAtomic			= 0x00000010,
    TextMode				= 0x00000020,
    BlockRead				= 0x00000040,
    BlockWrite				= 0x00000080,
    BlockDelete				= 0x00000100,
    BlockAdvisory			= 0x00000200,
    NoFollow				= 0x00000400,
    DeleteOnClose			= 0x00000800,
    AccessAuditAlarmInfo	= 0x00001000,
    AccessBackup			= 0x00002000,
    BackupStream			= 0x00004000,
	OverrideOwner			= 0x00008000,
	BlockDefault			= 0x10000000, }; }; // Ignore BlockRead/Write/Delete and instead let the server use the defaults

struct SfsDllPermission { enum {
	WorldExecute			= 0x0001,
	WorldWrite				= 0x0002,
	WorldRead				= 0x0004,
	GroupExecute			= 0x0008,
	GroupWrite				= 0x0010,
	GroupRead				= 0x0020,
	OwnerExecute			= 0x0040,
	OwnerWrite				= 0x0080,
	OwnerRead				= 0x0100,
	SaveTextImage			= 0x0200,
	SetGuid					= 0x0400,
	SetUid					= 0x0800, }; };

struct SfsDllControlByte { enum {
	NoCheck					= 1,
	StatIf					= 2,
	StatAlways				= 3, }; };

struct SfsDllRenameFlag { enum {
    Overwrite				= 0x01,
    Atomic					= 0x02,
	Native					= 0x04, }; };

struct SfsDllRequestType { enum {
    Open					= 3,
    Close					= 4,
    Read					= 5,
    Write					= 6,
    FStat					= 8,
    SetStat					= 9,
    FSetStat				= 10,
    OpenDir					= 11,
    ReadDir					= 12,
    Remove					= 13,
    MkDir					= 14,
    RmDir					= 15,
    RealPath				= 16,
    Stat					= 17,
    Rename					= 18,
    ReadLink				= 19,
    Link					= 21,
    Block					= 22,
    Unblock					= 23,
    TextSeek				= 1001,
    CheckFileHandle			= 2001,
    CheckFileName			= 2002,
    SpaceAvail				= 2003,
	FileCopy				= 2005,
	DataCopy				= 2006,
	NativePath				= 3001,
	PosixPerm				= 9001,
	// Added in version 2
	ClientVersion			= 9002,
	// Added in version 3
	ChangePassword			= 9003, }; }; 

struct SfsDllResponseType { enum {
    Status					= 101,
    Handle					= 102,
    Data					= 103,
    Names					= 104,
    Attrs					= 105,
    Name					= 1101,
    CheckFileReply			= 2101,
	SpaceAvailReply			= 2102,
	PosixPermReply			= 9101,
	// Added in version 2
	ClientVersionReply		= 9102, 
	

}; };

struct SfsDllStatusCode { enum {
    Ok						= 0,
    Eof						= 1,
    NoSuchFile				= 2,
    PermissionDenied		= 3,
    Failure					= 4,
    BadMessage				= 5,
    NoConnection			= 6,
    ConnectionLost			= 7,
    OpUnsupported			= 8,
    InvalidHandle			= 9,
    NoSuchPath				= 10,
    FileAlreadyExists		= 11,
    WriteProtect			= 12,
    NoMedia					= 13,
    NoSpaceOnFilesystem		= 14,
    QuotaExceeded			= 15,
    UnknownPrincipal		= 16,
    LockConflict			= 17,
    DirNotEmpty				= 18,
    NotADirectory			= 19,
    InvalidFilename			= 20,
    LinkLoop				= 21,
    CannotDelete			= 22,
    InvalidParameter		= 23,
    FileIsADirectory		= 24,
    ByteRangeLockConflict	= 25,
    ByteRangeLockRefused	= 26,
    DeletePending			= 27,
    FileCorrupt				= 28,
    OwnerInvalid			= 29,
    GroupInvalid			= 30,
    NoMatchingByteRangeLock	= 31 }; };


// SFS protocol: common structures
	
struct SfsDllAce
{
	unsigned int m_type;
    unsigned int m_flags;	// SfsDllAceFlag
    unsigned int m_mask;	// SfsDllAceMask
    wchar_t const* m_who;
};
 
struct SfsDllAcl
{
	unsigned int m_flags;
	unsigned int m_aceCount;
	SfsDllAce const* m_aceArray;
};
 
struct SfsDllAttrs
{
	unsigned int m_validAttrFlags;	// SfsDllAttr
	unsigned int m_type;			// SfsDllType
	unsigned __int64 m_size;
	unsigned __int64 m_allocSize;
	wchar_t const* m_owner;
	wchar_t const* m_group;
	unsigned int m_permissions;
	unsigned __int64 m_accessTime;
	unsigned int m_accessTimeNs;
	unsigned __int64 m_createTime;
	unsigned int m_createTimeNs;
	unsigned __int64 m_modifyTime;
	unsigned int m_modifyTimeNs;
	unsigned __int64 m_cTime;
	unsigned int m_cTimeNs;
	SfsDllAcl m_acl;
	unsigned int m_attrBits;		// SfsDllAttrBit
	unsigned int m_attrBitsValid;
	unsigned char m_textHint;		// SfsDllTextHint
	wchar_t const* m_mimeType;
	unsigned int m_linkCount;
};


// SFS protocol: request structures

struct SfsDllOpen	// Valid responses: SfsDllStatus, SfsDllHandle
{
	wchar_t const* m_fileName;
	unsigned int m_desiredAccess;	// SfsDllAceMask
	unsigned int m_flags;			// SfsDllOpenFlag
	SfsDllAttrs m_attrs;
};

struct SfsDllClose // Valid response: SfsDllStatus
{
	unsigned int m_handle;
};

struct SfsDllRead // Valid responses: SfsDllStatus, SfsDllData
{
	unsigned int m_handle;
	unsigned __int64 m_offset;
	unsigned int m_length;			// Reading chunks up to 4 MB
};

struct SfsDllWrite // Valid response: SfsDllStatus
{
	unsigned int m_handle;
	unsigned __int64 m_offset;
    unsigned int m_dataSize;
	unsigned char const* m_dataPtr;	// m_dataPtr[0-m_dataSize)
};

struct SfsDllFStat // Valid responses: SfsDllStatus, SfsDllAttrs
{
	unsigned int m_handle;
	unsigned int m_flags;	// SfsDllAttr
};

struct SfsDllSetStat // Valid response: SfsDllStatus
{
	wchar_t const* m_path;
    SfsDllAttrs m_attrs;
};

struct SfsDllFSetStat // Valid response: SfsDllStatus
{
	unsigned int m_handle;
	SfsDllAttrs m_attrs;
};

struct SfsDllOpenDir // Valid responses: SfsDllStatus, SfsDllHandle
{
	 wchar_t const* m_path;
};

struct SfsDllReadDir // Valid responses: SfsDllStatus, SfsDllNames
{
	 unsigned int m_handle;
};

struct SfsDllRemove // Valid response: SfsDllStatus
{
	wchar_t const* m_fileName;
};

struct SfsDllMkDir // Valid response: SfsDllStatus
{
	wchar_t const* m_path;
	SfsDllAttrs m_attrs;
};
 
struct SfsDllRmDir // Valid response: SfsDllStatus
{
	wchar_t const* m_path;
};

struct SfsDllRealPath // Valid responses: SfsDllStatus, SfsDllName
{
	wchar_t const* m_path;
	unsigned char m_controlByte;	// SfsDllControlByte
};

struct SfsDllStat // Valid responses: SfsDllStatus, SfsDllAttrs
{
	wchar_t const* m_path;
	unsigned int m_flags;	// SfsDllAttr
	bool m_followSymlink;	// Set false for LStat.
};

struct SfsDllRename // Valid response: SfsDllStatus
{
	wchar_t const* m_oldPath;
	wchar_t const* m_newPath;
	unsigned int m_flags; // SfsDllRenameFlag
};

struct SfsDllReadLink // Valid responses: SfsDllStatus, SfsDllName
{
	wchar_t const* m_path;
};

struct SfsDllLink // Valid response: SfsDllStatus
{
   wchar_t const* m_newLinkPath;
   wchar_t const* m_existingPath;
   bool m_symlink;
};

struct SfsDllBlock // Valid response: SfsDllStatus
{
	unsigned int m_handle;
	unsigned __int64 m_offset;
	unsigned __int64 m_length;
	unsigned int m_flags;	// SfsDllOpenFlag::Block*
};

struct SfsDllUnblock // Valid response: SfsDllStatus
{
	unsigned int m_handle;
	unsigned __int64 m_offset;
	unsigned __int64 m_length;
};

struct SfsDllTextSeek // Valid response: SfsDllStatus
{
	unsigned int m_handle;
	unsigned __int64 m_lineNumber;
};

struct SfsDllCheckFileHandle // Valid responses: SfsDllStatus, SfsDllCheckFileReply
{
	unsigned int m_handle;
	wchar_t const* m_hashAlgList;
	unsigned __int64 m_startOffset;
	unsigned __int64 m_length;
	unsigned int m_blockSize;
};
 
struct SfsDllCheckFileName // Valid responses: SfsDllStatus, SfsDllCheckFileReply
{
	wchar_t const* m_fileName;
	wchar_t const* m_hashAlgList;
	unsigned __int64 m_startOffset;
	unsigned __int64 m_length;
	unsigned int m_blockSize;
};

struct SfsDllSpaceAvail // Valid responses: SfsDllStatus, SfsDllSpaceAvailReply
{
	wchar_t const* m_path;
};

struct SfsDllFileCopy // Valid response: SfsDllStatus
{
	wchar_t const* m_srcFileName;
	wchar_t const* m_dstFileName;
	bool m_overwrite; // Overwrite destination file if exists.
};

struct SfsDllDataCopy // Valid response: SfsDllStatus
{
	unsigned int m_readHandle;
	unsigned __int64 m_readOffset;
    unsigned __int64 m_readLength; // Read until EOF if 0.
    unsigned int m_writeHandle;
    unsigned __int64 m_writeOffset;
};

struct SfsDllNativePath // Valid responses: SfsDllStatus, SfsDllName
{
	bool m_fromHandle;
	unsigned int m_handle;	// Valid if m_fromHandle == true
	wchar_t const* m_path;	// Valid if m_fromHandle == false
};

struct SfsDllPosixPerm // Valid responses: SfsDllStatus, SfsDllPosixPermReply
{
	// Requesting POSIX permissions for directories and files
	// as configured in BvSshServer for the logged on user.
};

struct SfsDllClientVersion // Valid responses: SfsDllStatus, SfsDllClientVersionReply; Added in version 2
{
	// Retrieving client SSH version string.
};

struct SfsDllChangePassword // Valid response: SfsDllStatus; Added in version 3
{
	wchar_t const* m_curPassword;
	wchar_t const* m_newPassword;
};

struct SfsDllRequest
{
	unsigned int m_type;	// SfsDllRequestType
	void const* m_content;
};

struct SfsDllRequestEx // Added in version 3
{
	unsigned int m_type;	// SfsDllRequestType
	void const* m_content;
	bool m_internal;		// Logging is downgraded for internal requests
};


// SFS protocol: response structures

struct SfsDllStatus
{
	unsigned int m_statusCode;		// SfsDllStatusCode
	wchar_t const* m_errorMessage;	// May be null
	wchar_t const* m_languageTag;	// May be null
};

struct SfsDllHandle
{
	unsigned int m_handle;
	bool m_createdNewFile;
};
 
struct SfsDllData
{
	unsigned int m_dataSize;
	unsigned char const* m_dataPtr;
	bool m_endOfFile;
};

struct SfsDllName
{
	wchar_t const* m_fileName;
	SfsDllAttrs m_attrs;
};

struct SfsDllNames
{
	unsigned int m_nameCount;
	SfsDllName const* m_nameArray;	// m_nameArray[0-m_nameCount)
	bool m_endOfList;
};

struct SfsDllCheckFileReply
{
	wchar_t const* m_hashAlgUsed;
	unsigned int m_hashDataSize;
	unsigned char const* m_hashDataPtr;	// m_hashDataPtr[0-m_hashDataSize)
};
  
struct SfsDllSpaceAvailReply
{
	unsigned __int64 m_bytesOnDevice;
	unsigned __int64 m_unusedBytesOnDevice;
	unsigned __int64 m_bytesAvailableToUser;
	unsigned __int64 m_unusedBytesAvailableToUser;
	unsigned int m_bytesPerAllocationUnit;
};

struct SfsDllPosixPermReply
{
	// Returing POSIX permissions for directories and files
	// as configured in BvSshServer for the logged on user.
	unsigned short m_posixPermDir;
	unsigned short m_posixPermFile;
};

struct SfsDllClientVersionReply // Added in version 2
{
	wchar_t const* m_versionUnsanitized;
};
  
struct SfsDllResponse
{
	unsigned int m_type;	// SfsDllResponseType
	void const* m_content;			
};


// SFS exception handling

struct SfsDllExceptionType { enum {
	Unrecognized	= 0,
	Standard		= 1,
	BadAlloc		= 2,
	WinApi			= 3,
	Flow			= 4, }; };

struct SfsDllExceptionOrigin { enum {
	InCall		= 0,
	InSession	= 1, }; };

struct SfsDllWinApiError
{
	wchar_t const* m_function;
	unsigned int m_lastError;
};

struct SfsDllFlowError
{
	wchar_t const* m_component;
	unsigned int m_cls;
	unsigned int m_code;
};

struct SfsDllException
{
	unsigned int m_type;		// SfsDllExceptionType
	wchar_t const* m_desc;
	union // auxilary info
	{
		SfsDllWinApiError m_winApiError;	// if m_type == SfsDllExceptionType::WinApi
		SfsDllFlowError m_flowError;		// if m_type == SfsDllExceptionType::Flow
	};
};

typedef void (__cdecl* SfsDllExceptionHandlerType)(void* handlerData, SfsDllException const* exception);


// SFS event handling

struct SfsDllEventType { enum {
	ServerDisconnect	= 1,
	TerminalException	= 2, }; };

struct SfsDllEvent
{
	unsigned int m_type;		// SfsDllEventType
	union // auxilary info
	{
		SfsDllException m_terminalException; // if m_type == SfsDllEventType::TerminalException
	};
};

typedef void (__cdecl* SfsDllEventHandlerType)(void* handlerData, SfsDllEvent const* event_);


// SFS handlers

struct SfsDllHandlers
{
	// Exception handler can be called only during execution of any
	// of the three SFS functions declared at the end of this file.
	// Exception handler is called in the same thread that's executing
	// the exported function; exception handler may throw exceptions.
	SfsDllExceptionHandlerType m_exceptionHandler; 
	void* m_exceptionHandlerData;

	// Event handler is called for events occurring in the SfsDll's
	// worker thread. Event handler should not throw exceptions, or
    // else SfsDll will terminate the process with exit code 5996.
	SfsDllEventHandlerType m_eventHandler;
	void* m_eventHandlerData;
};


// SFS functions

// SFS DLL must be initialized before first use in each process.
// Once successfully initialized, any further initialization
// attempts will result in failure. On failure, exception handler
// will be called before returning false.
SFSDLL_FUNC bool __cdecl SfsDllInitialize(SfsDllHandlers const* handlers);

// SFS functionality is accessed through this function. The function
// will take an SFS request, block until result is ready, and then return
// a newly allocated SFS result. Use SfsDllFree() to deallocate it.
// On failure, exception handler will be called before returning null.
SFSDLL_FUNC SfsDllResponse const* __cdecl SfsDllIssue(SfsDllRequest const* request);
SFSDLL_FUNC SfsDllResponse const* __cdecl SfsDllIssueEx(SfsDllRequestEx const* request);

// SfsDllResponse returned from SfsDllIssue() must be freed with this
// function, when it's no longer needed.
SFSDLL_FUNC void __cdecl SfsDllFree(SfsDllResponse const* response);

// Returns the version of SFS DLL file. Version is increased each
// time a change is made. All changes must be backward compatible.
// SfsDllVersion() may be called without initializing SFS DLL first.
SFSDLL_FUNC unsigned int __cdecl SfsDllVersion();

enum { CurrentSfsDllVersion = 3 };

// CHANGELOG:
//
// Version 3 (Bitvise SSH Server X.XX) //!TODO
// - Added functionality for changing password.
//
// Version 2 (Bitvise SSH Server 7.12):
// - Added SfsDllIssueEx function. 
// - Added functionality for retriving client's SSH version string.
//
// Version 1 (Bitvise SSH Server 6.41):
// - Initial release.
