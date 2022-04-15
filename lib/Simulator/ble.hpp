#include <cstring>
#include <functional>

#include "Inputs/RemoteInput.hpp"

void BleFindTurnTouch(std::string address, std::function<void(RemoteCommand command)> callback);
