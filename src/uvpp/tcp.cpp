#include "uvpp/tcp.hpp"

namespace uv {
tcp::data::data(uv_tcp_t* native_tcp) : _native_tcp(native_tcp) {
}

tcp::data::~data() {
  delete _native_tcp;
}

tcp::tcp(uv_loop_t* native_loop, uv_tcp_t* native_tcp)
    : stream(native_tcp, new data(native_tcp)), _native_tcp(native_tcp) {
  error::test(uv_tcp_init(native_loop, native_tcp));
}

tcp::tcp(uv_loop_t* native_loop) : tcp(native_loop, new uv_tcp_t()) {
}

tcp::tcp(uv_tcp_t* native_tcp) : tcp(uv_default_loop(), native_tcp) {
}

tcp::tcp() : tcp(uv_default_loop(), new uv_tcp_t()) {
}

tcp::operator uv_tcp_t*() noexcept {
  return _native_tcp;
}

tcp::operator const uv_tcp_t*() const noexcept {
  return _native_tcp;
}

void tcp::accept(tcp& client, std::function<void(uv::error)> cb) {
  stream::accept(client);

#ifdef UVPP_SSL_INCLUDE
  if (!_ssl_context) {
    cb(uv::error{0});
    return;
  }

  client.useSSL(*_ssl_context);
  client.handshake(cb);
#else
  cb(uv::error{0});
#endif
}

#ifdef UVPP_TASK_INCLUDE
task<void> tcp::accept(tcp& client) {
  return task<void>::create([this, &client](auto& resolve, auto& reject) {
    accept(client, [&resolve, &reject](auto error) {
      if (error) {
        reject(std::make_exception_ptr(error));
      } else {
        resolve();
      }
    });
  });
}
#endif

void tcp::nodelay(bool enable) {
  uv_tcp_nodelay(*this, enable);
}

void tcp::simultaneousAccepts(bool enable) {
  uv_tcp_simultaneous_accepts(*this, enable);
}

void tcp::bind(const sockaddr* addr, unsigned int flags) {
  error::test(uv_tcp_bind(*this, addr, flags));
}

void tcp::bind4(const char* ip, int port, unsigned int flags) {
  sockaddr_in addr;
  uv_ip4_addr(ip, port, &addr);
  bind((const sockaddr*)&addr, flags);
}

void tcp::bind6(const char* ip, int port, unsigned int flags) {
  sockaddr_in6 addr;
  uv_ip6_addr(ip, port, &addr);
  bind((const sockaddr*)&addr, flags);
}

std::string ipname(uv_tcp_t* tcp, decltype(uv_tcp_getsockname) getsockname) {
  sockaddr_storage addr;
  int addr_len = sizeof(decltype(addr));

  error::test(getsockname(tcp, (sockaddr*)&addr, &addr_len));

  if (addr_len == sizeof(sockaddr_in)) {
    char ipstr[sizeof("255.255.255.255")] = {0};
    size_t ipstr_len = sizeof(ipstr);

    uv_ip4_name((sockaddr_in*)&addr, ipstr, ipstr_len);
    return {ipstr, ipstr_len};
  } else if (addr_len == sizeof(sockaddr_in6)) {
    return {};
  } else {
    throw error{"unknown ip type"};
  }
}

std::string tcp::sockname() {
  return ipname(_native_tcp, uv_tcp_getsockname);
}

std::string tcp::peername() {
  return ipname(_native_tcp, uv_tcp_getpeername);
}

void tcp::connect(uv::dns::addrinfo addr, std::function<void(uv::error)> cb) {
  auto _connect = [this](uv::dns::addrinfo addr, std::function<void(uv::error)>& cb) {
    struct data_t : public uv::detail::req::data {
      std::function<void(uv::error)> cb;
    };
    using req_t = uv::req<uv_connect_t, data_t>;

    auto req = new req_t();
    auto data = req->dataPtr();
    data->cb = cb;

    error::test(uv_tcp_connect(*req, *this, addr->ai_addr, [](uv_connect_t* req, int status) {
      auto data = req_t::dataPtr(req);
      auto cb = std::move(data->cb);
      delete data->req;

      cb(uv::error{status});
    }));
  };

#ifdef UVPP_SSL_INCLUDE
  if (!_ssl_state) {
    _connect(addr, cb);
    return;
  }

  std::function<void(uv::error)> cb_ssl = [this, cb{std::move(cb)}](auto error) {
    if (error) {
      cb(error);
      return;
    }

    handshake(std::move(cb));
  };
  _connect(addr, cb_ssl);
#else
  _connect(addr, cb);
#endif
}

#ifdef UVPP_TASK_INCLUDE
task<void> tcp::connect(uv::dns::addrinfo addr) {
  return task<void>::create([this, &addr](auto& resolve, auto& reject) {
    connect(addr, [&resolve, &reject](auto error) {
      if (error) {
        reject(std::make_exception_ptr(error));
      } else {
        resolve();
      }
    });
  });
}
#endif

void tcp::connect(const std::string& node, const std::string& service, std::function<void(uv::error)> cb) {
  uv::dns::getaddrinfo(node, service, [this, cb](auto addr, auto error) {
    if (error) {
      cb(error);
      return;
    }

    connect(addr, cb);
  });
}

#ifdef UVPP_TASK_INCLUDE
task<void> tcp::connect(const std::string& node, const std::string& service) {
  auto addr = co_await uv::dns::getaddrinfo(node, service);

  co_await connect(addr);
}
#endif

void tcp::connect(const std::string& node, short port, std::function<void(uv::error)> cb) {
  connect(node.data(), std::to_string(port).data(), cb);
}

#ifdef UVPP_TASK_INCLUDE
task<void> tcp::connect(const std::string& node, short port) {
  co_await connect(node, std::to_string(port));
}
#endif
} // namespace uv
