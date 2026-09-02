#ifndef COCKPIT_COMMAND_SERIALIZER_H_
#define COCKPIT_COMMAND_SERIALIZER_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "cockpit/g29_state.h"

namespace cockpit {

class CommandSerializer {
 public:
  explicit CommandSerializer(Identity identity);

  // Compact one-line JSON. View is valid until the next Serialize*().
  std::string_view Serialize(const G29State& state, std::uint32_t header_id);

  // Indented JSON for a terminal. View is valid until the next Serialize*().
  std::string_view SerializePretty(const G29State& state,
                                  std::uint32_t header_id);

 private:
  Identity identity_;
  char buf_[8192]{};
};

}  // namespace cockpit

#endif  // COCKPIT_COMMAND_SERIALIZER_H_
