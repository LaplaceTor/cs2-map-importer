#pragma once

#include "Core/Error/ExecutionGuard.h"

namespace Application::Execution {

/**
 * @brief Application Execution Boundary Guard.
 *
 * Aliases the unified Core::Error::ExecutionGuard infrastructure, ensuring
 * identical exception safety, error classification, and Result translation
 * across Application and Workflow boundaries without code duplication.
 */
using ExecutionGuard = Core::Error::ExecutionGuard;

} // namespace Application::Execution
