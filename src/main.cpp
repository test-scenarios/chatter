#include "config.hpp"
#include "explain.hpp"
#include <chrono>
#include <iostream>

namespace program {

struct chatter {
  //
  // the type of executor which will provide default invocation of handlers
  //

  using executor_type = net::io_context::executor_type;

  using duration = std::chrono::system_clock::duration;

  /// @param exec is the default executor
  /// @note exec will also be the basis of all internal operation, except they
  /// will be invoked on an inner private strand
  chatter(executor_type exec);

  template <class CompletionToken>
  auto async_say_after(std::string message, duration after,
                       CompletionToken &&token)
      -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code));

  struct report {};

  template <class CompletionToken>
  auto async_report(report &target, CompletionToken &&token)
      -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code));

  auto get_executor() -> executor_type;

  //
  // The object will have some kind of implementation
  //
  struct impl {

    impl(executor_type exec) : internal_executor_(exec) {}

    auto default_exec() { return internal_executor_.get_inner_executor(); }

    auto internal_exec() { return internal_executor_; }

  private:
    template <class Handler, class... Guards>
    struct job_op : boost::asio::coroutine {

      struct state {

        template <class Exec>
        state(Handler h, Exec e) : handler_(std::move(h)), timer_(e) {}

        Handler handler_;
        net::system_timer timer_;
      };

      template <class HandlerArg>
      job_op(impl *self, std::string message,
             std::chrono::system_clock::time_point when, HandlerArg &&handler,
             Guards const &... guards)
          : impl_(self),
            state_(std::make_unique<state>(std::forward<HandlerArg>(handler),
                                           get_executor())),
            message_(std::move(message)), when_(when), guards_(guards...) {}

      void operator()(boost::system::error_code const& ec = {}, std::size_t = 0) {
        auto &s = *state_;
        BOOST_ASIO_CORO_REENTER(this) {
          s.timer_.expires_at(when_);
          BOOST_ASIO_CORO_YIELD
          s.timer_.async_wait(std::move(this));
          if (!ec) {
//            impl_->current_report_; // update the report
            std::cout << message_ << std::endl;
          }
          // now we need to complete but we need to do it on the correct
          // executor
          {
            auto e =
                net::get_associated_executor(s.handler_, impl_->default_exec());
            net::post(e, beast::bind_front_handler(std::move(s.handler_), ec));
          }
        }
      }

      using executor_type = net::strand<chatter::executor_type>;

      auto get_executor() const -> executor_type {
        return impl_->internal_exec();
      }

      impl *impl_;
      std::unique_ptr<state> state_;
      std::string message_;
      std::chrono::system_clock::time_point when_;
      std::tuple<Guards...> guards_;
    };

  public:
    template <class Handler>
    auto make_op(std::string message,
                 std::chrono::system_clock::time_point when,
                 Handler &&handler) {
      auto handler_exec = net::get_associated_executor(handler, default_exec());
      auto handler_guard = net::make_work_guard(handler_exec);
      auto internal_guard = net::make_work_guard(default_exec());

      return job_op<std::decay_t<Handler>, decltype(handler_guard),
                    decltype(internal_guard)>(this, std::move(message), when,
                                              std::forward<Handler>(handler),
                                              handler_guard, internal_guard);
    }

    net::strand<executor_type> internal_executor_;

    // internal state - must be accessed concurrently
    report current_report_;
  };

  std::unique_ptr<impl> impl_;
};

chatter::chatter(executor_type exec) : impl_(std::make_unique<impl>(exec)) {}

template <class CompletionToken>
auto chatter::async_say_after(std::string message, duration after,
                              CompletionToken &&token)
    -> BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(error_code)) {
  // we need to ensure that the operation is launched on the correct internal
  // strand thus we will not use the async_compose mechanism we will use the
  // more fundamental building blocks

  auto launch = [message, after, this](auto &&handler) mutable {
    auto op = impl_->make_op(std::move(message),
                             std::chrono::system_clock::now() + after,
                             std::forward<decltype(handler)>(handler));

    // ensure the op starts on the internal executor
    net::dispatch(impl_->internal_exec(), std::move(op));
  };

  return net::async_initiate<CompletionToken, void(error_code)>(
      std::move(launch), token);
}

using namespace std::literals;

int run() {
  net::io_context ioc;

  auto chat = chatter(ioc.get_executor());
  chat.async_say_after("Hello, ", 1s, [](error_code) {});
  chat.async_say_after("World!", 2s, [](error_code) {});
  chatter::report rep;
  chat.async_report(rep, [](error_code) {});

  ioc.run();
  ioc.reset();
  chat.async_report(rep, [](error_code) {});
  ioc.run();

  return 0;
}
} // namespace program

int main() {
  try {
    return program::run();
  } catch (...) {
    std::cerr << program::explain() << std::endl;
    return 127;
  }
}