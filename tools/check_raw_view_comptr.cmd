@echo off
REM ComPtr<ID3D11*View> の直接使用をチェック
REM View<SRV/RTV/DSV/UAV> を使用すべき

setlocal enabledelayedexpansion

set FOUND=0

REM view.h は除外（ドキュメント内の例があるため）
for /r "source" %%f in (*.h *.cpp) do (
    echo %%f | findstr /i "view.h" >nul
    if errorlevel 1 (
        findstr /n "ComPtr<ID3D11ShaderResourceView>" "%%f" >nul 2>&1
        if not errorlevel 1 (
            echo [ERROR] %%f: ComPtr^<ID3D11ShaderResourceView^> found. Use View^<SRV^> instead.
            set FOUND=1
        )
        findstr /n "ComPtr<ID3D11RenderTargetView>" "%%f" >nul 2>&1
        if not errorlevel 1 (
            echo [ERROR] %%f: ComPtr^<ID3D11RenderTargetView^> found. Use View^<RTV^> instead.
            set FOUND=1
        )
        findstr /n "ComPtr<ID3D11DepthStencilView>" "%%f" >nul 2>&1
        if not errorlevel 1 (
            echo [ERROR] %%f: ComPtr^<ID3D11DepthStencilView^> found. Use View^<DSV^> instead.
            set FOUND=1
        )
        findstr /n "ComPtr<ID3D11UnorderedAccessView>" "%%f" >nul 2>&1
        if not errorlevel 1 (
            echo [ERROR] %%f: ComPtr^<ID3D11UnorderedAccessView^> found. Use View^<UAV^> instead.
            set FOUND=1
        )
    )
)

if %FOUND%==1 (
    echo.
    echo [FAILED] Raw ComPtr^<View^> usage detected. Please use View^<Tag^> instead.
    exit /b 1
)

echo [OK] No raw ComPtr^<View^> usage found.
exit /b 0
