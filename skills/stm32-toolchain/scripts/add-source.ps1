# 把源文件加入 Keil .uvprojx 工程
#
# 为什么需要它：新建的 .c 不加进工程就不会被编译，链接时报 "Undefined symbol"，
# 而报错信息完全不提"文件没加进工程"，很容易误以为是代码写错了。
# CubeMX 重新生成后更是会一次性抹掉所有手动添加的文件，需要批量加回。
#
# 用法:
#   .\add-source.ps1 -Files ..\Core\Src\pid.c,..\Core\Inc\pid.h
#   .\add-source.ps1 -Files ..\Core\Src\*.c -Group "Application/User/Core"
#   .\add-source.ps1 -List                      # 只列出当前工程包含哪些文件
param(
    [string]$Project = "",
    [string[]]$Files = @(),
    [string]$Group   = "",
    [switch]$List
)

. "$PSScriptRoot\_common.ps1"
$proj = Find-UvProj $Project
$projDir = Split-Path $proj -Parent

[xml]$xml = Get-Content $proj
$groups = $xml.Project.Targets.Target.Groups.Group

if ($List) {
    foreach ($g in $groups) {
        Write-Output ("[" + $g.GroupName + "]")
        if ($g.Files.File) {
            foreach ($f in $g.Files.File) { Write-Output ("    " + $f.FileName) }
        }
    }
    exit 0
}

if ($Files.Count -eq 0) {
    Write-Output "[X] 未指定文件。用 -Files 指定，或用 -List 查看当前工程内容"
    exit 1
}

# 选目标分组：默认用最后一个非 Drivers 分组（通常是放用户代码的那个）
if ($Group -ne "") {
    $target = $groups | Where-Object { $_.GroupName -eq $Group }
    if (-not $target) {
        Write-Output "[X] 找不到分组 '$Group'。现有分组："
        $groups | ForEach-Object { Write-Output ("    " + $_.GroupName) }
        exit 1
    }
} else {
    # 优先放进 CubeMX 约定的用户代码分组；否则退而求其次选第一个非 Drivers 分组。
    # 注意不能简单取"最后一个非 Drivers 分组"——工程里常有 test 之类的分组排在后面。
    $target = $groups | Where-Object { $_.GroupName -match 'Application/User' } | Select-Object -First 1
    if (-not $target) {
        $target = $groups | Where-Object { $_.GroupName -notmatch 'Drivers|CMSIS|test' } | Select-Object -First 1
    }
    if (-not $target) { $target = $groups | Select-Object -First 1 }
    Write-Output ("目标分组: " + $target.GroupName + "  (可用 -Group 指定其他分组)")
}

# 收集已有文件名，避免重复添加（Keil 对重复条目的表现很怪）
$existing = @()
if ($target.Files.File) { $existing = $target.Files.File | ForEach-Object { $_.FileName } }

$added = 0
foreach ($pattern in $Files) {
    # 支持通配符；相对路径按 .uvprojx 所在目录解析
    $resolved = Get-ChildItem -Path (Join-Path $projDir $pattern) -ErrorAction SilentlyContinue
    if (-not $resolved) {
        $resolved = Get-ChildItem -Path $pattern -ErrorAction SilentlyContinue
    }
    if (-not $resolved) {
        Write-Output "[!] 找不到文件，跳过: $pattern"
        continue
    }

    foreach ($file in $resolved) {
        $name = $file.Name
        if ($existing -contains $name) {
            Write-Output "    已存在，跳过: $name"
            continue
        }

        # FileType: 1=C源文件 2=汇编 4=库 5=头文件
        $ext = $file.Extension.ToLower()
        switch ($ext) {
            '.c'   { $ftype = '1' }
            '.cpp' { $ftype = '8' }
            '.s'   { $ftype = '2' }
            '.h'   { $ftype = '5' }
            default {
                Write-Output "[!] 不认识的扩展名，跳过: $name"
                continue
            }
        }

        # .uvprojx 里的路径写成相对于工程文件所在目录
        Push-Location $projDir
        $rel = (Resolve-Path -Relative $file.FullName) -replace '^\.\\', ''
        Pop-Location

        $node = $xml.CreateElement('File')
        foreach ($pair in @(@('FileName',$name), @('FileType',$ftype), @('FilePath',$rel))) {
            $child = $xml.CreateElement($pair[0])
            $child.InnerText = $pair[1]
            [void]$node.AppendChild($child)
        }
        [void]$target.Files.AppendChild($node)
        Write-Output "  + $rel  (FileType=$ftype)"
        $added++
    }
}

if ($added -gt 0) {
    $xml.Save($proj)
    Write-Output ""
    Write-Output "[OK] 已添加 $added 个文件到 $proj"
    Write-Output "     如果 Keil 正开着这个工程，请先关闭再重新打开，否则 Keil 保存时会覆盖掉本次修改。"
} else {
    Write-Output "[--] 没有新增文件"
}
