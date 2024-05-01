<#
.DESCRIPTION
    This script exports virtual account entries from Bitvise SSH Server settings into a CSV format.
    The PowerShell instance executing this script needs to run elevated, as administrator, to access
    SSH Server settings.

.PARAMETER fileName
    Specifies the path to the CSV output file. If it's not provided, standard output is used to show the result.

.PARAMETER delimiter
    Specifies the delimiter that separates the values in the CSV file. The default value is the list
    separator character configured in Windows (typically ",").

.Parameter encoding
    Specifies the encoding for the exported CSV file. Valid values are Unicode,
    UTF7, UTF8, ASCII, UTF32, BigEndianUnicode, Default and OEM.

.Parameter layout
    Specifies the layout of the output.

.PARAMETER instance
    The name of the SSH Server instance. Only necessary if more than one instance is installed.

.EXAMPLE
    PS C:\>.\VirtAccountExporter.ps1

    Prints virtual account information to the standard output.

    Result:
    SheilaB,Virtual Users,C:\Path\To\Sftp\Root,
    Jones,Virtual Users,<multiple mount points>,

.EXAMPLE
    PS C:\>.\VirtAccountExporter.ps1 virtAccounts.csv

    Exports virtual account information into the virtAccounts.csv file.

    Exported data:
    SheilaB,Virtual Users,C:\Path\To\Sftp\Root,
    Jones,Virtual Users,<multiple mount points>,

.EXAMPLE
    PS C:\>.\VirtAccountExporter.ps1 -layout @("virtAccount", "winAccount", "group", "sftpRootDirectory", "xfer.mountPoints.count")

    Prints virtual account information to the standard output. It additionally exports the local Windows account
    name (it might be empty) and the number of configured mount points.

    Result:
    SheilaB,SheilaB,Virtual Users,C:\Path\To\Sftp\Root,1,
    Jones,,Virtual Users,<multiple mount points>,2,

.EXAMPLE
    PS C:\>.\VirtAccountExporter.ps1 -fileName virtAccountsUTF8.csv -delimiter ";" -encoding UTF8 -layout @("sftpRootDirectory", "virtAccount", "group")

    Exports virtual account information into the virtAccountsUTF8.csv file with UTF8 encoding. Since the file uses a
    different layout, delimiter, and encoding, it has to be defined explicitly.

    Data in the CSV file:
    C:\Path\To\Sftp\Root;SheilaB;Virtual Users;
    <multiple mount points>;Jones;Virtual Users;

#>

####################################
# Defining command line parameters
####################################
Param (
    [Parameter(HelpMessage = "Path to the CSV output file.")]
    [string]
    $fileName,
    
    [Parameter(HelpMessage = "Delimiter character.")]
    [string]
    $delimiter = (Get-Culture).TextInfo.ListSeparator,

    [Parameter(HelpMessage = "Encoding of the output file.")]
    [string]
    [ValidateSet("Unicode", "UTF7", "UTF8", "ASCII", "UTF32", "BigEndianUnicode", "Default", "OEM")]
    $encoding = "ASCII",
    
    [Parameter(HelpMessage = "CSV layout.")]
    [string[]]
    $layout = @("virtAccount", "group", "sftpRootDirectory"),
    
    [Parameter(HelpMessage = "Bitvise SSH Server instance.")]
    [string]
    $instance = ""
)


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

    $processState = "Loading SSH Server settings"
    $cfg.settings.Load()

    ####################################
    # Processing virtual accounts
    ####################################

    $processState = "Processing virtual accounts"
    $result = ""
    Foreach ($virtAccount in $cfg.settings.access.virtAccounts.entries)
    {
        Try
        {
            $line = ""
            Foreach ($command in $layout)
            {
                ####################################
                # Preparing virtual account information
                ####################################
                #
                # Modify the script here if you would like to export other information in the virtual account to the CSV file.
                #
                # For simple export of the virtual account members the layout parameter can be used.
                # For example, to export the local Windows account name that provides the security context for the virtual account, 
                #   execute the script with the following parameters:
                #     PS C:\>.\VirtAccountExporter.ps1 -layout @("virtAccount", "group", "sftpRootDirectory", "winAccount")
                #
                # For more complex processing of a virtual account's data before export, you can add a new ElseIf block to the If below.
                # For example, to export the first shared folder's remote directory (if it exists), use the following ElseIf block:
                #
                #   ElseIf ($command -eq "firstRemoteDir")
                #   {
                #       If ($virtAccount.session.shares.count -gt 0)
                #       {
                #           $line += $virtAccount.session.shares.GetItem(0).remoteDir
                #       }
                #   }
                #
                #   IMPORTANT:
                #     - As shown in the above example, a unique layout name needs to be specified: "firstRemoteDir"
                #     - to use the newly added lines, the layout parameter has to be specified correctly
                #       PS C:\>.\VirtAccountExporter.ps1 -layout @("virtAccount", "group", "sftpRootDirectory", "firstRemoteDir")

                If ($command -eq "sftpRootDirectory")
                {
                    If ($virtAccount.xfer.mountPoints.count -eq 1)
                    {
                        If (!$virtAccount.xfer.mountPoints.GetItem(0).allowUnlimitedAccess)
                        {
                            $line += $virtAccount.xfer.mountPoints.GetItem(0).realRootPath
                        }
                    }
                    ElseIf ($virtAccount.xfer.mountPoints.count -gt 1)
                    {
                        $line += "<multiple mount points>"
                    }
                }
                ####################################
                # Add ElseIf here
                ####################################
                #ElseIf ($command -eq "firstRemoteDir")
                #{
                #    If ($virtAccount.session.shares.count -gt 0)
                #    {
                #        $line += $virtAccount.session.shares.GetItem(0).remoteDir
                #    }
                #}
                Else
                {
                    $line += Invoke-Expression "`$virtAccount.$command"
                }

                $line += $delimiter

            }

            $data += "$line`r`n"
        }
        Catch
        {
            Write-Host "Failed to export account: `"$($virtAccount.virtAccount)`"" -ForegroundColor Red
            Write-Host "    Exception Type: $($_.Exception.GetType().FullName)" -ForegroundColor Red
            Write-Host "    Exception Message: $($_.Exception.Message)" -ForegroundColor Red
        }
    }
    
    $processState = "Writing result"
    If ($fileName)
    {
        Out-File -filepath $fileName -inputobject $data -encoding $encoding
    }
    Else
    {
        Write-Host $data
    }
}
Catch
{
    Write-Host "$processState has failed." -ForegroundColor Red
    Write-Host "    Exception Type: $($_.Exception.GetType().FullName)" -ForegroundColor Red
    Write-Host "    Exception Message: $($_.Exception.Message)" -ForegroundColor Red
}
