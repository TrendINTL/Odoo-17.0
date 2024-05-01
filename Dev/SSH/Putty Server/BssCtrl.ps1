Param (
    [string]$version  = $(throw "-version is a required parameter. Valid usage: -version `"915`""),
    [string]$instanceName = ""
)

# Help strings:
$initError = @"
Failed to instantiate the BssCfg$version COM object.
Command:
`$cfg = new-object -com `"Bitvise.BssCfg.$version`"
"@

$instanceWarning = @"
The Bitvise SSH Server instance could not be set. Could not load settings.
"@

$instanceWarningDesc = @"
Execute the following command for more details:
`$cfg.SetInstance(`"$instanceName`")
"@

$lockWarning = @"
The Bitvise SSH Server settings file could not be locked.
"@

$lockWarningPostfix = @"
Settings cannot be modified until the file is unlocked.
"@

$loadDesc = @"
Bitvise SSH Server settings could not be loaded.
"@

$loadError = @"
Bitvise SSH Server settings could not be loaded.
Execute the following command for more details:
`$cfg.settings.Load()
"@

$help = @"
To modify or query Bitvise SSH Server settings, use the "`$cfg" object.
Use the Tab key to auto-complete member names. Use `$cfg.help for help.

In this window, `$cfg is already initialized. To create it in other scripts and windows, use:
`$cfg = new-object -com `"Bitvise.BssCfg`"

Examples:

`$cfg.settings.help
  Shows the properties and methods of the settings object along with help text.
  Every settings object in the hierarchy has a "help" member.

`$cfg.settings.SetDefaults()
  Resets the entire settings hierarchy to default values.

`$cfg.settings.Dump()
  Displays the current values of the settings hierarchy.
  The output uses PowerShell-compatible syntax. It can be run to configure the same settings.
  The output format can also be imported using 'BssCfg settings importText'.

`$cfg.settings.logging.DumpEx(`$cfg.enums.showDefaults.yes)
  Displays current values in the logging section of settings.
  The showDefaults parameter controls whether default values are shown or omitted.

`$cfg.settings.server.preferredIpVersion = `$cfg.enums.IpVersionType.ipv4
  Sets the IP version for outgoing connections to IPv4.

`$cfg.settings.access.winAccounts.entries[0].winAccount
  Prints the first Windows account name from the Windows account list.

foreach (`$acct in `$cfg.settings.access.virtAccounts.entries) { Write-Host `$acct.virtAccount }
  Queries all virtual account names.

`$cfg.settings.access.virtAccounts.new.virtAccount = "TestAccount"
`$cfg.settings.access.virtAccounts.new.group = "Virtual Users"
`$cfg.settings.access.virtAccounts.NewCommit()
  Create a new virtual account named "TestAccount". Set the properties of the new entry using
  the "new" member. The NewCommit() method adds the new entry to the list of virtual accounts.

`$a = `$cfg.settings.access.virtAccounts.FirstWhere1("virtAccount eq ?", "TemplateAccount")
Invoke-Expression `$a.Dump()
`$cfg.settings.access.virtAccounts.new.virtAccount = "NewAccount"
`$cfg.settings.access.virtAccounts.NewCommit()
  Find existing virtual account named "TemplateAccount". Dump() lists PowerShell commands
  which reproduce settings for this account. Apply them using Invoke-Expression. Set the
  account name and commit the new entry. The new account is now a copy of "TemplateAccount".

IMPORTANT! To save changes before closing this window, use the command:
`$cfg.settings.Save()`n
"@

$warningsDesc = @"
Configuration contains invalid Bitvise SSH Server settings.

"@

# Initialize the BssCfg com object
Try
{
	$cfg = new-object -com "Bitvise.BssCfg.$version"
	$cfg.CleanUpOnProcessExit($pid)
}
Catch
{
	Write-Error $initError
	Exit
}

# Print help
Write-Host $help

# Try to set the instance. If it fails, settings cannot be loaded
Try
{
	If ($instanceName)
	{
		$cfg.SetInstance($instanceName)
	}
}
Catch
{
	$msg = $instanceWarning + "`n"
	If ([string]::IsNullOrEmpty($_.Exception.Message))
	{
        $msg += $instanceWarningDesc;
	}
    Else
	{
        $msg += $_.Exception.Message;
	}

	Write-Warning $msg

	Exit
}

# Try to lock server settings. If it fails, settings can still be loaded
Try
{
	$cfg.settings.Lock()
}
Catch
{
	$msg = ""
	If ([string]::IsNullOrEmpty($_.Exception.Message))
	{
        $msg += $lockWarning;
	}
    Else
	{
        $msg += $_.Exception.Message;
	}

    $msg += "`n" + $lockWarningPostfix;
    Write-Warning $msg
}

# Try to load server settings.
Try
{
	$loadResult = $cfg.settings.TryLoad()
    $loadResultDesc = $loadResult.Describe()
    If ($loadResult.loadError)
    {
	    Write-Error $loadDesc$loadResultDesc
    }
	ElseIf ($loadResult.Describe())
	{
		Write-Warning $warningsDesc$loadResultDesc
	}
}
Catch
{
	Write-Error $loadError
}
