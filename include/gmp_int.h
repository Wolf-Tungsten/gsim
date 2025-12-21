#ifndef GSIM_GMP_INT_H
#define GSIM_GMP_INT_H

#include <gmpxx.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <cstdio>
#include <ostream>
#include <string>

namespace gsim {

inline bool shadow_check_enabled() {
  static int enabled = []() -> int {
    const char* env = std::getenv("GSIM_SHADOW");
    if (env && (!std::strcmp(env, "0") || !std::strcmp(env, "false"))) return 0;
    return 1;
  }();
  return enabled != 0;
}

template<int WIDTH, bool SIGNED>
struct BitintHelper;

template<int WIDTH>
struct BitintHelper<WIDTH, false> {
  using type = unsigned _BitInt(WIDTH);
};

template<int WIDTH>
struct BitintHelper<WIDTH, true> {
  using type = _BitInt(WIDTH);
};

template<int WIDTH, bool SIGNED>
class GmpShadow {
 public:
  using shadow_t = typename BitintHelper<WIDTH, SIGNED>::type;

  GmpShadow() : g_(0), shadow_(0) {}

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow(T v) {
    assign_integral(v);
  }

  GmpShadow(const mpz_class& v) { assign_mpz(v); }

  GmpShadow(const GmpShadow&) = default;
  GmpShadow& operator=(const GmpShadow&) = default;
  template<int OTHER_WIDTH, bool OTHER_SIGNED, typename = std::enable_if_t<OTHER_WIDTH != WIDTH || OTHER_SIGNED != SIGNED>>
  GmpShadow(const GmpShadow<OTHER_WIDTH, OTHER_SIGNED>& other) {
    assign_mpz(other.value());
  }
  template<int OTHER_WIDTH, bool OTHER_SIGNED, typename = std::enable_if_t<OTHER_WIDTH != WIDTH || OTHER_SIGNED != SIGNED>>
  GmpShadow& operator=(const GmpShadow<OTHER_WIDTH, OTHER_SIGNED>& other) {
    assign_mpz(other.value());
    return *this;
  }
  template<bool OTHER_SIGNED, typename = std::enable_if_t<OTHER_SIGNED != SIGNED>>
  GmpShadow(const GmpShadow<WIDTH, OTHER_SIGNED>& other) {
    assign_mpz(other.value());
  }
  template<bool OTHER_SIGNED, typename = std::enable_if_t<OTHER_SIGNED != SIGNED>>
  GmpShadow& operator=(const GmpShadow<WIDTH, OTHER_SIGNED>& other) {
    assign_mpz(other.value());
    return *this;
  }

  static constexpr int width() { return WIDTH; }

  // Accessors
  const mpz_class& value() const { return g_; }
  shadow_t shadow() const { return shadow_; }

  explicit operator bool() const { return g_ != 0; }
  operator uint64_t() const { return to_uint64(); }

  // Arithmetic
  GmpShadow operator+(const GmpShadow& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a + b; }, [](shadow_t a, shadow_t b) { return a + b; }, "+"); }
  GmpShadow operator-(const GmpShadow& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a - b; }, [](shadow_t a, shadow_t b) { return a - b; }, "-"); }
  GmpShadow operator*(const GmpShadow& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a * b; }, [](shadow_t a, shadow_t b) { return a * b; }, "*"); }
  GmpShadow operator/(const GmpShadow& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return b == 0 ? mpz_class(0) : a / b; }, [](shadow_t a, shadow_t b) { return b == 0 ? (shadow_t)0 : a / b; }, "/"); }
  GmpShadow operator%(const GmpShadow& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return b == 0 ? mpz_class(0) : a % b; }, [](shadow_t a, shadow_t b) { return b == 0 ? (shadow_t)0 : a % b; }, "%"); }

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator+(T rhs) const { return *this + GmpShadow(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator-(T rhs) const { return *this - GmpShadow(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator*(T rhs) const { return *this * GmpShadow(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator/(T rhs) const { return *this / GmpShadow(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator%(T rhs) const { return *this % GmpShadow(rhs); }

  GmpShadow& operator+=(const GmpShadow& rhs) { *this = *this + rhs; return *this; }
  GmpShadow& operator-=(const GmpShadow& rhs) { *this = *this - rhs; return *this; }
  GmpShadow& operator*=(const GmpShadow& rhs) { *this = *this * rhs; return *this; }
  GmpShadow& operator/=(const GmpShadow& rhs) { *this = *this / rhs; return *this; }
  GmpShadow& operator%=(const GmpShadow& rhs) { *this = *this % rhs; return *this; }

  // Bitwise
  GmpShadow operator&(const GmpShadow& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a & b; }, [](shadow_t a, shadow_t b) { return a & b; }, "&"); }
  GmpShadow operator|(const GmpShadow& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a | b; }, [](shadow_t a, shadow_t b) { return a | b; }, "|"); }
  GmpShadow operator^(const GmpShadow& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a ^ b; }, [](shadow_t a, shadow_t b) { return a ^ b; }, "^"); }

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator&(T rhs) const { return *this & GmpShadow(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator|(T rhs) const { return *this | GmpShadow(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator^(T rhs) const { return *this ^ GmpShadow(rhs); }

  GmpShadow& operator&=(const GmpShadow& rhs) { *this = *this & rhs; return *this; }
  GmpShadow& operator|=(const GmpShadow& rhs) { *this = *this | rhs; return *this; }
  GmpShadow& operator^=(const GmpShadow& rhs) { *this = *this ^ rhs; return *this; }

  GmpShadow operator~() const {
    GmpShadow ret(*this);
    ret.g_ = ~ret.g_;
    ret.shadow_ = ~ret.shadow_;
    ret.normalize();
    ret.check_shadow("~");
    return ret;
  }

  GmpShadow operator-() const {
    GmpShadow ret(*this);
    ret.g_ = -ret.g_;
    ret.shadow_ = -ret.shadow_;
    ret.normalize();
    ret.check_shadow("neg");
    return ret;
  }

  // Shifts
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator<<(T shift) const { return shift_left(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator>>(T shift) const { return shift_right(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow& operator<<=(T shift) { *this = shift_left(static_cast<unsigned>(shift)); return *this; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow& operator>>=(T shift) { *this = shift_right(static_cast<unsigned>(shift)); return *this; }

  // Comparisons (compare primary GMP values after normalization)
  bool operator==(const GmpShadow& rhs) const { check_shadow("cmp=="); rhs.check_shadow("cmp=="); return g_ == rhs.g_; }
  bool operator!=(const GmpShadow& rhs) const { return !(*this == rhs); }
  bool operator<(const GmpShadow& rhs) const { check_shadow("cmp<"); rhs.check_shadow("cmp<"); return g_ < rhs.g_; }
  bool operator>(const GmpShadow& rhs) const { return rhs < *this; }
  bool operator<=(const GmpShadow& rhs) const { return !(*this > rhs); }
  bool operator>=(const GmpShadow& rhs) const { return !(*this < rhs); }

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  bool operator==(T rhs) const { return *this == GmpShadow(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  bool operator!=(T rhs) const { return !(*this == rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  bool operator<(T rhs) const { return *this < GmpShadow(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  bool operator>(T rhs) const { return *this > GmpShadow(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  bool operator<=(T rhs) const { return *this <= GmpShadow(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  bool operator>=(T rhs) const { return *this >= GmpShadow(rhs); }

  bool operator!() const { return g_ == 0; }

  uint64_t to_uint64() const {
    mpz_class norm = canonical(g_);
    return norm.get_ui();
  }

  std::string to_hex() const {
    mpz_class norm = canonical(g_);
    return norm.get_str(16);
  }

  friend std::ostream& operator<<(std::ostream& os, const GmpShadow& v) {
    return os << v.to_hex();
  }

 private:
  mpz_class g_;
  shadow_t shadow_;

  static mpz_class mask() {
    static mpz_class m = []() {
      mpz_class tmp(1);
      mpz_mul_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), WIDTH);
      tmp -= 1;
      return tmp;
    }();
    return m;
  }

  static mpz_class two_pow_width() {
    static mpz_class v = []() {
      mpz_class tmp(1);
      mpz_mul_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), WIDTH);
      return tmp;
    }();
    return v;
  }

  static mpz_class canonical(const mpz_class& in) {
    if constexpr (SIGNED) {
      mpz_class v = in & mask();
      if (WIDTH > 0 && mpz_tstbit(v.get_mpz_t(), WIDTH - 1)) {
        v -= two_pow_width();
      }
      return v;
    } else {
      return in & mask();
    }
  }

  static shadow_t mpz_to_shadow(const mpz_class& in) {
    mpz_class v = in;
    if constexpr (SIGNED) {
      if (v < 0) v += two_pow_width();
    }
    shadow_t ret = 0;
    for (int i = 0; i < WIDTH; i += 64) {
      mpz_class chunk = (v >> i) & ((mpz_class(1) << 64) - 1);
      uint64_t c64 = chunk.get_ui();
      ret |= (shadow_t)c64 << i;
    }
    return ret;
  }

  static mpz_class bitint_to_mpz(shadow_t v) {
    mpz_class res(0);
    for (int i = 0; i < WIDTH; i += 64) {
      uint64_t chunk = (uint64_t)((shadow_t)(v >> i));
      if (chunk) {
        res += (mpz_class(chunk) << i);
      }
    }
    if constexpr (SIGNED) {
      if (v < 0) res -= two_pow_width();
    }
    return canonical(res);
  }

  void normalize() { g_ = canonical(g_); }

  void check_shadow(const char* op) const {
    if (!shadow_check_enabled()) return;
    mpz_class shadow_mpz = canonical(bitint_to_mpz(shadow_));
    mpz_class primary = canonical(g_);
    if (shadow_mpz != primary) report_mismatch(op, primary, shadow_mpz);
  }

  [[noreturn]] static void report_mismatch(const char* op, const mpz_class& primary, const mpz_class& shadow) {
    std::fprintf(stderr, "[GMP shadow mismatch] op=%s width=%d primary=0x%s shadow=0x%s\n",
                 op, WIDTH, primary.get_str(16).c_str(), shadow.get_str(16).c_str());
    std::abort();
  }

  template<typename OpG, typename OpShadow>
  GmpShadow binary_op(const GmpShadow& rhs, OpG opg, OpShadow ops, const char* opname) const {
    GmpShadow ret;
    ret.g_ = canonical(opg(g_, rhs.g_));
    ret.shadow_ = ops(shadow_, rhs.shadow_);
    ret.check_shadow(opname);
    return ret;
  }

  GmpShadow shift_left(unsigned shift) const {
    GmpShadow ret(*this);
    ret.g_ <<= shift;
    ret.shadow_ = shift >= (unsigned)WIDTH ? 0 : (shadow_ << shift);
    ret.normalize();
    ret.check_shadow("<<");
    return ret;
  }

  GmpShadow shift_right(unsigned shift) const {
    GmpShadow ret(*this);
    if constexpr (SIGNED) {
      ret.g_ >>= shift;
      ret.shadow_ = (shadow_t)(shadow_ >> shift);
    } else {
      ret.g_ >>= shift;
      ret.shadow_ = shift >= (unsigned)WIDTH ? 0 : (shadow_ >> shift);
    }
    ret.normalize();
    ret.check_shadow(">>");
    return ret;
  }

  template<typename T>
  void assign_integral(T v) {
    g_ = canonical(mpz_class(v));
    shadow_ = (shadow_t)v;
    check_shadow("init");
  }

  void assign_mpz(const mpz_class& v) {
    g_ = canonical(v);
    shadow_ = mpz_to_shadow(g_);
    check_shadow("init");
  }
};

template<int WIDTH>
using GmpShadowU = GmpShadow<WIDTH, false>;
template<int WIDTH>
using GmpShadowS = GmpShadow<WIDTH, true>;

} // namespace gsim

using gsim::GmpShadowU;
using gsim::GmpShadowS;

#endif // GSIM_GMP_INT_H
