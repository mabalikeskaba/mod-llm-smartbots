# Multi-stage build for the C# LLM service.
# Build context = this `service/` directory (it contains nuget.config so the
# restore uses the public feed).

FROM mcr.microsoft.com/dotnet/sdk:8.0 AS build
WORKDIR /src
COPY . .
RUN dotnet restore BotAgent.Service/BotAgent.Service.csproj
RUN dotnet publish BotAgent.Service/BotAgent.Service.csproj -c Release -o /app --no-restore

FROM mcr.microsoft.com/dotnet/aspnet:8.0
WORKDIR /app
COPY --from=build /app .

# Listen on all interfaces inside the container (overridable).
ENV ASPNETCORE_URLS=http://0.0.0.0:8820
EXPOSE 8820

ENTRYPOINT ["dotnet", "BotAgent.Service.dll"]
