<#
Author: CsVeryLoveXieWenLi
Description: 打 SDK 包（类似 cmake install）

  scripts\install_sdk.ps1                     -> build\install
  scripts\install_sdk.ps1 -Prefix D:\sdk\corasio

  先走标准 xmake install（asio / llhttp / corasio 的头和库），
  再把 corasio 自己的头挪进 include\corasio\，使用方就能写
  #include <corasio/http/client.h>
#>
param(
    [string]$Prefix = 'build\install'
)

$ErrorActionPreference = 'Stop'

xmake install -o $Prefix | Out-Null

$include = Join-Path $Prefix 'include'
$nested = Join-Path $include 'corasio'
New-Item -ItemType Directory -Force $nested | Out-Null

foreach ($name in 'http', 'websocket', 'detail') {
    $from = Join-Path $include $name
    if (Test-Path $from) { Move-Item -Force $from $nested }
}

Write-Host "sdk ready: $Prefix"
Write-Host "  include\asio.hpp, asio\   include\llhttp.h   include\corasio\{http,websocket,detail}\"
Write-Host "  lib\corasio.lib, llhttp.lib"