Set-Location C:\Users\nanat\Desktop\HEW2026

# 一時フォルダ作成
Remove-Item -Recurse -Force 'dist' -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path 'dist\build\bin\Release-windows-x86_64\game' -Force

# game.exeとDLLをコピー
Copy-Item 'build\bin\Release-windows-x86_64\game\*' 'dist\build\bin\Release-windows-x86_64\game\' -Recurse

# assetsフォルダをコピー
Copy-Item 'assets' 'dist\assets' -Recurse

# バッチファイルをコピー
Copy-Item 'ゲーム起動.bat' 'dist\'

# ZIP作成
Remove-Item 'ARAS_v0.3.3.zip' -ErrorAction SilentlyContinue
Compress-Archive -Path 'dist\*' -DestinationPath 'ARAS_v0.3.3.zip' -Force

Write-Host "Done. ZIP size:"
(Get-Item 'ARAS_v0.3.3.zip').Length / 1MB
