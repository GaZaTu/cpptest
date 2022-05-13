#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#ifdef SSLPP_TASK_INCLUDE
#include SSLPP_TASK_INCLUDE
#endif

namespace ssl {
enum method {
  TLS1_2,
  TLS1_3,
};

enum filetype {
  PEM,
};

enum mode {
  CONNECT,
  ACCEPT,
};

class ssl_error : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

struct driver {
public:
  struct state {
  public:
    virtual ~state();

    virtual void handshake(std::function<void()>& on_handshake) = 0;

    virtual bool ready() = 0;

    virtual void decrypt(std::string_view data) = 0;

    virtual void encrypt(std::string_view data, std::function<void(std::exception_ptr)> cb) = 0;

    virtual void onReadDecrypted(std::function<void(std::string_view)>& value) = 0;

    virtual void onWriteEncrypted(
        std::function<void(std::string&&, std::function<void(std::exception_ptr)>)>& value) = 0;

    virtual std::string_view protocol() = 0;
  };

  struct context {
  public:
    virtual ~context();

    virtual std::shared_ptr<state> createState() = 0;

    virtual void useCertificateFile(const char* path, ssl::filetype type) = 0;

    virtual void usePrivateKeyFile(const char* path, ssl::filetype type) = 0;

    virtual void useCertificateChainFile(const char* path) = 0;

    virtual void useALPNProtocols(const std::vector<std::string>& protocols) = 0;

    virtual void useALPNCallback(std::function<bool(std::string_view)>& cb) = 0;
  };

  virtual std::shared_ptr<context> getContext(ssl::mode mode) const = 0;
};

struct state;

struct context {
public:
  friend state;

  explicit context(ssl::driver& driver, ssl::mode mode = CONNECT);

  context(const context&) = delete;

  context(context&&) = delete;

  context& operator=(const context&) = delete;

  context& operator=(context&&) = delete;

  void useCertificateFile(const char* path, ssl::filetype type = ssl::PEM);

  void usePrivateKeyFile(const char* path, ssl::filetype type = ssl::PEM);

  void useCertificateChainFile(const char* path);

  void useALPNProtocols(const std::vector<std::string>& protocols);

  void useALPNCallback(std::function<bool(std::string_view)> cb);

  void useALPNCallback(std::vector<std::string> protocols);

private:
  std::shared_ptr<ssl::driver::context> _driver_context;
};

struct state {
public:
  state();

  explicit state(ssl::context& context);

  state(const state& other);

  state& operator=(const state& other);

  state(state&& other);

  state& operator=(state&& other);

  void handshake(std::function<void()> on_handshake);

#ifdef SSLPP_TASK_INCLUDE
  SSLPP_TASK_TYPE<void> handshake();
#endif

  bool ready();

  void decrypt(std::string_view data);

  void encrypt(std::string_view data, std::function<void(std::exception_ptr)> cb);

#ifdef SSLPP_TASK_INCLUDE
  SSLPP_TASK_TYPE<void> encrypt(std::string_view data);
#endif

  void onReadDecrypted(std::function<void(std::string_view)> value);

  void onWriteEncrypted(std::function<void(std::string&&, std::function<void(std::exception_ptr)>)> value);

  std::string_view protocol();

  operator bool();

private:
  std::shared_ptr<ssl::driver::context> _driver_context;
  std::shared_ptr<ssl::driver::state> _driver_state;
};
} // namespace ssl
