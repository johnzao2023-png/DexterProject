# atualizar_offsets.ps1
# Baixa info.json, offsets.json e client_dll.json do repositório a2x
# Atualiza/cria offsets.json local NA PASTA RAIZ (mesma pasta do .ps1/.bat)
# Garante que todas as chaves estejam presentes.
# Requer PowerShell (v5.1 OK).

$ErrorActionPreference = 'Continue'

# Trabalhar na pasta do script
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptDir

$urls = @{
    info = "https://github.com/a2x/cs2-dumper/raw/refs/heads/main/output/info.json"
    offsets = "https://github.com/a2x/cs2-dumper/raw/refs/heads/main/output/offsets.json"
    client_dll = "https://github.com/a2x/cs2-dumper/raw/refs/heads/main/output/client_dll.json"
}

function Get-Json($u) {
    try {
        return Invoke-RestMethod -Uri $u -UseBasicParsing -ErrorAction Stop
    } catch {
        Write-Host "Falha ao baixar: $u"
        Write-Host $_.Exception.Message
        return $null
    }
}

# DESTINO na RAIZ
$destPath = Join-Path $scriptDir "offsets.json"

# Template ordenado com todas as chaves necessárias (padrões)
$template = [ordered]@{
    build_number = 0
    dwBuildNumber = 0
    dwEntityList = 0
    dwLocalPlayer = 0
    dwPlantedC4 = 0
    dwViewMatrix = 0
    m_ArmorValue = 0
    m_bIsDefusing = 0
    m_flC4Blow = 0
    m_flFlashOverlayAlpha = 0
    m_flNextBeep = 0
    m_flTimerLength = 0
    m_hPlayerPawn = 0
    m_iAccount = 0
    m_iHealth = 0
    m_iTeamNum = 0
    m_pClippingWeapon = 0
    m_pGameSceneNode = 0
    m_pInGameMoneyServices = 0
    m_sSanitizedPlayerName = ""
    m_szName = ""
    m_vOldOrigin = 0
    m_vecAbsOrigin = 0
    dwLocalPlayerController = 0
    m_hController = 0
    m_iszPlayerName = ""
}

# Se não existir, criar offsets.json com template
if (-not (Test-Path $destPath)) {
    Write-Host "Arquivo local offsets.json não encontrado. Criando padrão em: $destPath"
    $template | ConvertTo-Json -Depth 12 | Out-File -FilePath $destPath -Encoding UTF8
    Write-Host "Arquivo padrão criado."
}

Write-Host "Usando destino local: $destPath"
Write-Host "Baixando info.json..."
$info = Get-Json $urls.info
if (-not $info) { Write-Host "Erro: não foi possível baixar info.json"; exit 1 }

Write-Host "Baixando offsets.json (remoto)..."
$remoteOffsets = Get-Json $urls.offsets
if (-not $remoteOffsets) { Write-Host "Erro: não foi possível baixar offsets.json remoto"; exit 1 }

Write-Host "Baixando client_dll.json..."
$client = Get-Json $urls.client_dll
if (-not $client) { Write-Host "Erro: não foi possível baixar client_dll.json"; exit 1 }

# Ler offsets local existente (mantendo valores anteriores quando possível)
try {
    $localRaw = Get-Content -Raw -Path $destPath -ErrorAction Stop
    $local = $localRaw | ConvertFrom-Json -ErrorAction Stop
} catch {
    Write-Host "Falha ao ler/parsar offsets local. Usando valores padrão."
    $local = @{}
}

# Garantir que $local contenha todas as chaves (preencher com template quando faltar)
$merged = [ordered]@{}
foreach ($k in $template.Keys) {
    if ($local -and $local.PSObject.Properties.Name -contains $k) {
        $merged[$k] = $local.$k
    } else {
        $merged[$k] = $template[$k]
    }
}

# Atualizar build_number a partir de info.json (sempre)
try {
    $merged.build_number = [int]$info.build_number
} catch {
    Write-Host "Não foi possível ler build_number do info.json. Mantendo valor local/padrão."
}

# Atualizar campos do offsets.json remoto
try {
    if ($remoteOffsets.'engine2.dll' -and $remoteOffsets.'engine2.dll'.dwBuildNumber) {
        $merged.dwBuildNumber = $remoteOffsets.'engine2.dll'.dwBuildNumber
    }
    if ($remoteOffsets.'client.dll') {
        $r = $remoteOffsets.'client.dll'
        if ($r.dwLocalPlayerPawn) { $merged.dwLocalPlayer = $r.dwLocalPlayerPawn }
        if ($r.dwLocalPlayerController) { $merged.dwLocalPlayerController = $r.dwLocalPlayerController }
        if ($r.dwEntityList) { $merged.dwEntityList = $r.dwEntityList }
        if ($r.dwViewMatrix) { $merged.dwViewMatrix = $r.dwViewMatrix }
        if ($r.dwPlantedC4) { $merged.dwPlantedC4 = $r.dwPlantedC4 }
    }
} catch {
    Write-Host "Estrutura de offsets.json remoto inesperada: $($_.Exception.Message)"
}

# Atualizar campos do client_dll.json (classes -> fields)
try {
    $c = $client.'client.dll'.classes
    if ($c) {
        if ($c.C_CSPlayerPawn.fields.m_bIsDefusing) { $merged.m_bIsDefusing = $c.C_CSPlayerPawn.fields.m_bIsDefusing }
        if ($c.C_CSPlayerPawn.fields.m_ArmorValue) { $merged.m_ArmorValue = $c.C_CSPlayerPawn.fields.m_ArmorValue }
        if ($c.C_CSPlayerPawn.fields.m_pClippingWeapon) { $merged.m_pClippingWeapon = $c.C_CSPlayerPawn.fields.m_pClippingWeapon }

        if ($c.C_CSPlayerPawnBase.fields.m_flFlashOverlayAlpha) { $merged.m_flFlashOverlayAlpha = $c.C_CSPlayerPawnBase.fields.m_flFlashOverlayAlpha }

        if ($c.C_PlantedC4.fields.m_flC4Blow) { $merged.m_flC4Blow = $c.C_PlantedC4.fields.m_flC4Blow }
        if ($c.C_PlantedC4.fields.m_flNextBeep) { $merged.m_flNextBeep = $c.C_PlantedC4.fields.m_flNextBeep }
        if ($c.C_PlantedC4.fields.m_flTimerLength) { $merged.m_flTimerLength = $c.C_PlantedC4.fields.m_flTimerLength }

        if ($c.CCSPlayerController.fields.m_hPlayerPawn) { $merged.m_hPlayerPawn = $c.CCSPlayerController.fields.m_hPlayerPawn }
        if ($c.CCSPlayerController_InGameMoneyServices.fields.m_iAccount) { $merged.m_iAccount = $c.CCSPlayerController_InGameMoneyServices.fields.m_iAccount }
        if ($c.CCSPlayerController.fields.m_pInGameMoneyServices) { $merged.m_pInGameMoneyServices = $c.CCSPlayerController.fields.m_pInGameMoneyServices }

        if ($null -ne $c.CCSPlayerController.fields.m_sSanitizedPlayerName) {
            $merged.m_sSanitizedPlayerName = $c.CCSPlayerController.fields.m_sSanitizedPlayerName
        } elseif ($c.CCSPlayerController.fields.m_iszPlayerName) {
            $merged.m_sSanitizedPlayerName = $c.CCSPlayerController.fields.m_iszPlayerName
        }

        if ($c.C_BasePlayerPawn.fields.m_hController) { $merged.m_hController = $c.C_BasePlayerPawn.fields.m_hController }
        if ($c.CBasePlayerController.fields.m_iszPlayerName) { $merged.m_iszPlayerName = $c.CBasePlayerController.fields.m_iszPlayerName }

        if ($c.C_BaseEntity.fields.m_iHealth) { $merged.m_iHealth = $c.C_BaseEntity.fields.m_iHealth }
        if ($c.C_BaseEntity.fields.m_iTeamNum) { $merged.m_iTeamNum = $c.C_BaseEntity.fields.m_iTeamNum }
        if ($c.C_BaseEntity.fields.m_pGameSceneNode) { $merged.m_pGameSceneNode = $c.C_BaseEntity.fields.m_pGameSceneNode }

        if ($c.CCSWeaponBaseVData.fields.m_szName) { $merged.m_szName = $c.CCSWeaponBaseVData.fields.m_szName }
        if ($c.C_BasePlayerPawn.fields.m_vOldOrigin) { $merged.m_vOldOrigin = $c.C_BasePlayerPawn.fields.m_vOldOrigin }
        if ($c.CGameSceneNode.fields.m_vecAbsOrigin) { $merged.m_vecAbsOrigin = $c.CGameSceneNode.fields.m_vecAbsOrigin }
    }
} catch {
    Write-Host "Estrutura de client_dll.json diferente do esperado: $($_.Exception.Message)"
}

# Garantir tipos simples (int/string) quando possível
foreach ($k in $merged.Keys) {
    if ($merged[$k] -is [System.Management.Automation.PSObject]) {
        $merged[$k] = $merged[$k].ToString()
    }
}

# Salvar offsets.json atualizado na RAIZ
try {
    $out = $merged | ConvertTo-Json -Depth 16
    $out | Out-File -FilePath $destPath -Encoding UTF8
    Write-Host "Offsets atualizados com sucesso em: $destPath"
} catch {
    Write-Host "Erro ao salvar offsets.json: $($_.Exception.Message)"
    exit 1
}

Write-Host "Concluído."
exit 0