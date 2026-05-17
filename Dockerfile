# escape=`
FROM mcr.microsoft.com/powershell:lts-windowsservercore-ltsc2022 AS base

WORKDIR C:\workspace

ENV UE_ENGINE_DIR=""
ENV PLUGIN_TARGET_PLATFORMS="Win64"

COPY . C:\workspace

FROM base AS test

CMD ["pwsh", "-NoLogo", "-NoProfile", "-File", "C:\\workspace\\Build\\RunPlaneToPBRAutomationTests.ps1"]

FROM base AS package

CMD ["pwsh", "-NoLogo", "-NoProfile", "-Command", "& C:\\workspace\\Build\\RunPlaneToPBRAutomationTests.ps1; & C:\\workspace\\Build\\PackagePlaneToPBRPlugin.ps1"]
