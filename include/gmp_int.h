#ifndef GSIM_GMP_INT_H
#define GSIM_GMP_INT_H

#include <gmpxx.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <ostream>
#include <string>

namespace gsim {

// Enable shadow checks only when ENABLE_GMP_SHADOW is defined and set to 1.
inline bool shadow_check_enabled() {
#ifdef ENABLE_GMP_SHADOW
  static int enabled = []() -> int {
    const char* env = std::getenv("GSIM_SHADOW");
    if (env && (!std::strcmp(env, "0") || !std::strcmp(env, "false"))) return 0;
    return 1;
  }();
  return enabled != 0;
#else
  return false;
#endif
}

template<int WIDTH, bool SIGNED>
class GmpInt {
 public:
  GmpInt() : g_(0) {}

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt(T v) { assign_integral(v); }

  explicit GmpInt(const mpz_class& v) { assign_mpz(v); }

  GmpInt(const GmpInt&) = default;
  GmpInt& operator=(const GmpInt&) = default;

  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<OTHER_W != WIDTH || OTHER_S != SIGNED>>
  GmpInt(const GmpInt<OTHER_W, OTHER_S>& other) { assign_mpz(other.value()); }
  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<OTHER_W != WIDTH || OTHER_S != SIGNED>>
  GmpInt& operator=(const GmpInt<OTHER_W, OTHER_S>& other) { assign_mpz(other.value()); return *this; }

  const mpz_class& value() const { return g_; }

  // Narrowing conversion for assignments to native integers.
  operator uint64_t() const { return canonical(g_).get_ui(); }

  // Arithmetic
  GmpInt operator+(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a + b; }); }
  GmpInt operator-(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a - b; }); }
  GmpInt operator*(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a * b; }); }
  GmpInt operator/(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return b == 0 ? mpz_class(0) : a / b; }); }
  GmpInt operator%(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return b == 0 ? mpz_class(0) : a % b; }); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator+(T rhs) const { return *this + GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator-(T rhs) const { return *this - GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator*(T rhs) const { return *this * GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator/(T rhs) const { return *this / GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator%(T rhs) const { return *this % GmpInt(rhs); }

  GmpInt& operator+=(const GmpInt& rhs) { *this = *this + rhs; return *this; }
  GmpInt& operator-=(const GmpInt& rhs) { *this = *this - rhs; return *this; }
  GmpInt& operator*=(const GmpInt& rhs) { *this = *this * rhs; return *this; }
  GmpInt& operator/=(const GmpInt& rhs) { *this = *this / rhs; return *this; }
  GmpInt& operator%=(const GmpInt& rhs) { *this = *this % rhs; return *this; }

  // Bitwise
  GmpInt operator&(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a & b; }); }
  GmpInt operator|(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a | b; }); }
  GmpInt operator^(const GmpInt& rhs) const { return binary_op(rhs, [](const mpz_class& a, const mpz_class& b) { return a ^ b; }); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator&(T rhs) const { return *this & GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator|(T rhs) const { return *this | GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator^(T rhs) const { return *this ^ GmpInt(rhs); }
  GmpInt& operator&=(const GmpInt& rhs) { *this = *this & rhs; return *this; }
  GmpInt& operator|=(const GmpInt& rhs) { *this = *this | rhs; return *this; }
  GmpInt& operator^=(const GmpInt& rhs) { *this = *this ^ rhs; return *this; }

  GmpInt operator~() const { GmpInt ret(*this); ret.g_ = ~ret.g_; ret.normalize(); return ret; }
  GmpInt operator-() const { GmpInt ret(*this); ret.g_ = -ret.g_; ret.normalize(); return ret; }

  // Shifts
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator<<(T shift) const { return shift_left(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator>>(T shift) const { return shift_right(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt& operator<<=(T shift) { *this = shift_left(static_cast<unsigned>(shift)); return *this; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt& operator>>=(T shift) { *this = shift_right(static_cast<unsigned>(shift)); return *this; }

  // Comparisons
  bool operator==(const GmpInt& rhs) const { return canonical(g_) == canonical(rhs.g_); }
  bool operator!=(const GmpInt& rhs) const { return !(*this == rhs); }
  bool operator<(const GmpInt& rhs) const { return canonical(g_) < canonical(rhs.g_); }
  bool operator>(const GmpInt& rhs) const { return rhs < *this; }
  bool operator<=(const GmpInt& rhs) const { return !(*this > rhs); }
  bool operator>=(const GmpInt& rhs) const { return !(*this < rhs); }

  friend std::ostream& operator<<(std::ostream& os, const GmpInt& v) {
    return os << v.canonical(v.g_).get_str(16);
  }

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator+(T lhs, const GmpInt& rhs) { return GmpInt(lhs) + rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator-(T lhs, const GmpInt& rhs) { return GmpInt(lhs) - rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator*(T lhs, const GmpInt& rhs) { return GmpInt(lhs) * rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator/(T lhs, const GmpInt& rhs) { return GmpInt(lhs) / rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator%(T lhs, const GmpInt& rhs) { return GmpInt(lhs) % rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator&(T lhs, const GmpInt& rhs) { return GmpInt(lhs) & rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator|(T lhs, const GmpInt& rhs) { return GmpInt(lhs) | rhs; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  friend GmpInt operator^(T lhs, const GmpInt& rhs) { return GmpInt(lhs) ^ rhs; }

 protected:
  mpz_class g_;

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
      if (WIDTH > 0 && mpz_tstbit(v.get_mpz_t(), WIDTH - 1)) v -= two_pow_width();
      return v;
    } else {
      return in & mask();
    }
  }

  void normalize() { g_ = canonical(g_); }

  template<typename Op>
  GmpInt binary_op(const GmpInt& rhs, Op op) const {
    GmpInt ret;
    ret.g_ = canonical(op(g_, rhs.g_));
    return ret;
  }

  GmpInt shift_left(unsigned shift) const {
    GmpInt ret(*this);
    mpz_mul_2exp(ret.g_.get_mpz_t(), ret.g_.get_mpz_t(), shift);
    ret.normalize();
    return ret;
  }

  GmpInt shift_right(unsigned shift) const {
    GmpInt ret(*this);
    if constexpr (SIGNED) {
      mpz_fdiv_q_2exp(ret.g_.get_mpz_t(), ret.g_.get_mpz_t(), shift);
    } else {
      mpz_tdiv_q_2exp(ret.g_.get_mpz_t(), ret.g_.get_mpz_t(), shift);
    }
    ret.normalize();
    return ret;
  }

  template<typename T>
  void assign_integral(T v) {
    if constexpr (std::is_signed<T>::value) mpz_set_si(g_.get_mpz_t(), static_cast<long>(v));
    else mpz_set_ui(g_.get_mpz_t(), static_cast<unsigned long>(v));
    normalize();
  }

  void assign_mpz(const mpz_class& v) {
    g_ = canonical(v);
  }
};

template<int WIDTH> using GmpIntU = GmpInt<WIDTH, false>;
template<int WIDTH> using GmpIntS = GmpInt<WIDTH, true>;

#ifdef ENABLE_GMP_SHADOW
// Shadow type that pairs GMP primary with _BitInt shadow.
template<int WIDTH, bool SIGNED>
struct BitintHelper;
template<int WIDTH>
struct BitintHelper<WIDTH, false> { using type = unsigned _BitInt(WIDTH); };
template<int WIDTH>
struct BitintHelper<WIDTH, true> { using type = _BitInt(WIDTH); };

template<int WIDTH, bool SIGNED>
class GmpShadow : public GmpInt<WIDTH, SIGNED> {
  using Base = GmpInt<WIDTH, SIGNED>;
 public:
  using shadow_t = typename BitintHelper<WIDTH, SIGNED>::type;
  GmpShadow() : Base(), shadow_(0) {}
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow(T v) : Base(v) { shadow_ = static_cast<shadow_t>(v); check_shadow("init"); }
  explicit GmpShadow(const mpz_class& v) : Base(v) { shadow_ = mpz_to_shadow(Base::g_); check_shadow("init"); }
  GmpShadow(const GmpShadow&) = default;
  GmpShadow& operator=(const GmpShadow&) = default;
  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<OTHER_W != WIDTH || OTHER_S != SIGNED>>
  GmpShadow(const GmpShadow<OTHER_W, OTHER_S>& other) : Base(other.value()) { shadow_ = mpz_to_shadow(Base::g_); check_shadow("init"); }
  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<OTHER_W != WIDTH || OTHER_S != SIGNED>>
  GmpShadow& operator=(const GmpShadow<OTHER_W, OTHER_S>& other) { Base::assign_mpz(other.value()); shadow_ = mpz_to_shadow(Base::g_); check_shadow("init"); return *this; }

  shadow_t shadow() const { return shadow_; }

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

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator<<(T shift) const { return shift_left(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow operator>>(T shift) const { return shift_right(static_cast<unsigned>(shift)); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow& operator<<=(T shift) { *this = shift_left(static_cast<unsigned>(shift)); return *this; }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpShadow& operator>>=(T shift) { *this = shift_right(static_cast<unsigned>(shift)); return *this; }

 private:
  shadow_t shadow_;

  static shadow_t mpz_to_shadow(const mpz_class& in) {
    mpz_class v = Base::canonical(in);
    shadow_t ret = 0;
    for (int i = 0; i < WIDTH; i += 64) {
      mpz_class chunk = (v >> i) & ((mpz_class(1) << 64) - 1);
      ret |= (shadow_t)chunk.get_ui() << i;
    }
    return ret;
  }

  void check_shadow(const char* op) const {
    if (!shadow_check_enabled()) return;
    mpz_class shadow_mpz = Base::canonical(mpz_class(mpz_to_shadow(Base::g_)));
    mpz_class primary = Base::canonical(Base::g_);
    if (shadow_mpz != primary) {
      std::fprintf(stderr, "[GMP shadow mismatch] op=%s width=%d primary=0x%s shadow=0x%s\n",
                   op, WIDTH, primary.get_str(16).c_str(), shadow_mpz.get_str(16).c_str());
      std::abort();
    }
  }

  template<typename OpG, typename OpShadow>
  GmpShadow binary_op(const GmpShadow& rhs, OpG opg, OpShadow ops, const char* opname) const {
    GmpShadow ret;
    ret.g_ = Base::canonical(opg(this->g_, rhs.g_));
    ret.shadow_ = ops(shadow_, rhs.shadow_);
    ret.check_shadow(opname);
    return ret;
  }

  GmpShadow shift_left(unsigned shift) const {
    GmpShadow ret(*this);
    mpz_mul_2exp(ret.g_.get_mpz_t(), ret.g_.get_mpz_t(), shift);
    ret.shadow_ = shift >= (unsigned)WIDTH ? 0 : (shadow_ << shift);
    ret.normalize();
    ret.check_shadow("<<");
    return ret;
  }

  GmpShadow shift_right(unsigned shift) const {
    GmpShadow ret(*this);
    if constexpr (SIGNED) {
      mpz_fdiv_q_2exp(ret.g_.get_mpz_t(), ret.g_.get_mpz_t(), shift);
      ret.shadow_ = (shadow_t)(shadow_ >> shift);
    } else {
      mpz_tdiv_q_2exp(ret.g_.get_mpz_t(), ret.g_.get_mpz_t(), shift);
      ret.shadow_ = shift >= (unsigned)WIDTH ? 0 : (shadow_ >> shift);
    }
    ret.normalize();
    ret.check_shadow(">>");
    return ret;
  }
};

template<int WIDTH> using GmpShadowU = GmpShadow<WIDTH, false>;
template<int WIDTH> using GmpShadowS = GmpShadow<WIDTH, true>;
#endif // ENABLE_GMP_SHADOW

#ifdef ENABLE_GMP_SHADOW
template<int WIDTH> using GmpWideU = GmpShadowU<WIDTH>;
template<int WIDTH> using GmpWideS = GmpShadowS<WIDTH>;
#else
template<int WIDTH> using GmpWideU = GmpIntU<WIDTH>;
template<int WIDTH> using GmpWideS = GmpIntS<WIDTH>;
#endif

} // namespace gsim

using gsim::GmpIntU;
using gsim::GmpIntS;
using gsim::GmpWideU;
using gsim::GmpWideS;
#ifdef ENABLE_GMP_SHADOW
using gsim::GmpShadowU;
using gsim::GmpShadowS;
#endif

#endif // GSIM_GMP_INT_H
