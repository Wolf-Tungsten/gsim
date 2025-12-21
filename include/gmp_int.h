#ifndef GSIM_GMP_INT_H
#define GSIM_GMP_INT_H

#include <gmpxx.h>
#include <cstdint>
#include <type_traits>
#include <ostream>

namespace gsim {

template<int WIDTH, bool SIGNED>
class GmpInt {
 public:
  static_assert(WIDTH > 0, "WIDTH must be positive");
  static constexpr int width = WIDTH;
  static constexpr bool is_signed = SIGNED;

  GmpInt() { init_zero(); }

  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt(T v) { init_zero(); assign_integral(v); }

  explicit GmpInt(const mpz_class& v) { init_zero(); assign_mpz(v); }

  GmpInt(const GmpInt& other) { init_zero(); mpz_set(g_, other.g_); }
  GmpInt& operator=(const GmpInt& other) {
    if (this != &other) mpz_set(g_, other.g_);
    return *this;
  }

  GmpInt(GmpInt&& other) noexcept { init_zero(); mpz_swap(g_, other.g_); }
  GmpInt& operator=(GmpInt&& other) noexcept {
    if (this != &other) mpz_swap(g_, other.g_);
    return *this;
  }

  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<OTHER_W != WIDTH || OTHER_S != SIGNED>>
  GmpInt(const GmpInt<OTHER_W, OTHER_S>& other) { init_zero(); assign_mpz(other.value()); }
  template<int OTHER_W, bool OTHER_S, typename = std::enable_if_t<OTHER_W != WIDTH || OTHER_S != SIGNED>>
  GmpInt& operator=(const GmpInt<OTHER_W, OTHER_S>& other) { assign_mpz(other.value()); return *this; }

  ~GmpInt() { mpz_clear(g_); }

  mpz_class value() const { return canonical_mpz(); }

  operator uint64_t() const { return canonical_mpz().get_ui(); }

  // Arithmetic
  GmpInt operator+(const GmpInt& rhs) const { return binary_op(rhs, __gmpz_add); }
  GmpInt operator-(const GmpInt& rhs) const { return binary_op(rhs, __gmpz_sub); }
  GmpInt operator*(const GmpInt& rhs) const { return binary_op(rhs, __gmpz_mul); }
  GmpInt operator/(const GmpInt& rhs) const {
    GmpInt ret;
    if (mpz_sgn(rhs.g_) == 0) mpz_set_ui(ret.g_, 0);
    else { mpz_fdiv_q(ret.g_, g_, rhs.g_); ret.normalize(); }
    return ret;
  }
  GmpInt operator%(const GmpInt& rhs) const {
    GmpInt ret;
    if (mpz_sgn(rhs.g_) == 0) mpz_set_ui(ret.g_, 0);
    else { mpz_fdiv_r(ret.g_, g_, rhs.g_); ret.normalize(); }
    return ret;
  }
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

  GmpInt& operator+=(const GmpInt& rhs) { __gmpz_add(g_, g_, rhs.g_); normalize(); return *this; }
  GmpInt& operator-=(const GmpInt& rhs) { __gmpz_sub(g_, g_, rhs.g_); normalize(); return *this; }
  GmpInt& operator*=(const GmpInt& rhs) { __gmpz_mul(g_, g_, rhs.g_); normalize(); return *this; }
  GmpInt& operator/=(const GmpInt& rhs) {
    if (mpz_sgn(rhs.g_) == 0) mpz_set_ui(g_, 0);
    else { mpz_fdiv_q(g_, g_, rhs.g_); normalize(); }
    return *this;
  }
  GmpInt& operator%=(const GmpInt& rhs) {
    if (mpz_sgn(rhs.g_) == 0) mpz_set_ui(g_, 0);
    else { mpz_fdiv_r(g_, g_, rhs.g_); normalize(); }
    return *this;
  }

  // Bitwise
  GmpInt operator&(const GmpInt& rhs) const { return binary_op(rhs, __gmpz_and); }
  GmpInt operator|(const GmpInt& rhs) const { return binary_op(rhs, __gmpz_ior); }
  GmpInt operator^(const GmpInt& rhs) const { return binary_op(rhs, __gmpz_xor); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator&(T rhs) const { return *this & GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator|(T rhs) const { return *this | GmpInt(rhs); }
  template<typename T, typename = std::enable_if_t<std::is_integral<T>::value>>
  GmpInt operator^(T rhs) const { return *this ^ GmpInt(rhs); }
  GmpInt& operator&=(const GmpInt& rhs) { __gmpz_and(g_, g_, rhs.g_); normalize(); return *this; }
  GmpInt& operator|=(const GmpInt& rhs) { __gmpz_ior(g_, g_, rhs.g_); normalize(); return *this; }
  GmpInt& operator^=(const GmpInt& rhs) { __gmpz_xor(g_, g_, rhs.g_); normalize(); return *this; }

  GmpInt operator~() const { GmpInt ret; mpz_com(ret.g_, g_); ret.normalize(); return ret; }
  GmpInt operator-() const { GmpInt ret; mpz_neg(ret.g_, g_); ret.normalize(); return ret; }

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
  bool operator==(const GmpInt& rhs) const { return canonical_mpz() == rhs.canonical_mpz(); }
  bool operator!=(const GmpInt& rhs) const { return !(*this == rhs); }
  bool operator<(const GmpInt& rhs) const { return canonical_mpz() < rhs.canonical_mpz(); }
  bool operator>(const GmpInt& rhs) const { return rhs < *this; }
  bool operator<=(const GmpInt& rhs) const { return !(*this > rhs); }
  bool operator>=(const GmpInt& rhs) const { return !(*this < rhs); }

  friend std::ostream& operator<<(std::ostream& os, const GmpInt& v) {
    return os << v.canonical_mpz().get_str(16);
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
  mpz_t g_;

  static const mpz_class& mask() {
    static mpz_class m = []() {
      mpz_class tmp(1);
      mpz_mul_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), WIDTH);
      tmp -= 1;
      return tmp;
    }();
    return m;
  }

  static const mpz_class& two_pow_width() {
    static mpz_class v = []() {
      mpz_class tmp(1);
      mpz_mul_2exp(tmp.get_mpz_t(), tmp.get_mpz_t(), WIDTH);
      return tmp;
    }();
    return v;
  }

  mpz_class canonical_mpz() const {
    mpz_class out;
    mpz_and(out.get_mpz_t(), g_, mask().get_mpz_t());
    if constexpr (SIGNED) {
      if (mpz_tstbit(out.get_mpz_t(), WIDTH - 1)) {
        mpz_sub(out.get_mpz_t(), out.get_mpz_t(), two_pow_width().get_mpz_t());
      }
    }
    return out;
  }

  void normalize() {
    mpz_class c = canonical_mpz();
    mpz_set(g_, c.get_mpz_t());
  }

  template<typename Op>
  GmpInt binary_op(const GmpInt& rhs, Op op) const {
    GmpInt ret;
    op(ret.g_, g_, rhs.g_);
    ret.normalize();
    return ret;
  }

  GmpInt shift_left(unsigned shift) const {
    GmpInt ret;
    mpz_mul_2exp(ret.g_, g_, shift);
    ret.normalize();
    return ret;
  }

  GmpInt shift_right(unsigned shift) const {
    GmpInt ret;
    if constexpr (SIGNED) {
      mpz_fdiv_q_2exp(ret.g_, g_, shift);
    } else {
      mpz_tdiv_q_2exp(ret.g_, g_, shift);
    }
    ret.normalize();
    return ret;
  }

  template<typename T>
  void assign_integral(T v) {
    if constexpr (std::is_signed<T>::value) mpz_set_si(g_, static_cast<long>(v));
    else mpz_set_ui(g_, static_cast<unsigned long>(v));
    normalize();
  }

  void assign_mpz(const mpz_class& v) {
    mpz_set(g_, v.get_mpz_t());
    normalize();
  }

  void init_zero() {
    mpz_init2(g_, WIDTH);
    mpz_set_ui(g_, 0);
  }
};

template<int WIDTH> using GmpIntU = GmpInt<WIDTH, false>;
template<int WIDTH> using GmpIntS = GmpInt<WIDTH, true>;
template<int WIDTH> using GmpWideU = GmpIntU<WIDTH>;
template<int WIDTH> using GmpWideS = GmpIntS<WIDTH>;

} // namespace gsim

using gsim::GmpIntU;
using gsim::GmpIntS;
using gsim::GmpWideU;
using gsim::GmpWideS;

#endif // GSIM_GMP_INT_H
