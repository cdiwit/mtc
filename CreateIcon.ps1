
Add-Type -AssemblyName System.Drawing

try {
    # 1. 创建 Bitmap
    $size = 64
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)

    # 2. 绘制背景
    $bgAttr = [System.Drawing.Color]::FromArgb(255, 30, 30, 30)
    $g.Clear($bgAttr)

    # 4. 绘制文本 ">_"
    $fontName = "Consolas"
    $fontSize = 30
    $font = New-Object System.Drawing.Font $fontName, $fontSize
    
    $brush = [System.Drawing.Brushes]::LimeGreen
    $g.DrawString(" >_", $font, $brush, 0, 10)

    # 5. 保存为 PNG 内存流
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngBytes = $ms.ToArray()
    $ms.Dispose()
    $g.Dispose()
    $bmp.Dispose()

    # 6. 构建 ICO 文件 (PNG 格式)
    $outputBytes = New-Object System.Collections.Generic.List[byte]
    
    # Header
    $outputBytes.AddRange([BitConverter]::GetBytes([int16]0))
    $outputBytes.AddRange([BitConverter]::GetBytes([int16]1))
    $outputBytes.AddRange([BitConverter]::GetBytes([int16]1))

    # Directory Entry
    $outputBytes.Add([byte]64)   # Width
    $outputBytes.Add([byte]64)   # Height
    $outputBytes.Add([byte]0)
    $outputBytes.Add([byte]0)
    $outputBytes.AddRange([BitConverter]::GetBytes([int16]1))
    $outputBytes.AddRange([BitConverter]::GetBytes([int16]32))
    $outputBytes.AddRange([BitConverter]::GetBytes([int32]$pngBytes.Length))
    $outputBytes.AddRange([BitConverter]::GetBytes([int32]22))

    # Image Data
    $outputBytes.AddRange($pngBytes)

    [System.IO.File]::WriteAllBytes("resources/mtc.ico", $outputBytes.ToArray())

    Write-Host "Icon created successfully at resources/mtc.ico"
}
catch {
    Write-Host "Error: $_"
}
