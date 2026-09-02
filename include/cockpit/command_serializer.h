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

  // Returns a view into an internal buffer valid until the next Serialize().
  std::string_view Serialize(const G29State& state, std::uint32_t header_id);

 private:
  Identity identity_;
  char buf_[4096]{};
};

}  // namespace cockpit

#endif  // COCKPIT_COMMAND_SERIALIZER_H_
