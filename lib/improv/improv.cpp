#include "improv.h"

namespace improv {

auto ParseImprovData(const std::vector<uint8_t> &data) -> ImprovCommand {
  return ParseImprovData(data.data(), data.size());
}

auto ParseImprovData(const uint8_t *data, size_t length) -> ImprovCommand {
  auto command = (Command) data[0];
  uint8_t dataLength = data[1];

  if (dataLength != length - 3) {
    return {.command = UNKNOWN};
  }

  uint8_t checksum = data[length - 1];

  uint32_t calculatedChecksum = 0;
  for (uint8_t i = 0; i < length - 1; i++) {
    calculatedChecksum += data[i];
  }

  if ((uint8_t) calculatedChecksum != checksum) {
    return {.command = BAD_CHECKSUM};
  }

  if (command == WIFI_SETTINGS) {
    uint8_t ssidLength = data[2];
    uint8_t ssidStart = 3;
    size_t ssidEnd = ssidStart + ssidLength;

    uint8_t passLength = data[ssidEnd];
    size_t passStart = ssidEnd + 1;
    size_t passEnd = passStart + passLength;

    std::string ssid(data + ssidStart, data + ssidEnd);
    std::string password(data + passStart, data + passEnd);
    return {.command = command, .ssid = ssid, .password = password};
  }

  return {
      .command = command,
  };
}

auto BuildRpcResponse(Command command, const std::vector<std::string> &datum) -> std::vector<uint8_t> {
  std::vector<uint8_t> out;
  uint32_t length = 0;
  out.push_back(command);
  for (auto str : datum) {
    uint8_t len = str.length();
    length += len;
    out.push_back(len);
    out.insert(out.end(), str.begin(), str.end());
  }
  out.insert(out.begin() + 1, length);

  uint32_t calculatedChecksum = 0;

  for (uint8_t byte : out) {
    calculatedChecksum += byte;
  }
  out.push_back(calculatedChecksum);
  return out;
}

auto BuildRpcResponse(Command command, const std::vector<String> &datum) -> std::vector<uint8_t> {
  std::vector<uint8_t> out;
  uint32_t length = 0;
  out.push_back(command);
  for (auto str : datum) {
    uint8_t len = str.length();
    length += len;
    out.push_back(len);
    out.insert(out.end(), str.begin(), str.end());
  }
  out.insert(out.begin() + 1, length);

  uint32_t calculatedChecksum = 0;

  for (uint8_t byte : out) {
    calculatedChecksum += byte;
  }
  out.push_back(calculatedChecksum);
  return out;
}

}  // namespace improv
