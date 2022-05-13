#pragma once

#include "./common.hpp"
#include "./base64.hpp"
#include "./binary-utils.hpp"
#include <random>
#include <type_traits>

namespace websocket {
namespace http {
namespace detail {
using random_bytes_engine = std::independent_bits_engine<std::default_random_engine, std::numeric_limits<uint8_t>::digits, uint8_t>;
}

::http::request upgrade(::http::request&& request) {
  std::random_device rd;
  detail::random_bytes_engine rng{(detail::random_bytes_engine::result_type)rd()};
  std::vector<uint8_t> rnd_bytes(16);
  std::generate(std::begin(rnd_bytes), std::end(rnd_bytes), std::ref(rng));
  std::string rnd_string = base64::encode(rnd_bytes);

  request.method = ::http::GET;
  request.headers.emplace("connection", "upgrade");
  request.headers.emplace("upgrade", "websocket");
  request.headers.emplace("sec-websocket-version", "13");
  request.headers.emplace("sec-websocket-key", rnd_string);

  return request;
}

// ::http::response respond(const ::http::request& request) {
//   return {
//     .status = ::http::status::SWITCHING_PROTOCOLS,
//     .headers = {
//       {"connection", "Upgrade"},
//       {"upgrade", "websocket"},
//       {"sec-websocket-accept", "PLACEHOLDER_SHA1"},
//     },
//   };
// }
}

enum status_code_t {
  CLOSE_NORMAL              = 1000,
  CLOSE_SHUTDOWN            = 1001,
  CLOSE_PROTOCOL_ERROR      = 1002,
  CLOSE_UNSUPPORTED_PAYLOAD = 1003,
  CLOSE_WITHOUT_STATUS_CODE = 1005,
  CLOSE_INVALID_PAYLOAD     = 1007,
  CLOSE_POLICY_VIOLATION    = 1008,
  CLOSE_PAYLOAD_TOO_BIG     = 1009,
  CLOSE_EXPECTED_EXTENSION  = 1010,
  CLOSE_SERVER_ERROR        = 1011,
};

namespace frame {
enum opcode_t {
  OP_CONTINUE = 0x00,
  OP_TEXT     = 0x01,
  OP_BINARY   = 0x02,
  OP_CLOSE    = 0x08,
  OP_PING     = 0x09,
  OP_PONG     = 0x0a,
  OP_NONE     = std::numeric_limits<uint8_t>::max(),
};

enum maskbit_t {
  FROM_CLIENT = true,
  FROM_SERVER = false,
};

enum finbit_t {
  IS_FINAL       = true,
  IS_CONTINUEING = false,
};

std::string write(uint8_t op, std::string_view payload = "", bool from_client = FROM_CLIENT, bool is_final = IS_FINAL) {
  binary::writer frame;

  frame.writeUInt<01>(is_final);
  frame.writeUInt<01>(0);
  frame.writeUInt<01>(0);
  frame.writeUInt<01>(0);
  frame.writeUInt<04>(op);

  frame.writeUInt<01>(from_client);

  size_t payload_len_max = binary::bitset_max<07>();
  size_t extended_payload_len16_max = binary::bitset_max<16>();

  if (payload.size() > (payload_len_max - 2)) {
    if (payload.size() > extended_payload_len16_max) {
      frame.writeUInt<07>(payload_len_max);
      frame.writeUInt<64>(payload.size());
    } else {
      frame.writeUInt<07>(payload_len_max - 1);
      frame.writeUInt<16>(payload.size());
    }
  } else {
    frame.writeUInt<07>(payload.size());
  }

  frame.reserve(frame.size() + payload.size());

  if (from_client) {
    std::random_device rd;
    std::mt19937 rng{rd()};
    std::uniform_int_distribution<uint32_t> distrib{std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max()};
    uint32_t masking_key_value = distrib(rng);

    std::bitset<32> masking_key{masking_key_value};
    frame.write(masking_key);

    for (size_t i = 0; i < payload.size(); i++) {
      size_t j = i % (sizeof(uint32_t) / sizeof(uint8_t));
      std::bitset<8> masking_key_octet = binary::bitset_slice<8>(masking_key, j * 8);
      char tc = payload[i] ^ masking_key_octet.to_ulong();

      frame.writeString(tc);
    }
  } else {
    frame.writeString(payload);
  }

  auto result = std::move(frame.bytes());
  return result;
}

struct read_result {
public:
  opcode_t opcode = OP_NONE;
  uint64_t payload_len = std::numeric_limits<uint64_t>::max();
  std::string payload;
  bool mask = true;
  uint32_t masking_key = std::numeric_limits<uint32_t>::max();
  bool fin = true;
};

int read(std::string_view _chunk, read_result& frame) {
  binary::reader chunk{_chunk};

  if (frame.opcode == OP_NONE) {
    frame.fin = (bool)chunk.readUInt<1>();
    chunk.readUInt<1>();
    chunk.readUInt<1>();
    chunk.readUInt<1>();
    frame.opcode = (opcode_t)chunk.readUInt<4>();

    if (!chunk.remaining()) {
      return 0;
    }
  }

  if (frame.payload_len == std::numeric_limits<decltype(frame.payload_len)>::max()) {
    frame.mask = chunk.readUInt<1>();
    frame.payload_len = chunk.readUInt<7>();

    switch (frame.payload_len) {
      case 0x7F:
        frame.payload_len = chunk.readUInt<64>();
        break;
      case 0x7E:
        frame.payload_len = chunk.readUInt<16>();
        break;
    }

    frame.payload.reserve(frame.payload_len);

    if (!chunk.remaining()) {
      return 0;
    }
  }

  if (frame.mask) {
    if (frame.masking_key == std::numeric_limits<decltype(frame.masking_key)>::max()) {
      frame.masking_key = chunk.readUInt<32>();

      if (!chunk.remaining()) {
        return 0;
      }
    }
  }

  auto payload_existing_size = frame.payload.size();
  auto payload = _chunk.substr(chunk.index(), frame.payload_len - payload_existing_size);

  if (frame.mask) {
    std::bitset<32> masking_key{frame.masking_key};

    for (size_t i = 0; i < payload.size(); i++) {
      size_t j = (payload_existing_size + i) % (sizeof(uint32_t) / sizeof(uint8_t));
      std::bitset<8> masking_key_octet = binary::bitset_slice<8>(masking_key, j * 8);
      char tc = payload[i] ^ masking_key_octet.to_ulong();

      frame.payload += tc;
    }
  } else {
    frame.payload += payload;
  }

  if (frame.payload.size() == frame.payload_len) {
    return chunk.index() + payload.size();
  } else {
    return 0;
  }
}

std::string create_close_payload(uint16_t status_code = CLOSE_NORMAL, std::string_view status_text = "") {
  std::string result;
  binary::int_append_to_bytes(status_code, result);
  result += status_text;
  return result;
}

struct parse_close_result {
  uint16_t status_code = CLOSE_WITHOUT_STATUS_CODE;
  std::string status_text;
};

parse_close_result parse_close_payload(read_result& frame) {
  parse_close_result result;
  if (binary::int_read_from_bytes(result.status_code, frame.payload)) {
    result.status_text = frame.payload.substr(sizeof(result.status_code));
  } else {
    result.status_text = frame.payload;
  }
  return result;
}
}

enum maskbit_t {
  IS_CLIENT = frame::FROM_CLIENT,
  IS_SERVER = frame::FROM_SERVER,
};

struct message {
public:
  enum kind_t {
    TEXT   = frame::OP_TEXT,
    BINARY = frame::OP_BINARY,
    CLOSE  = frame::OP_CLOSE,
    PING   = frame::OP_PING,
    PONG   = frame::OP_PONG,
  };

  kind_t kind = TEXT;
  std::string payload;
};

std::ostream& operator<<(std::ostream& os, message::kind_t kind) {
  switch (kind) {
    case message::TEXT:
      os << "TEXT";
      break;
    case message::BINARY:
      os << "BINARY";
      break;
    case message::CLOSE:
      os << "CLOSE";
      break;
    case message::PING:
      os << "PING";
      break;
    case message::PONG:
      os << "PONG";
      break;
  }
  return os;
}

struct handler {
public:
  handler(bool is_client) {
    _is_client = is_client;
  }

  void onRecv(std::function<void(const message&)> on_recv) {
    _on_recv = std::move(on_recv);
  }

  void onSend(std::function<void(std::string_view)> on_send) {
    _on_send = std::move(on_send);
  }

  void feed(std::string_view chunk) {
    int idx = 0;
    while (idx < chunk.size()) {
      frame::read_result wsframe_prev;
      if (!_current_frame.fin) {
        wsframe_prev = std::move(_current_frame);
      }

      int r = frame::read(chunk.substr(idx), _current_frame);
      if (!r) {
        std::cerr << chunk << std::endl;
        break;
      }
      idx += r;

      if (_current_frame.opcode == frame::OP_CONTINUE) {
        wsframe_prev.payload += _current_frame.payload;
        if (_current_frame.fin) {
          _current_frame = std::move(wsframe_prev);
          _current_frame.fin = true;
        } else {
          continue;
        }
      }

      if (_current_frame.opcode == frame::OP_CLOSE) {
        close();

        auto close_payload = frame::parse_close_payload(_current_frame);
        _current_frame.payload = std::to_string(close_payload.status_code) + close_payload.status_text;
      }

      if (_current_frame.opcode == frame::OP_PING) {
        pong();
      }

      _on_recv(message{(message::kind_t)_current_frame.opcode, _current_frame.payload});

      if (!wsframe_prev.fin) {
        _current_frame = std::move(wsframe_prev);
      } else {
        _current_frame = {};
      }
    }
  }

  void send(message&& msg) {
    _on_send(frame::write(msg.kind, msg.payload, _is_client, frame::IS_FINAL));
  }

  void send(const message& msg) {
    message _msg = msg;
    send(std::move(_msg));
  }

  void send(std::string&& payload, message::kind_t kind = message::TEXT) {
    send(message{kind, std::move(payload)});
  }

  void send(std::string_view payload, message::kind_t kind = message::TEXT) {
    send(message{kind, (std::string)payload});
  }

  void send(message::kind_t kind) {
    send(message{kind});
  }

  void close(uint16_t status_code = CLOSE_NORMAL, std::string_view status_text = "") {
    if (_sent_close) {
      return;
    }

    send(message{message::CLOSE, frame::create_close_payload(status_code, status_text)});
    _sent_close = true;
  }

  void ping() {
    send(message::PING);
  }

  void pong() {
    send(message::PONG);
  }

private:
  bool _is_client = false;
  bool _sent_close = false;

  frame::read_result _current_frame;

  std::function<void(const message&)> _on_recv;
  std::function<void(std::string_view)> _on_send;
};
}
