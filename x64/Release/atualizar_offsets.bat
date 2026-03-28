@echo off
REM atualizar_offsets.bat
REM Executa o PowerShell que atualiza/gera offsets.json (na raiz do .bat) e pausa no final

pushd "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0atualizar_offsets.ps1"

echo.
echo Pressione qualquer tecla para fechar...
pause >nul

popd