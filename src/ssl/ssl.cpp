#include "ssl/ssl.hpp"

namespace ssl {
driver::state::~state() {
  printf("");
}

driver::context::~context() {
  printf("");
}

context::context(ssl::driver& driver, ssl::mode mode) {
  _driver_context = driver.getContext(mode);
}

void context::useCertificateFile(const char* path, ssl::filetype type) {
  _driver_context->useCertificateFile(path, type);
}

void context::usePrivateKeyFile(const char* path, ssl::filetype type) {
  _driver_context->usePrivateKeyFile(path, type);
}

void context::useCertificateChainFile(const char* path) {
  _driver_context->useCertificateChainFile(path);
}

void context::useALPNProtocols(const std::vector<std::string>& protocols) {
  _driver_context->useALPNProtocols(protocols);
}

void context::useALPNCallback(std::function<bool(std::string_view)> cb) {
  _driver_context->useALPNCallback(cb);
}

void context::useALPNCallback(std::vector<std::string> protocols) {
  useALPNCallback([protocols](auto p) {
    for (const auto& protocol : protocols) {
      if (protocol == p) {
        return true;
      }
    }

    return false;
  });
}

state::state() {
}

state::state(ssl::context& context) {
  _driver_context = context._driver_context;
  _driver_state = context._driver_context->createState();
}

state::state(const state& other) {
  *this = other;
}

state& state::operator=(const state& other) {
  _driver_context = other._driver_context;
  _driver_state = other._driver_state;
  return *this;
};

state::state(state&& other) {
  *this = other;
}

state& state::operator=(state&& other) {
  _driver_context = std::move(other._driver_context);
  _driver_state = std::move(other._driver_state);
  return *this;
}

void state::handshake(std::function<void()> on_handshake) {
  _driver_state->handshake(on_handshake);
}

#ifdef SSLPP_TASK_INCLUDE
SSLPP_TASK_TYPE<void> state::handshake() {
  return SSLPP_TASK_CREATE<void>([this](auto& resolve, auto& reject) {
    handshake([&resolve, &reject]() {
      resolve();
    });
  });
}
#endif

bool state::ready() {
  return _driver_state->ready();
}

void state::decrypt(std::string_view data) {
  _driver_state->decrypt(data);
}

void state::encrypt(std::string_view data, std::function<void(std::exception_ptr)> cb) {
  _driver_state->encrypt(data, cb);
}

#ifdef SSLPP_TASK_INCLUDE
SSLPP_TASK_TYPE<void> state::encrypt(std::string_view data) {
  return SSLPP_TASK_CREATE<void>([this, data](auto& resolve, auto& reject) {
    encrypt(data, [&resolve, &reject](auto error) {
      if (error) {
        reject(error);
      } else {
        resolve();
      }
    });
  });
}
#endif

void state::onReadDecrypted(std::function<void(std::string_view)> value) {
  _driver_state->onReadDecrypted(value);
}

void state::onWriteEncrypted(std::function<void(std::string&&, std::function<void(std::exception_ptr)>)> value) {
  _driver_state->onWriteEncrypted(value);
}

std::string_view state::protocol() {
  if (!*this) {
    return {};
  }

  return _driver_state->protocol();
}

state::operator bool() {
  return _driver_state != nullptr;
}
} // namespace ssl
