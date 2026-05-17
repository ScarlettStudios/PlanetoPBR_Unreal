FROM mcr.microsoft.com/powershell:7.4-ubuntu-22.04

WORKDIR /workspace

ENV UE_ENGINE_DIR=""
ENV PLUGIN_TARGET_PLATFORMS="Linux"

COPY . /workspace

CMD ["pwsh", "-NoLogo", "-NoProfile", "-File", "/workspace/Scripts/PackagePlaneToPBRPlugin.ps1"]
