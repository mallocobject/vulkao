#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace vkao {
template <typename T>
class Ref {
    template <typename U>
    friend class Ref;

  public:
    constexpr Ref() noexcept = default;
    constexpr Ref(std::nullptr_t) noexcept {
    }

    explicit Ref(T *ptr) noexcept : ptr_(ptr) {
        add_ref();
    }

    Ref(const Ref &other) noexcept : ptr_(other.ptr_) {
        add_ref();
    }

    Ref(Ref &&other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    template <typename U>
        requires std::convertible_to<U *, T *>
    Ref(const Ref<U> &other) noexcept : ptr_(other.get()) {
        add_ref();
    }

    template <typename U>
        requires std::convertible_to<U *, T *>
    Ref(Ref<U> &&other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    ~Ref() {
        release();
    }

    Ref &operator=(const Ref &other) noexcept {
        if (this != &other) {
            Ref tmp(other);
            swap(tmp);
        }
        return *this;
    }

    Ref &operator=(Ref &&other) noexcept {
        if (this != &other) {
            Ref tmp(std::move(other));
            swap(tmp);
        }
        return *this;
    }

    Ref &operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    T *get() const noexcept {
        return ptr_;
    }

    T *operator->() const noexcept {
        return ptr_;
    }

    T &operator*() const noexcept {
        return *ptr_;
    }

    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    bool operator==(T *ptr) const noexcept {
        return ptr_ == ptr;
    }

    template <typename U>
        requires std::convertible_to<U *, T *>
    bool operator==(const Ref<U> &other) const noexcept {
        return ptr_ == other.ptr_;
    }

    void reset(T *ptr = nullptr) noexcept {
        if (ptr == ptr_) {
            return;
        }
        release();
        ptr_ = ptr;
        add_ref();
    }

    void swap(Ref &other) noexcept {
        std::swap(ptr_, other.ptr_);
    }

  private:
    void add_ref() {
        if (ptr_) {
            ptr_->add_ref();
        }
    }

    void release() {
        if (ptr_) {
            ptr_->release();
        }
    }

    T *ptr_{nullptr};
};

class RefCounted {
    template <typename U>
    friend class Ref;

  public:
    uint32_t ref_count() const noexcept {
        return ref_count_.load(std::memory_order_relaxed);
    }

  protected:
    RefCounted() = default;

    virtual ~RefCounted() = default;

    RefCounted(const RefCounted &) = delete;

  private:
    void add_ref() const noexcept {
        ref_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void release() const noexcept {
        if (ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }

    mutable std::atomic<uint32_t> ref_count_{0};
};

template <typename T, typename... Args>
    requires std::constructible_from<T, Args...>
[[nodiscard]] Ref<T> make_ref(Args &&...args) {
    return Ref<T>(new T(std::forward<Args>(args)...));
}
} // namespace vkao