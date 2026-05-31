$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$port = if ($args.Count -gt 0) { [int]$args[0] } else { 8765 }
python (Join-Path $scriptDir "serve.py") --port $port
