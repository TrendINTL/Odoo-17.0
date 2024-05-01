<#
.DESCRIPTION
    This script imports virtual account entries from a CSV file into Bitvise SSH Server settings.
    The PowerShell instance executing this script needs to run elevated, as administrator, to access
    SSH Server settings.

.PARAMETER fileName
    Specifies the path to the CSV file which contains the information about the virtual accounts you
    would like to import.

.PARAMETER delimiter
    Specifies the delimiter that separates the values in the CSV file. The default value is the list
    separator character configured in Windows (typically ",").

.Parameter encoding
    Specifies the type of character encoding that was used in the CSV file. Valid values are Unicode,
    UTF7, UTF8, ASCII, UTF32, BigEndianUnicode, Default and OEM.

    This parameter is only available if Windows PowerShell 3.0 or newer is used.

.Parameter defaultGroup
    Specifies the default virtual group for the newly created accounts.
    IMPORTANT: This virtual group must already exist, or has to be created manually before the script is executed.

.Parameter layout
    Specifies the layout of the CSV file.

.PARAMETER instance
    The name of the SSH Server instance. Only necessary if more than one instance is installed.
    
.EXAMPLE
    PS C:\>.\VirtAccountImporter.ps1 virtAccounts.csv
    
    Imports virtual accounts listed in the virtAccounts.csv file.

    Data in the CSV file:
    SheilaB,password,C:\Path\To\Sftp\Root
    Jones,password,C:\Path\To\Sftp\Root

.EXAMPLE
    PS C:\>.\VirtAccountImporter.ps1 -fileName virtAccounts.csv -defaultGroup "Imported Virtual Users"
    
    Imports virtual accounts listed in the virtAccounts.csv file. Newly created accounts will be associated with the
    "Imported Virtual Users" group.

    IMPORTANT: The virtual group "Imported Virtual Users" must already exist, or has to be created manually before the script is executed.

.EXAMPLE
    PS C:\>.\VirtAccountImporter.ps1 -fileName virtAccountsUTF8.csv -delimiter ";" -encoding UTF8 -layout @("sftpRootDirectory", "username", "password")

    Imports virtual accounts listed in the virtAccounts.csv file. Since the file uses a different layout, delimiter,
    and encoding, it has to be defined explicitly.

    Data in the CSV file:
    C:\Path\To\Sftp\Root;SheilaB;password
    C:\Path\To\Sftp\Root;Jones;password

#>

####################################
# Defining command line parameters
####################################
Param (
    [Parameter(Mandatory = $true,
        HelpMessage = "Path to the CSV file with the new virtual accounts.")]
    [string]
    $fileName,
    
    [Parameter(HelpMessage = "Delimiter character.")]
    [string]
    $delimiter = (Get-Culture).TextInfo.ListSeparator,

    [Parameter(HelpMessage = "Encoding of the file.")]
    [string]
    [ValidateSet("Unicode", "UTF7", "UTF8", "ASCII", "UTF32", "BigEndianUnicode", "Default", "OEM")]
    $encoding = "ASCII",
    
    [Parameter(HelpMessage = "Default group.")]
    [string]
    $defaultGroup = "Virtual Users",
    
    [Parameter(HelpMessage = "CSV layout.")]
    [string[]]
    $layout = @("username", "password", "sftpRootDirectory"),
    
    [Parameter(HelpMessage = "Bitvise SSH Server instance.")]
    [string]
    $instance = ""
)


$unlock = $false
Try
{
    ####################################
    # Initialization
    ####################################

    $processState = "Initializing COM object"
    $cfg = new-object -com "Bitvise.BssCfg.915"

    $processState = "Setting SSH Server instance"
    If ($PSBoundParameters.ContainsKey("instance"))
    { $cfg.SetInstance($instance) }
    ElseIf ($cfg.instances.current -eq $null)
    { Throw "Multiple SSH Server instances are installed. Use the -instance parameter to select the instance." }

    $processState = "Locking SSH Server settings"
    $cfg.settings.Lock()
    $unlock = $true

    $processState = "Loading SSH Server settings"
    $cfg.settings.Load()

    $processState = "Reading the CSV file"
    If ($PSVersionTable.PSVersion.Major -ge 3)
    { $data = Import-Csv $fileName -Delimiter $delimiter -Header $layout -Encoding $encoding }
    Else
    { $data = Import-Csv $fileName -Delimiter $delimiter -Header $layout }


    ####################################
    # Processing data
    ####################################

    $processState = "Processing data"
    foreach ($line in $data)
    {
        Try
        {
            ####################################
            # Creating new virtual accounts
            ####################################
            #
            # Modify the script here if you would like to set other members of the virtual account according to the CSV file.
            #
            # For example, to set a local Windows account name to be used as security context for the virtual account,
            #   add the following lines after this comment:
            #     $cfg.settings.access.virtAccounts.new.securityContext = $cfg.enums.SecurityContextWD.localAccount
            #     $cfg.settings.access.virtAccounts.new.winAccount = $line.winUser
            #
            #   IMPORTANT: to use the newly added lines, the layout parameter has to be specified, for example:
            #     PS C:\>.\VirtAccountImporter.ps1 virtAccounts.csv -layout @("username", "password", "sftpRootDirectory", "winUser")

            $cfg.settings.access.virtAccounts.new.virtAccount = $line.username
            $cfg.settings.access.virtAccounts.new.virtPassword.Set($line.password)
            $cfg.settings.access.virtAccounts.new.group = $defaultGroup

            $cfg.settings.access.virtAccounts.new.xfer.mountPoints.Clear()
            If ([string]::IsNullOrEmpty($line.sftpRootDirectory))
            {
                $cfg.settings.access.virtAccounts.new.xfer.mountPoints.new.allowUnlimitedAccess = $true
            }
            Else
            {
                $cfg.settings.access.virtAccounts.new.xfer.mountPoints.new.allowUnlimitedAccess = $false
                $cfg.settings.access.virtAccounts.new.xfer.mountPoints.new.realRootPath = $line.sftpRootDirectory
            }
            $cfg.settings.access.virtAccounts.new.xfer.mountPoints.NewCommit()

            $cfg.settings.access.virtAccounts.NewCommit()

            Write-Host "Created account: `"$($line.username)`""
        }
        Catch
        {
            Write-Host "Failed to create account: `"$($line.username)`"" -ForegroundColor Red
            Write-Host "    Exception Type: $($_.Exception.GetType().FullName)" -ForegroundColor Red
            Write-Host "    Exception Message: $($_.Exception.Message)" -ForegroundColor Red
        }
    }


    ####################################
    # Saving and unlocking
    ####################################

    $processState = "Saving SSH Server settings"
    $cfg.settings.Save()

    $processState = "Unlocking SSH Server settings"
    $cfg.settings.Unlock()
    $unlock = $false
}
Catch
{
    Write-Host "$processState has failed." -ForegroundColor Red
    Write-Host "    Exception Type: $($_.Exception.GetType().FullName)" -ForegroundColor Red
    Write-Host "    Exception Message: $($_.Exception.Message)" -ForegroundColor Red
}

If ($unlock) { $cfg.settings.Unlock() }
