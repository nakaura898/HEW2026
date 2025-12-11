--============================================================================
-- premake5.lua
-- プロジェクト構成ファイル
--============================================================================

-- ワークスペース設定
workspace "HEW2026"
    configurations { "Debug", "Release" }
    platforms { "x64" }
    location "build"

    -- C++20
    language "C++"
    cppdialect "C++20"

    -- 文字セット
    characterset "Unicode"

    -- 共通設定
    filter "configurations:Debug"
        defines { "DEBUG", "_DEBUG" }
        symbols "On"
        optimize "Off"
        runtime "Debug"

    filter "configurations:Release"
        defines { "NDEBUG" }
        symbols "Off"
        optimize "Full"
        runtime "Release"

    filter "platforms:x64"
        architecture "x64"

    filter {}

-- 出力ディレクトリ (build/配下に統一)
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
bindir = "build/bin/" .. outputdir
objdir_base = "build/obj/" .. outputdir

--============================================================================
-- 外部ライブラリ（ビルド済みバイナリを使用）
--============================================================================
-- DirectXTex と DirectXTK は external/lib/ にビルド済み.libを配置
-- ビルド時間短縮のため、ソースからのビルドは行わない

--============================================================================
-- dx11ライブラリ
--============================================================================
project "dx11"
    kind "StaticLib"
    location "build/dx11"

    targetdir (bindir .. "/%{prj.name}")
    objdir (objdir_base .. "/%{prj.name}")

    files {
        "source/dx11/**.h",
        "source/dx11/**.cpp",
        "source/dx11/**.inl"
    }

    -- 除外ファイル
    removefiles {
        "source/dx11/compile/shader_reflection.cpp"
    }

    includedirs {
        "source",
        "external/DirectXTex/DirectXTex",
        "external/DirectXTK/Inc"
    }

    -- ビルド済み外部ライブラリのパス
    libdirs {
        "external/lib/%{cfg.buildcfg}"
    }

    links {
        "d3d11",
        "d3dcompiler",
        "dxguid",
        "dxgi",
        "xinput",
        "DirectXTex",
        "DirectXTK"
    }

    defines {
        "_WIN32_WINNT=0x0A00",
        "WIN32_LEAN_AND_MEAN"
    }

    -- 警告設定
    warnings "Extra"
    flags { "FatalWarnings" }

    buildoptions { "/utf-8", "/permissive-" }

    -- リンカー警告を無視 (Windows SDK重複定義)
    linkoptions { "/ignore:4006" }

--============================================================================
-- エンジンライブラリ
--============================================================================
project "engine"
    kind "StaticLib"
    location "build/engine"

    targetdir (bindir .. "/%{prj.name}")
    objdir (objdir_base .. "/%{prj.name}")

    files {
        "source/engine/**.h",
        "source/engine/**.cpp"
    }

    includedirs {
        "source",
        "source/engine",
        "external/DirectXTK/Inc"
    }

    -- ビルド済み外部ライブラリのパス
    libdirs {
        "external/lib/%{cfg.buildcfg}"
    }

    links {
        "dx11",
        "DirectXTK"
    }

    defines {
        "_WIN32_WINNT=0x0A00"
    }

    warnings "Extra"
    flags { "FatalWarnings" }

    buildoptions { "/utf-8", "/permissive-" }

--============================================================================
-- ゲーム実行ファイル
--============================================================================
project "game"
    kind "WindowedApp"
    location "build/game"

    targetdir (bindir .. "/%{prj.name}")
    objdir (objdir_base .. "/%{prj.name}")

    files {
        "source/game/**.h",
        "source/game/**.cpp"
    }

    includedirs {
        "source",
        "source/engine",
        "external/DirectXTK/Inc"
    }

    -- ビルド済み外部ライブラリのパス
    libdirs {
        "external/lib/%{cfg.buildcfg}"
    }

    links {
        "engine",
        "dx11",
        "DirectXTex",
        "DirectXTK",
        "d3d11",
        "d3dcompiler",
        "dxguid",
        "dxgi",
        "xinput"
    }

    defines {
        "_WIN32_WINNT=0x0A00"
    }

    -- 作業ディレクトリ
    debugdir "."

    warnings "Extra"
    buildoptions { "/utf-8", "/permissive-" }

    -- リンカー警告を無視 (外部ライブラリPDB不足)
    linkoptions { "/ignore:4099" }

--============================================================================
-- テスト実行ファイル (現在無効)
--============================================================================
--[[
project "tests"
    kind "ConsoleApp"
    location "build/tests"

    targetdir (bindir .. "/%{prj.name}")
    objdir (objdir_base .. "/%{prj.name}")

    files {
        "tests/**.h",
        "tests/**.cpp"
    }

    removefiles {
        "tests/generate_test_textures.cpp"
    }

    includedirs {
        "source",
        "tests"
    }

    links {
        "engine",
        "dx11",
        "DirectXTex",
        "DirectXTK",
        "d3d11",
        "d3dcompiler",
        "dxguid",
        "dxgi",
        "xinput"
    }

    defines {
        "_WIN32_WINNT=0x0A00"
    }

    debugdir "."

    warnings "Extra"
    buildoptions { "/utf-8", "/permissive-" }
]]--
