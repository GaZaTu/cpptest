#pragma once

#include <limits>
#include <bitset>
#include <string>
#include <string_view>

namespace binary {
template <auto Start, auto End, auto Inc, class F>
constexpr void constexpr_for(F&& f) {
  if constexpr (Start < End) {
    if (!f(std::integral_constant<decltype(Start), Start>())) {
      return;
    }

    constexpr_for<Start + Inc, End, Inc>(f);
  }
}

template <size_t N>
auto bitset_max(std::bitset<N>&& b = {}) {
  return b.set().to_ullong();
}

template <size_t N0>
auto bitset_concat(const std::bitset<N0>& b0) {
  return b0;
}

template <size_t N0, size_t N1, size_t... Ns>
auto bitset_concat(const std::bitset<N0>& b0, const std::bitset<N1>& b1, const std::bitset<Ns>&... bs) {
  return bitset_concat(std::bitset<N0 + N1>{b0.to_string() + b1.to_string()}, std::forward<const std::bitset<Ns>&>(bs)...);
}

template <size_t Length>
auto bitset_slice(const std::string& bstr, size_t start) {
  return std::bitset<Length>{bstr.substr(start, Length)};
}

template <size_t Length, size_t N>
auto bitset_slice(const std::bitset<N>& b, size_t start) {
  return bitset_slice<Length>(b.to_string(), start);
}

template <size_t N>
void bitset_append_to_bytes(const std::bitset<N>& b, std::string& bytes, size_t& bytes_idx) {
  constexpr size_t uint8_bitc = std::numeric_limits<uint8_t>::digits;

  std::string bstr = b.to_string();

  constexpr_for<0ul, N, uint8_bitc>([&](auto i) {
    ((uint8_t&)bytes[bytes_idx]) = (uint8_t)bitset_slice<uint8_bitc>(bstr, i).to_ulong();
    bytes_idx += sizeof(uint8_t);
    return true;
  });
}

template <size_t N>
void bitset_append_to_bytes(const std::bitset<N>& b, std::string& bytes) {
  constexpr size_t uint8_bitc = std::numeric_limits<uint8_t>::digits;

  std::string bstr = b.to_string();

  constexpr_for<0ul, N, uint8_bitc>([&](auto i) {
    bytes += (uint8_t)bitset_slice<uint8_bitc>(bstr, i).to_ulong();
    return true;
  });
}

template <typename T>
void int_append_to_bytes(T& i, std::string& bytes) {
  std::bitset<std::numeric_limits<T>::digits> b = i;
  bitset_append_to_bytes(b, bytes);
}

template <size_t N>
bool bitset_read_from_bytes(std::bitset<N>& b, std::string_view bytes, size_t& bytes_idx) {
  constexpr size_t uint8_bitc = std::numeric_limits<uint8_t>::digits;

  auto bytes_len_at_idx = bytes.substr(bytes_idx).length();
  if (bytes_len_at_idx == 0) {
    return false;
  }
  if (bytes_len_at_idx < (N / uint8_bitc)) {
    throw std::runtime_error{"FRICK"};
  }

  std::string bstr;

  constexpr_for<0ul, N, uint8_bitc>([&](auto i) {
    bstr += std::bitset<uint8_bitc>{(uint8_t)bytes[bytes_idx]}.to_string();
    bytes_idx += sizeof(uint8_t);
    return true;
  });

  b = std::bitset<N>{bstr};
  return true;
}

template <size_t N>
bool bitset_read_from_bytes(std::bitset<N>& b, std::string_view bytes) {
  size_t bytes_idx = 0;
  return bitset_read_from_bytes(b, bytes, bytes_idx);
}

template <typename T>
bool int_read_from_bytes(T& i, std::string_view bytes) {
  std::bitset<std::numeric_limits<T>::digits> b;
  size_t bytes_idx = 0;
  bool r = bitset_read_from_bytes(b, bytes, bytes_idx);
  if (r) {
    i = (T)b.to_ullong();
  }
  return r;
}

struct reader {
public:
  reader(std::string_view bytes) {
    _bytes = bytes;
  }

  template <size_t N, typename I, typename O>
  using conditional_int_t = std::conditional_t<N <= std::numeric_limits<I>::digits, I, O>;

  template <size_t N>
  using conditional_uint64_t = conditional_int_t<N, uint64_t, void>;
  template <size_t N>
  using conditional_uint32_t = conditional_int_t<N, uint32_t, conditional_uint64_t<N>>;
  template <size_t N>
  using conditional_uint16_t = conditional_int_t<N, uint16_t, conditional_uint32_t<N>>;
  template <size_t N>
  using conditional_uint8_t = conditional_int_t<N, uint8_t, conditional_uint16_t<N>>;

  template <size_t N>
  using fitting_uint_t = conditional_uint8_t<N>;

  template <size_t N>
  std::bitset<N> read() {
    std::string result_as_bitstr;

    size_t bitc = N;

    if (_current_byte_bit_index > 0) {
      result_as_bitstr += _current_byte.to_string().substr(_current_byte_bit_index, N);
      _current_byte_bit_index += result_as_bitstr.size();
      if (_current_byte_bit_index >= 8) {
        _current_byte_bit_index = 0;
      }

      if (result_as_bitstr.size() <= bitc) {
        bitc -= result_as_bitstr.size();
      }
    }

    for (size_t i = 0; i < bitc; i += 8) {
      bitset_read_from_bytes(_current_byte, _bytes, _byte_index);
      if ((bitc - i) < 8) {
        _current_byte_bit_index = bitc - i;
        result_as_bitstr += _current_byte.to_string().substr(0, _current_byte_bit_index);
      } else {
        result_as_bitstr += _current_byte.to_string();
      }
    }

    std::bitset<N> result_as_bitset{result_as_bitstr};
    return result_as_bitset;
  }

  template <size_t N>
  fitting_uint_t<N> readUInt() {
    return read<N>().to_ullong();
  }

  std::string_view readString(size_t length) {
    std::string_view result = _bytes.substr(_byte_index, length);
    _byte_index += result.size();
    return result;
  }

  std::string_view bytes() const {
    return _bytes;
  }

  void seek(size_t index) {
    _byte_index = index;
  }

  void move(int64_t by) {
    _byte_index += by;
  }

  size_t size() const {
    return _bytes.size();
  }

  size_t index() const {
    return _byte_index;
  }

  size_t remaining() const {
    return size() - _byte_index;
  }

private:
  std::string_view _bytes;
  size_t _byte_index = 0;

  std::bitset<8> _current_byte;
  uint8_t _current_byte_bit_index = 0;
};

struct writer {
public:
  writer() { }

  template <size_t N>
  void write(const std::bitset<N>& data_as_bitset) {
    std::string data_as_bitstr = data_as_bitset.to_string();
    size_t data_as_bitstr_index = 0;

    size_t bitc = N;

    if (_current_bytestr.size() > 0) {
      auto bitstr = data_as_bitstr.substr(data_as_bitstr_index, 8 - _current_bytestr.size());
      data_as_bitstr_index += bitstr.size();

      _current_bytestr += bitstr;
      tryWriteCurrentBytestr();
    }

    for (; data_as_bitstr_index < bitc; data_as_bitstr_index += 8) {
      _current_bytestr = data_as_bitstr.substr(data_as_bitstr_index, 8);
      tryWriteCurrentBytestr();
    }
  }

  template <size_t N>
  void writeUInt(reader::fitting_uint_t<N> data) {
    write(std::bitset<N>{data});
  }

  void writeString(std::string_view data) {
    _bytes += data;
  }

  void writeString(char data) {
    _bytes += data;
  }

  std::string& bytes() {
    return _bytes;
  }

  std::string_view bytes() const {
    return _bytes;
  }

  void reserve(size_t length) {
    _bytes.reserve(length);
  }

  void seek(size_t index) {
    _byte_index = index;
  }

  void move(int64_t by) {
    _byte_index += by;
  }

  size_t size() const {
    return _bytes.size();
  }

  size_t index() const {
    return _byte_index;
  }

private:
  std::string _bytes;
  size_t _byte_index = 0;

  std::string _current_bytestr;

  void tryWriteCurrentBytestr() {
    if (_current_bytestr.size() < 8) {
      return;
    }

    _bytes.resize(_bytes.size() + 1);
    bitset_append_to_bytes(std::bitset<8>{_current_bytestr}, _bytes, _byte_index);
    _current_bytestr = "";
  }
};
} // namespace binary
