<#
.DESCRIPTION
    Returns or prints information about installed Bitvise SSH Server instances.
	Copyright (C) 2019-2024 by Bitvise Limited.

.EXAMPLE
    PS C:\>.\InstalledBssVersions.ps1
    
    Prints information about installed SSH Server instances.

.EXAMPLE
    PS C:\>$instances = .\InstalledBssVersions.ps1
    
    Stores information about installed SSH Server instances in the $instances array.
#>



####################################
# Utility functions
####################################


Function GetVersionFromArray($version, $versionArray)
{
    ForEach ($v in $versionArray)
    {
        If ($version -ge $v)
        {
            return $v
        }
    }
}


Function GetCfgFormatVersion($key)
{
    $v = $key.GetValue("Version")
    $cfgFormatVersion = $key.GetValue("CfgFormatVersion")
    If ([String]::IsNullOrEmpty($cfgFormatVersion) -and -not [String]::IsNullOrEmpty($v))
    {
        $cfgFormats = @(
            "7.21", "7.12", "6.45", "6.41", "6.31",
			"6.22", "6.21", "6.04", "6.03", "6.01",
			"6.00", "5.50", "5.22", "5.18", "5.17",
			"5.16", "5.11", "5.06", "5.02", "5.00",
			"4.24", "4.15", "4.12", "4.07", "4.03",
			"4.01", "3.95", "3.31", "3.30", "3.28",
			"3.27", "3.26", "3.25", "3.12", "3.09",
			"3.07", "3.01")

        $cfgFormatVersion = GetVersionFromArray -version $v -versionArray $cfgFormats
    }

    $cfgFormatVersion
}


Function GetCfgObjVersion($key)
{
    $v = $key.GetValue("Version")
    $cfgObjVersion = $key.GetValue("BssCfgManipVersion")
    If ([String]::IsNullOrEmpty($cfgObjVersion) -and -not [String]::IsNullOrEmpty($v))
    {
        $cfgObjVersions = @(
			"7.26", "7.21", "7.14", "7.12", "6.45",
			"6.41", "6.31", "6.22", "6.21", "6.05",
			"6.04", "6.03", "6.01", "5.54", "5.50",
			"5.23", "5.18", "5.11", "5.06", "5.02",
			"5.00", "4.24", "4.15", "4.12", "4.10")

        $cfgObjVersion = GetVersionFromArray -version $v -versionArray $cfgObjVersions
    }

    $cfgObjVersion
}


Function GetCfgObjName($cfgObjVersion)
{
    If (-not [String]::IsNullOrEmpty($cfgObjVersion))
    {
        $version = $cfgObjVersion -replace "[^0-9]", ""
        If ($cfgObjVersion -ge "9.12")
        {
            return "Bitvise.BssCfg.$version"
        }

        $cfgObjName = "WinsshdCfgManip$version"
        If ($cfgObjVersion -ge "5.50")
        {
            $cfgObjName = "BssCfg$version"
        }

        return "$cfgObjName.$cfgObjName"
    }
}


Function IsBitviseInstance($name)
{
    $instanceName = "Bitvise SSH Server"
    $instanceNamePrefix = "Bitvise SSH Server - "
    $oldInstanceName = "WinSSHD"
    $oldInstanceNamePrefix = "WinSSHD-"

    $name -like $instanceName -or
    $name -like $oldInstanceName -or
    $name.StartsWith($instanceNamePrefix, "CurrentCultureIgnoreCase") -or
    $name.StartsWith($oldInstanceNamePrefix, "CurrentCultureIgnoreCase")
}



####################################
# Main code
####################################


$uninstPath = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall"
If ([Environment]::Is64BitProcess)
{
    $uninstPath = "HKLM:\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall"
}

$keys = (Get-ChildItem $uninstPath)
$instances = @()

ForEach ($key in $keys)
{
    $keyName = Split-Path -Path $key.Name -Leaf
    If (IsBitviseInstance($keyName))
    {
        $cfgObjVersion = GetCfgObjVersion($key);
        $instances += (@([pscustomobject]@{
            name             = $keyName;
            version          = $key.GetValue("Version");
            versionInfo      = $key.GetValue("VersionInfo");
            cfgObjName       = GetCfgObjName($cfgObjVersion);
            cfgObjVersion    = $cfgObjVersion;
            cfgFormatVersion = GetCfgFormatVersion($key);
            installDir       = $key.GetValue("InstallDir")}))
    }
}

return $instances
