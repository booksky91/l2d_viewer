#pragma once

#include <string>

namespace l2dgui
{

void installCrashDiagnostics();
void setCrashStage(const char* stage);
const char* getCrashStage();
void logDiagnostic(const std::string& message);
std::string formatNativeException(unsigned long code, const void* address, const char* operation);

} // namespace l2dgui
