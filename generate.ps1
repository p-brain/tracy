
param(
    [string]$dest,
    [Switch]$help,
    [Switch]$keepGenerated
)


$cmakeProjects = @("ALL_BUILD", "INSTALL", "PACKAGE", "ZERO_CHECK")
$externDeps = @( "capstone", "freetype", "glfw", "rapidjson" )


function Cmake-GenerateProjects
{
    param(
        [string]$src
    )

    Write-Host "Generate the project files" -ForegroundColor DarkCyan

    $cmakeCommandParts = @(
        "cmake",
        "-B $src",
        "-S profiler",
        "-DCMAKE_CONFIGURATION_TYPES=`"Debug;Release`"",
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=`"MultiThreaded`$<`$<CONFIG:Debug>:Debug>`""
    )

    $cmakeCommand = $cmakeCommandParts -join ' '
    Write-Host $cmakeCommand
    Invoke-Expression $cmakeCommand

    $result = [bool]($LastExitCode -eq 0)
    if ( $result -eq $false ) {
        Write-Host "Cmake failed!" -ForegroundColor Red
    }

    Write-Host ""
    return [bool]$result
}


function Compile-Dependencies
{
    param(
        [string]$srcPath
    )
    Write-Host "Build the external dependencies" -ForegroundColor DarkCyan

    $depsPath = Join-Path -Path $srcPath -ChildPath "_deps"

    Push-Location $depsPath

    $commonBase = (Get-Item .).FullName

    $alldirs = Get-ChildItem -Path "." -Directory
    $dirs = $alldirs
    $buildDirs = Get-ChildItem -Path "." -Directory | Where-Object { $_.Name.EndsWith('-build')}

    $depProjectFiles = @()

    foreach ( $bd in $buildDirs ) {
        $checkDirs = Get-ChildItem -Path $bd -Directory -Recurse

        $depProj = $bd.Name.TrimEnd("-build")
        $depProjFile = [io.path]::ChangeExtension( $depProj, "vcxproj" )
        $checkPath = Join-Path -Path $bd.FullName -ChildPath $depProjFile

        if ( Test-Path -Path $checkPath ) {
            $relPath = $checkPath.Substring( $commonBase.Length ).TrimStart( '\' )
            $depProjectFiles += $relPath
        } else {
            foreach ( $dir in $checkDirs ) {
                $checkPath = Join-Path -Path $dir.FullName -ChildPath $depProjFile

                if ( Test-Path -Path $checkPath ) {
                    $relPath = $checkPath.Substring( $commonBase.Length ).TrimStart( '\' )
                    $depProjectFiles += $relPath
                }
            }
        }
    }

    foreach ( $dep in $depProjectFiles ) {
        $relPath = (Get-Item $dep).DirectoryName
        $file = (Get-Item $dep).Name
        Push-Location "$relPath"
        & 'msbuild' '/nologo' '/verbosity:quiet' '/p:Configuration=Release;Configuration=Debug' '/t:Rebuild' $file
        & 'msbuild' '/nologo' '/verbosity:quiet' '/p:Configuration=Release;Configuration=Release' '/t:Rebuild' $file
        Pop-Location
    }
    Pop-Location

    Write-Host ""
}


function Prep-Dependency()
{
    param(
        [string]$project,
        [string]$srcPath,
        [string]$dstPath
    )

    if ( $srcPath -eq $dstPath ) {
        Write-Host "Prep-Dependency: source and dest are the same, nothing to do" -ForegroundColor DarkYellow
        return
    }

    Write-Host "Preparing dependency " -ForegroundColor DarkCyan -NoNewline
    Write-Host "$project "  -ForegroundColor Green -NoNewline
    Write-Host "$srcPath -> $dstPath" -ForegroundColor DarkCyan

    $srcIncPath = Join-Path -Path "$srcPath" -ChildPath ( Join-Path -Path "_deps" -ChildPath "$project-src" | Join-Path -ChildPath "include" )
    $dstIncPath = Join-Path -Path "$dstPath" -ChildPath ( Join-Path -Path "_deps" -ChildPath "$project-src" | Join-Path -ChildPath "include" )

    if ( Test-Path -Path "$srcIncPath" -PathType Container ) {
        if ( -Not (Test-Path -Path "$dstIncPath" -PathType Container) ) {
            New-Item -Path "$dstIncPath" -ItemType Directory -Force | Out-Null
        }

        Copy-Item -Path "$srcIncPath\*" -Destination "$dstIncPath\" -Recurse -Container -Force
    }

    $srcLibPath = Join-Path -Path "$srcPath" -ChildPath ( Join-Path -Path "_deps" -ChildPath "$project-build" )
    $dstLibPath = Join-Path -Path "$dstPath" -ChildPath ( Join-Path -Path "_deps" -ChildPath "$project-build" )

    if ( Test-Path -Path "$srcLibPath\debug" -PathType Container ) {
        if ( -Not ( Test-Path -Path "$dstLibPath\debug" -PathType Container ) ) {
            New-Item -Path "$dstLibPath\debug" -ItemType Directory -Force | Out-Null
        }

        Copy-Item -Path "$srcLibPath\debug\*" -Destination "$dstLibPath\debug\" -Recurse -Container -Force
    } elseif ( Test-Path -Path "$srcLibPath\src\debug" -PathType Container ) {
        if ( -Not (Test-Path -Path "$dstLibPath\src\debug" -PathType Container) ) {
            New-Item -Path "$dstLibPath\src\debug" -ItemType Directory -Force | Out-Null
        }

        Copy-Item -Path "$srcLibPath\src\debug\*" -Destination "$dstLibPath\src\debug\" -Recurse -Container -Force
    }

    if ( Test-Path -Path "$srcLibPath\release" -PathType Container ) {
        if ( -Not ( Test-Path -Path "$dstLibPath\release" -PathType Container ) ) {
            New-Item -Path "$dstLibPath\release" -ItemType Directory -Force | Out-Null
        }

        Copy-Item -Path "$srcLibPath\release\*" -Destination "$dstLibPath\release\" -Recurse -Container -Force
    } elseif ( Test-Path -Path "$srcLibPath\src\release" -PathType Container ) {
        if ( -Not ( Test-Path -Path "$dstLibPath\src\release" -PathType Container ) ) {
            New-Item -Path "$dstLibPath\src\release" -ItemType Directory -Force | Out-Null
        }

        Copy-Item -Path "$srcLibPath\src\release\*" -Destination "$dstLibPath\src\release\" -Recurse -Container -Force
    }
}


function Copy-Dependencies
{
    param(
        [string]$srcPath,
        [string]$dstPath
    )

    Write-Host "Copy external includes and libs $srcPath -> $dstPath" -ForegroundColor DarkCyan

    if ( -Not (Test-Path -Path "$dstPath" -PathType Container) ) {
        New-Item -Path "$dstPath" -ItemType Directory -Force | Out-Null
    }

    foreach ( $proj in $externDeps ) {
        Prep-Dependency $proj $srcPath $dstPath
    }

    Write-Host ""
}


function Get-MatchingNodes {
    param(
        [System.Xml.XmlDocument]$xml,
        [System.Xml.XmlNamespaceManager]$nsManager,
        [string]$xpath,
        [string]$nodeName,
        [string]$attr,
        [string[]]$filters
    )

    if ( -Not ( [string]::IsNullOrEmpty( $nodeName ) ) ) {
        $xpath = $xpath + "/ns:$nodeName"
    }
    $childNodes = $xml.SelectNodes($xpath, $nsManager)

    $matchingNodes = @()

    foreach ($node in $childNodes) {
        if ($node -is [System.Xml.XmlNode]) {
            $attrVal = $node.GetAttribute($attr)

            if ( $attrVal -eq $null -or $filters -eq $null -or $filters.Count -eq 0 ) {
                $matchingNodes += $node
            } else {
                foreach ($filter in $filters) {
                    if ($attrVal -like "*$filter*") {
                        $matchingNodes += $node
                        break
                    }
                }
            }
        }
    }

    return $matchingNodes
}


function Print-MatchingNodes {
    param(
        [System.Xml.XmlNode[]]$nodes
    )

    foreach ($node in $nodes) {
        Write-Host "Matching Node:" -ForegroundColor DarkGreen
        Write-Host $node.OuterXml
    }
}


function Remove-MatchingNodes {
    param(
        [System.Xml.XmlNode[]]$nodes
    )

    foreach ($node in $nodes) {
        $node.ParentNode.RemoveChild($node) | Out-Null
    }
}


function Rewrite-Projects
{
    param(
        [string]$srcPath,
        [string]$dstPath,
        [string]$extFilter,
        [string]$intDir
    )

    $depsFilter = $( $cmakeProjects; $externDeps )

    $srcRoot = Join-Path -Path "$PSScriptRoot" -ChildPath ""
    $srcPathAbs = Join-Path -Path "$PSScriptRoot" -ChildPath $srcPath
    $dstPathAbs = Join-Path -Path $srcRoot -ChildPath $dstPath
    $srcLevels = $srcRoot.TrimEnd( '\' ).Split( '\' )
    $dstLevels = $dstPathAbs.TrimEnd( '\' ).Split( '\' )
    $depthDiff = $dstLevels.Count - $srcLevels.Count
    $backSlashRepl = "..\" * $depthDiff
    $slashRepl = "../" * $depthDiff

    Get-ChildItem "$srcPath" -Filter $extFilter | 
    Foreach-Object {
        $base = $_.Name.split(".",2)[0]

        if ( -Not ( $cmakeProjects -contains $base ) ) {
            $intputFile = $_.FullName
            $inputName = $_.Name
            $outputFile = Join-Path -Path $PSScriptRoot -ChildPath (Join-Path -Path $dstPath -ChildPath $_.Name)

            $content = Get-Content -Path $intputFile

            # Replace absolute source paths with absolute destination paths
            $content = $content.Replace( $srcPathAbs, $dstPathAbs )
            $content = $content.Replace( $srcPathAbs.Replace('\', '/'), $dstPathAbs )

            # # Replace absolute paths with relative paths
            $content = $content.Replace( $srcRoot.Replace('\', '/'), $slashRepl)
            $content = $content.Replace( $srcRoot, $backSlashRepl )

            [XML]$xml = $content
            $nsUri = "http://schemas.microsoft.com/developer/msbuild/2003"

            $nsManager = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
            $nsManager.AddNamespace("ns", $nsUri)

            # Remove external project references
            $projRefNodes = Get-MatchingNodes -xml $xml -nsManager $nsManager -xpath "/ns:Project/ns:ItemGroup/ns:ProjectReference" -attr 'Include' -filters $depsFilter
            Remove-MatchingNodes -nodes $projRefNodes

            # Remove cmake references
            $cmakeRefNodes = Get-MatchingNodes -xml $xml -nsManager $nsManager -xpath "/ns:Project/ns:ItemGroup/ns:CustomBuild" -attr 'Include' -filters @( "CMakeLists.txt" )
            Remove-MatchingNodes -nodes $cmakeRefNodes

            # Update intermediate directory
            $intNodes = Get-MatchingNodes -xml $xml -nsManager $nsManager -xpath "/ns:Project/ns:PropertyGroup/ns:IntDir"
            foreach ( $node in $intNodes ) {
                if ( $node -is [System.Xml.XmlNode] ) {
                    $nodeInt = Join-Path -Path $intDir -ChildPath $node.InnerXml
                    $node.InnerXml = $nodeInt
                }
            }

            # Update Windows SDK version
            $winSdkNodes = Get-MatchingNodes -xml $xml -nsManager $nsManager "/ns:Project/ns:PropertyGroup/ns:WindowsTargetPlatformVersion"
            foreach ( $node in $winSdkNodes ) {
                if ($node -is [System.Xml.XmlNode]) {
                    $node.InnerXml = "10.0"
                }
            }

            # Add a post build step to copy the release executable to the target folder
            $cfgTypeNodes = Get-MatchingNodes -xml $xml -nsManager $nsManager -xpath "/ns:Project/ns:PropertyGroup/ns:ConfigurationType"
            if ( $cfgTypeNodes -and $cfgTypeNodes.Count -gt 0 ) {
                $cfg = $cfgTypeNodes[0]
                if ($cfg -is [System.Xml.XmlNode] -and $cfg.InnerXml -eq 'Application') {
                    $itemDefGroupNodes = Get-MatchingNodes -xml $xml -nsManager $nsManager -xpath "/ns:Project/ns:ItemDefinitionGroup"
                    foreach( $itemDefGroup in $itemDefGroupNodes ) {
                        $cond = $itemDefGroup.GetAttribute( "Condition" )
                        if ( $cond -ne $null -and $cond -like  "*`$(Configuration)*==*Release*") {

                            $xmlCompile = $itemDefGroup.SelectSingleNode( "ns:ClCompile", $nsManager )
                            if ( $xmlCompile -ne $null ) {
                                $xmlDbgInfoFormat = $xmlCompile.SelectSingleNode( "ns:DebugInformationFormat", $nsManager )
                                if ( $xmlDbgInfoFormat -ne $null ) {
                                    $xmlDbgInfoFormat.ParentNode.RemoveChild($xmlDbgInfoFormat) | Out-Null
                                }

                                $xmlDbgInfoFormat = $xml.CreateElement( "DebugInformationFormat", $nsUri )
                                $xmlDbgInfoFormat.InnerXml = "ProgramDatabase"
                                $xmlCompile.AppendChild( $xmlDbgInfoFormat ) | Out-Null
                            }

                            $xmlLink = $itemDefGroup.SelectSingleNode( "ns:Link", $nsManager )
                            if ( $xmlLink -ne $null ) {
                                $xmlGenDbg = $xmlLink.SelectSingleNode( "ns:GenerateDebugInformation", $nsManager )
                                if ( $xmlGenDbg -ne $null ) {
                                    $xmlGenDbg.ParentNode.RemoveChild($xmlGenDbg) | Out-Null
                                }

                                $xmlGenDbg = $xml.CreateElement( "GenerateDebugInformation", $nsUri )
                                $xmlGenDbg.InnerXml = "true"
                                $xmlLink.AppendChild( $xmlGenDbg ) | Out-Null
                            }

                            $xmlPostChild = $itemDefGroup.SelectSingleNode( "ns:PostBuildEvent", $nsManager )
                            if ( $xmlPostChild -ne $null ) {
                                $xmlPostChild.ParentNode.RemoveChild($xmlPostChild) | Out-Null
                            }

                            $dstExecutablePath = "`$(SolutionDir)x64\Release"
                            $copyCmd = ""
                            $copyCmd += "`n"
                            $copyCmd += "setlocal"
                            $copyCmd += "`n"
                            $copyCmd += "if not exist `"$dstExecutablePath\`" (mkdir `"$dstExecutablePath`")"
                            $copyCmd += "`n"
                            $copyCmd += "copy `"`$(OutDir)`$(TargetName).*`" `"$dstExecutablePath\Tracy.*`""
                            $copyCmd += "`n"
                            $copyCmd += "endlocal"
                            $copyCmd += "`n"

                            $xmlPostChild = $xml.CreateElement( "PostBuildEvent", $nsUri )
                            $xmlCommandChild = $xml.CreateElement( "Command", $nsUri )
                            $xmlCommandChild.InnerXml = $copyCmd
                            $xmlPostChild.AppendChild( $xmlCommandChild ) | Out-Null
                            $itemDefGroup.AppendChild( $xmlPostChild ) | Out-Null
                            break
                        }
                    }
                }
            }

            $xml.Save($outputFile)
        }
    }
}


function Copy-Projects
{
    param(
        [string]$srcPath,
        [string]$dstPath
    )

    Write-Host "Copy tracy projects and solution" -ForegroundColor DarkCyan

    if ( -Not (Test-Path -Path "$dstPath" -PathType Container) ) {
        New-Item -Path "$dstPath" -ItemType Directory -Force | Out-Null
    }

    $intDir = "_temp"
    $intPath = Join-Path -Path "$dstPath" -ChildPath $intDir

    if ( Test-Path -Path "$intPath" -PathType Container ) {
        Remove-Item -Path "$intPath" -Recurse -Force | Out-Null
    }

    Rewrite-Projects "$srcPath" "$dstPath" "*.vcxproj" $intDir
    Rewrite-Projects "$srcPath" "$dstPath" "*.vcxproj.filters"

    Get-ChildItem "$srcPath" -Filter "*.sln" | 
    Foreach-Object {
        $intputFile = $_.FullName
        $outputFile = Join-Path -Path "$dstPath" -ChildPath $_.Name
        Copy-Item -Path "$intputFile" -Destination "$outputFile" -Force
    }

    Write-Host ""
}


function Disable-DirectoryBuildProps
{
    param(
        [string]$dstPath
    )
    $propsXmlFile = Join-Path -Path $dstPath -ChildPath "Directory.Build.props"
    Remove-Item -Path $propsXmlFile -Force -erroraction silentlycontinue
    $propsXml = New-Object System.XMl.XmlTextWriter( $propsXmlFile, $null)
    $propsXml.WriteStartDocument()
    $propsXml.WriteStartElement( 'Project' )
    $propsXml.WriteEndElement()
    $propsXml.WriteEndDocument()
    $propsXml.Flush()
    $propsXml.Close()
}


function Print-PostGenerateMessage
{
    $msg = @(
        "IMPORTANT:",
        "  - you must remove the cmake and external projects from the solution!"
    )

    $borderLen = "**  **".Length
    $outputLen = ($msg | Measure-Object -Property Length -Maximum).Maximum + $borderLen
    Write-Host ""
    Write-Host ""
    Write-Host ("*" * $outputLen) -ForegroundColor DarkCyan
    foreach ( $line in $msg ) {
        $spaces = 
        Write-Host "** "  -ForegroundColor DarkCyan -NoNewline
        Write-Host "$line" -NoNewline
        Write-Host (' ' * ($outputLen - $line.Length - $borderLen)) -NoNewline
        Write-Host " **" -ForegroundColor DarkCyan
    }
    Write-Host ("*" * $outputLen) -ForegroundColor DarkCyan
}

if ( $help ) {
    Write-Host "Usage: " $MyInvocation.MyCommand.Name " [-dest sln_output_path] [-keepGenerated]" -ForegroundColor DarkCyan
    Write-Host "" -ForegroundColor DarkCyan
    Write-Host "  -dest           Output directory of the generated solution/project files (default: profiler/build/win32)" -ForegroundColor DarkCyan
    Write-Host "  -keepGenerated  Do not delete the _generated folder" -ForegroundColor DarkCyan
} else {
    if ((Get-Command "cl.exe" -ErrorAction SilentlyContinue) -eq $null) {
        $defaultVs = 'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\Launch-VsDevShell.ps1'
        if ( Test-Path -Path $defaultVs ) {
            Invoke-Expression "& '$defaultVs'"
            Set-Location -Path $PSScriptRoot
        }
    }

    if ((Get-Command "cl.exe" -ErrorAction SilentlyContinue) -eq $null) {
        Write-Host ""
        Write-Host "Please run " -NoNewline
        Write-Host "%YOUR_VS_INSTALL_DIR%\Common7\Tools\Launch-VsDevShell.ps1" -ForegroundColor Red -NoNewline
        Write-Host " first"
        Write-Host "Usage: " $MyInvocation.MyCommand.Name " [-dest sln_output_path]" -ForegroundColor DarkCyan
        Write-Host ""
        Write-Host "Alternatively: " -NoNewline
        Write-Host "open Visual Studio Command Prompt (or manually run vcvars64.bat in cmd.exe)" -ForegroundColor Red -NoNewline
        Write-Host " first"
        Write-Host  "Usage: powershell -File " $MyInvocation.MyCommand.Name " [-dest sln_output_path]" -ForegroundColor DarkCyan
        Write-Host ""
        Exit 1
    } else {
        $profilerDir = "profiler"
        $profilerDirAbs = Join-Path -Path $PSScriptRoot -ChildPath $profilerDir
        $source = "$profilerDir\build\_generated"

        if ( [string]::IsNullOrEmpty( $dest ) ) {
            $dest = "$profilerDir\build\win32"
        }

        if ( Test-Path -Path "$source" -PathType Container ) {
            Remove-Item "$source" -Recurse -Force -ErrorAction SilentlyContinue
        }

        if ( Test-Path -Path "$dest" -PathType Container ) {
            Remove-Item "$dest" -Recurse -Force -ErrorAction SilentlyContinue
        }

        New-Item -Path "$dest" -ItemType Directory -Force | Out-Null

        # Make sure we ignore any Directory.Build.props file higher up in the directory structure
        Disable-DirectoryBuildProps $profilerDirAbs

        $cmakeResult = (Cmake-GenerateProjects $source)
        if ( $cmakeResult -eq $true ) {
            Compile-Dependencies $source
            Copy-Dependencies $source $dest
            Copy-Projects $source $dest

            if ( -not $keepGenerated ) {
                Write-Host "Removing generated directory $source" -ForegroundColor DarkYellow
                Remove-Item -Path $source -Recurse -Force -erroraction silentlycontinue
            }

            Print-PostGenerateMessage

            $destDirAbs = Join-Path -Path $PSScriptRoot -ChildPath $dest
            $slnFiles = Get-ChildItem -Path $destDirAbs -Filter "*.sln"
            if ( $slnFiles.Length -gt 0 ) {
                $slnFile = $slnFiles[0]
                $destSlnAbs = Join-Path -Path $destDirAbs -ChildPath $slnFile
                Write-Host ""
                Write-Host "Solution generated in: " -NoNewline
                Write-Host "'$destSlnAbs'" -ForegroundColor Green
                Write-Host ""
            }
            Write-Host "Press any key to quit..." -ForegroundColor DarkGreen
        } else {
            Write-Host "Projects have not been created!" -ForegroundColor Red
            Write-Host "Press any key to quit..." -ForegroundColor Red
        }

        cmd /c pause | out-null
    }
}
