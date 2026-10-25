#pragma once

#include "AppOptions.h"
#include "providers/ISensorProvider.h"

#include <memory>
#include <vector>

class MockProvider;

// The sensor providers for a data source: the real machine (default), prepared test trees (fixture
// mode) or one MockProvider standing in for all hardware (simulation). `mock` receives the
// MockProvider (owned by the returned list) in simulation mode, otherwise nullptr.
std::vector<std::unique_ptr<ISensorProvider>> createProviders(const AppOptions &options, MockProvider **mock = nullptr);
