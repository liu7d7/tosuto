#include <ostream>
#include <vector>
#include "../ami.h"
#include "vm.h"

namespace ami::vm {
  // basic file format:
  // -- interned strings
  // 1st 4 bytes of section: # of interned strings
  // for each interned string:
  //   1st 4 bytes: length of string (n)
  //   next n bytes: string

  // -- constants
  // 1st 4 bytes of section: # of constants
  // for each constant:
  //   1st byte: type of value (type)
  //   switch on type:
  //     0: next 8 bytes as a double
  //     1, 2, 3, 4, 7, 8: these should never be serialized.
  //     5: next 4 bytes as an index to an interned string
  //     6: 1st byte: arity, next 4 bytes: index of chunk

  // -- chunks
  // 1st 4 bytes of section: # of chunks
  // for each chunk:
  //   1st 4 bytes: index to name in interned strings
  //   2nd 4 bytes: index to start of literals
  //   3rd 4 bytes: # of literals
  //   4th 4 bytes: # of bytes in bytecode (n)
  //   next n bytes: bytecode

  struct serializer {
    std::vector<u8> interned_strings;
    std::vector<u8> literals;
    std::vector<u8> chunks;
    using serializable_literal_value = std::variant<double, size_t, std::pair<u8, size_t>>;
    using serializable_literal = std::pair<serializable_literal_value, u8>;

    std::vector<std::pair<chunk const*, std::vector<serializable_literal>>> aggregated_chunks;
    std::vector<serializable_literal> aggregated_literals;

    std::expected<void, std::string> write(std::ofstream& out, chunk const& ch);
    std::expected<void, std::string> write_chunk(chunk const& ch, std::vector<serializable_literal> const& lits);
    std::expected<void, std::string> write_literal(serializable_literal const& lit);
    std::expected<void, std::string> traverse_chunk(chunk const& ch);

    template<typename T>
    requires std::is_trivially_constructible_v<T>
    static void write_bytes(std::vector<u8>& bytes, T val) {
      static constexpr size_t size = sizeof(T);
      std::array<u8, size> bytes_of_val = std::bit_cast<std::array<u8, size>>(val);
      bytes.insert(bytes.end(), bytes_of_val.begin(), bytes_of_val.end());
    }
  };
}