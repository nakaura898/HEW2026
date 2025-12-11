::============================================================================
:: _common.cmd
:: ビルドスクリプト用の共通処理ライブラリ
::
:: 使用方法:
::   call tools\_common.cmd :関数名
::
:: 利用可能な関数:
::   :init             - UTF-8モード設定、作業ディレクトリをリポジトリルートに移動
::   :check_project    - build/HEW2026.sln の存在確認
::   :setup_msbuild    - VsDevCmd.bat を実行してMSBuild環境を構築
::   :find_msbuild_exe - MSBuild.exe のパスを MSBUILD_PATH に設定
::   :generate_project - Premake5でVisual Studio 2022ソリューションを生成
::============================================================================

goto %~1

::----------------------------------------------------------------------------
:: :init
:: UTF-8コードページ設定 & 作業ディレクトリをリポジトリルートに移動
::----------------------------------------------------------------------------
:init
    chcp 65001 >nul
    cd /d "%~dp0.."
    exit /b 0

::----------------------------------------------------------------------------
:: :check_project
:: ソリューションファイルの存在を確認
:: 存在しなければエラー終了
::----------------------------------------------------------------------------
:check_project
    if not exist "build\HEW2026.sln" (
        echo プロジェクトが見つかりません。先に @make_project.cmd を実行してください。
        exit /b 1
    )
    exit /b 0

::----------------------------------------------------------------------------
:: :setup_msbuild
:: vswhere.exe を使ってVisual Studioを探し、VsDevCmd.bat を実行
:: これによりMSBuildコマンドが使用可能になる
::----------------------------------------------------------------------------
:setup_msbuild
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "%VSWHERE%" (
        echo [ERROR] vswhere.exe が見つかりません。Visual Studio 2022 をインストールしてください。
        exit /b 1
    )
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find Common7\Tools\VsDevCmd.bat`) do (
        set "VSCMD_PATH=%%i"
    )
    if not defined VSCMD_PATH (
        echo [ERROR] Visual Studio が見つかりません
        exit /b 1
    )
    call "%VSCMD_PATH%" -arch=amd64 >nul 2>&1
    exit /b 0

::----------------------------------------------------------------------------
:: :find_msbuild_exe
:: MSBuild.exe のフルパスを MSBUILD_PATH 環境変数に設定
:: VsDevCmd.bat を使わずに直接MSBuildを呼び出したい場合に使用
::----------------------------------------------------------------------------
:find_msbuild_exe
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "%VSWHERE%" (
        echo [ERROR] vswhere.exe が見つかりません。Visual Studio 2022 をインストールしてください。
        exit /b 1
    )
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
        set "MSBUILD_PATH=%%i"
    )
    if not defined MSBUILD_PATH (
        echo [ERROR] MSBuild が見つかりません。Visual Studio 2022 の C++ ワークロードをインストールしてください。
        exit /b 1
    )
    exit /b 0

::----------------------------------------------------------------------------
:: :generate_project
:: Premake5を使ってVisual Studio 2022ソリューションを生成
::
:: 日本語パス対策:
::   Premake5は日本語パスで動作しないため、TEMPディレクトリに
::   ジャンクション (シンボリックリンク的なもの) を作成して回避
::----------------------------------------------------------------------------
:generate_project
    :: 日本語パス対策: TEMPにジャンクションを作成
    for /f %%a in ('powershell -command "[guid]::NewGuid().ToString()"') do set "GUID=%%a"
    mklink /j "%TEMP%\%GUID%" "%~dp0.." >nul
    pushd "%TEMP%\%GUID%"
    tools\premake5.exe vs2022
    set "PREMAKE_RESULT=%errorlevel%"
    popd
    rmdir "%TEMP%\%GUID%"
    if %PREMAKE_RESULT% neq 0 (
        echo [ERROR] プロジェクト生成に失敗しました
        exit /b 1
    )
    echo [OK] build\HEW2026.sln を生成しました
    exit /b 0
