#pragma once

namespace tinystl {

template <typename T> class function;

template <typename Return, typename... Args> class function<Return(Args...)> {
public:
  function() {
  }

  template <typename T> function(T functor) {
    m_func = [](const void *user, Args... args) -> Return {
      const T &func = *static_cast<const T *>(user);
      return func(static_cast<Args &&>(args)...);
    };

    m_dtor = [](void *user) {
      T &func = *static_cast<T *>(user);
      func.~T();
    };

    new (tinystl::placeholder(), m_storage) T(static_cast<T &&>(functor));
  }

  ~function() {
    if (m_dtor)
      m_dtor(m_storage);
  }

  Return operator()(Args... args) const {
    return m_func(m_storage, static_cast<Args &&>(args)...);
  }

  explicit operator bool() {
    return m_func != nullptr;
  }

  using Func = Return (*)(const void *, Args...);
  Func m_func = nullptr;
  using Dtor = void (*)(void *);
  Dtor m_dtor = nullptr;
  union {
    void *m_storage[8];
  };
};

} // namespace tinystl