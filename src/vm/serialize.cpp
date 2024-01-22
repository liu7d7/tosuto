#include "serialize.h"

namespace ami::vm {
  std::expected<void, std::string> serializer::write(std::ofstream& out, chunk const& ch) {
    write_bytes(interned_strings, u32(get_interned_string_backing_array().size()));
    for (auto const& it : get_interned_string_backing_array()) {
      write_bytes(interned_strings, u32(it.size()));
      for (auto const& byte : it) {
        write_bytes(interned_strings, byte);
      }
    }

    ami_discard(traverse_chunk(ch));

    write_bytes(chunks, u32(aggregated_chunks.size()));
    for (auto const& [chunk, lits] : aggregated_chunks) {
      ami_discard(write_chunk(*chunk, lits));
    }

    write_bytes(literals, aggregated_literals.size());
    for (auto const& lit : aggregated_literals) {
      ami_discard(write_literal(lit));
    }

    std::vector<u8> merged;
    merged.reserve(interned_strings.size() + literals.size() + chunks.size());
    merged.insert(merged.end(), interned_strings.begin(), interned_strings.end());
    merged.insert(merged.end(), literals.begin(), literals.end());
    merged.insert(merged.end(), chunks.begin(), chunks.end());

    out.write(reinterpret_cast<char const*>(merged.data()), std::streamsize(merged.size()));

    return {};
  }

  std::expected<void, std::string> serializer::traverse_chunk(const chunk& ch) {
    std::vector<serializable_literal> ch_literals;

    for (auto const& it : ch.literals) {
      if (it.is<value::fn>()) {
        ami_discard(traverse_chunk(it.get<value::fn>().ch->first));
        ch_literals.emplace_back(serializable_literal_value {std::make_pair(it.get<value::fn>().ch->second,
                                                 aggregated_chunks.size() - 1)}, it.val.index());
      } else if (it.is<value::num>()) {
        ch_literals.emplace_back(it.get<value::num>(), it.val.index());
      } else if (it.is<value::str>()) {
        ch_literals.emplace_back(it.get<value::str>().index, it.val.index());
      } else {
        return std::unexpected{"Couldn't serialize " + it.to_string()};
      }
    }

    aggregated_chunks.emplace_back(&ch, ch_literals);

    return {};
  }

  std::expected<void, std::string>
  serializer::write_chunk(chunk const& ch, std::vector<serializable_literal> const& lits) {
    write_bytes(chunks, u32(ch.name.index));
    auto index_to_start_of_literals = std::distance(aggregated_literals.begin(), aggregated_literals.insert(aggregated_literals.end(), lits.begin(), lits.end()));
    auto num_of_literals = lits.size();
    write_bytes(chunks, u32(index_to_start_of_literals));
    write_bytes(chunks, u32(num_of_literals));
    auto num_of_bytes_in_bytecode = ch.data.size();
    write_bytes(chunks, u32(num_of_bytes_in_bytecode));
    for (auto it : ch.data) {
      write_bytes(chunks, it);
    }

    return {};
  }

  std::expected<void, std::string>
  serializer::write_literal(const serializer::serializable_literal& lit) {
    write_bytes(literals, lit.second);
    switch (lit.second) {
      case 0: {
        write_bytes(literals, get<double>(lit.first));
        break;
      }
      case 5: {
        write_bytes(literals, u32(get<size_t>(lit.first)));
        break;
      }
      case 6: {
        auto fn = get<std::pair<u8, size_t>>(lit.first);
        u8 arity = fn.first;
        size_t index_to_chunk = fn.second;
        write_bytes(literals, arity);
        write_bytes(literals, u32(index_to_chunk));
        break;
      }
      default: {
        return std::unexpected{"Couldn't serialize literal " + std::to_string(lit.second)};
      }
    }

    return {};
  }
}