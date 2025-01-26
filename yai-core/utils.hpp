#include <cstdint>
#include <string>

namespace yai::utils {

inline static std::uint16_t StoPortNum(const char *s) {
  return static_cast<std::uint16_t>(std::stoul(s));
}

} // namespace yai::utils
