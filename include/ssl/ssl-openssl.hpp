#pragma once

#include "./ssl.hpp"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

namespace ssl::openssl {
class openssl_error : public ssl::ssl_error {
public:
  openssl_error(const std::string& msg);

  openssl_error(int code);

  static void test(int code);
};

struct driver : public ssl::driver {
public:
  struct shared {
    std::string _alpn_protocols;
    std::function<bool(std::string_view)> _alpn_callback;
  };

  struct state : public ssl::driver::state {
  public:
    state(SSL_CTX* native_context, ssl::mode mode);

    virtual ~state() override;

    void handshake(std::function<void()>& on_handshake) override;

    bool ready() override;

    void decrypt(std::string_view data) override;

    void encrypt(std::string_view data, std::function<void(std::exception_ptr)> cb) override;

    void onReadDecrypted(std::function<void(std::string_view)>& value) override;

    void onWriteEncrypted(std::function<void(std::string&&, std::function<void(std::exception_ptr)>)>& value) override;

    std::string_view protocol() override;

  private:
    ssl::mode _mode;

    SSL_CTX* _native_context;
    SSL* _native_state;
    BIO* _read;
    BIO* _write;

    std::function<void()> _on_handshake;
    bool _on_handshake_called = false;

    std::function<void(std::string_view)> _on_read_decrypted;
    std::function<void(std::string&&, std::function<void(std::exception_ptr)>)> _on_write_encrypted;

    int getError(int rc);

    void sendPending(std::function<void(std::exception_ptr)> cb = [](auto) {});

    void handshake();
  };

  struct context : public ssl::driver::context {
  public:
    context(ssl::mode mode);

    virtual ~context() override;

    std::shared_ptr<ssl::driver::state> createState() override;

    void useCertificateFile(const char* path, ssl::filetype type) override;

    void usePrivateKeyFile(const char* path, ssl::filetype type) override;

    void useCertificateChainFile(const char* path) override;

    void useALPNProtocols(const std::vector<std::string>& protocols) override;

    void useALPNCallback(std::function<bool(std::string_view)>& cb) override;

  private:
    static constexpr int NO_SSL = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    static constexpr int NO_TLS = SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 /* | SSL_OP_NO_TLSv1_2 */;
    static constexpr int NO_OLD_PROTOCOLS = NO_SSL | NO_TLS;

    ssl::mode _mode;

    SSL_CTX* _native_context;

    shared _shared;

    int _certkey_count = 0;

    void validateCertificateAndPrivateKey();

    static int mapFiletype(ssl::filetype type);
  };

  std::shared_ptr<ssl::driver::context> getContext(ssl::mode mode) const override;

private:
};
} // namespace ssl::openssl
