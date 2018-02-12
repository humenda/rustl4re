/*
 * (c) 2014 Alexander Warg <alexander.warg@kernkonzept.com>
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/types.h>
#include <l4/cxx/type_traits>

namespace Hw {


/* EXAMPLE usage:

\code

void test()
{
  // create a register block reference for max. 16bit accesses, using a
  // MMIO register block implementation (at address 0x1000).
  Hw::Register_block<16> regs = new Hw::Mmio_register_block<16>(0x1000);

  // Alternatively it is allowed to use an implementation that allows
  // wider access than actually needed.
  Hw::Register_block<16> regs = new Hw::Mmio_register_block<32>(0x1000);

  // read a 16bit register at offset 8byte
  unsigned short x = regs.r<16>(8);
  unsigned short x1 = regs[8];      // alternative

  // read an 8bit register at offset 0byte
  unsigned v = regs.r<8>(0);

  // do a 16bit write to register at offset 2byte (four variants)
  regs[2] = 22;
  regs.r<16>(2) = 22;
  regs[2].write(22);
  regs.r<16>().write(22);

  // do an 8bit write (two variants)
  regs.r<8>(0) = 9;
  regs.r<8>(0).write(9);

  // do 16bit read-modify-write (two variants)
  regs[4].modify(0xf, 3); // clear 4 lowest bits and set them to 3
  regs.r<16>(4).modify(0xf, 3);

  // do 8bit read-modify-write
  regs.r<8>(0).modify(0xf, 3);

  // fails to compile, because of too wide access
  // (32 bit access but regs is Hw::Register_block<16>)
  unsigned long v = regs.r<32>(4)
}

\endcode
*/


/**
 * \brief Abstract register block interface
 * \tparam MAX_BITS The maximum access width for the registers.
 *
 * This interfaces is based on virtual do_read_<xx> and do_write_<xx>
 * methods that have to be implemented up to the maximum access width.
 */
template< unsigned MAX_BITS = 32 >
struct Register_block_base;

template<>
struct Register_block_base<8>
{
  virtual l4_uint8_t do_read_8(l4_addr_t reg) const = 0;
  virtual void do_write_8(l4_uint8_t value, l4_addr_t reg) = 0;
  virtual ~Register_block_base() = 0;
};

inline Register_block_base<8>::~Register_block_base() {}

template<>
struct Register_block_base<16> : Register_block_base<8>
{
  virtual l4_uint16_t do_read_16(l4_addr_t reg) const = 0;
  virtual void do_write_16(l4_uint16_t value, l4_addr_t reg) = 0;
};

template<>
struct Register_block_base<32> : Register_block_base<16>
{
  virtual l4_uint32_t do_read_32(l4_addr_t reg) const = 0;
  virtual void do_write_32(l4_uint32_t value, l4_addr_t reg) = 0;
};

template<>
struct Register_block_base<64> : Register_block_base<32>
{
  virtual l4_uint64_t do_read_64(l4_addr_t reg) const = 0;
  virtual void do_write_64(l4_uint64_t value, l4_addr_t reg) = 0;
};
#undef REGBLK_READ_TEMPLATE
#undef REGBLK_WRITE_TEMPLATE

template<typename CHILD>
struct Register_block_modify_mixin
{
  template< typename T >
  T modify(T clear_bits, T set_bits, l4_addr_t reg) const
  {
    CHILD const *c = static_cast<CHILD const *>(this);
    T r = (c->template read<T>(reg) & ~clear_bits) | set_bits;
    c->template write<T>(r, reg);
    return r;
  }

  template< typename T >
  T set(T set_bits, l4_addr_t reg) const
  { return this->template modify<T>(T(0), set_bits, reg); }

  template< typename T >
  T clear(T clear_bits, l4_addr_t reg) const
  { return this->template modify<T>(clear_bits, T(0), reg); }
};


#define REGBLK_READ_TEMPLATE(sz) \
  template< typename T >          \
  typename cxx::enable_if<sizeof(T) == (sz / 8), T>::type read(l4_addr_t reg) const \
  { \
    union X { T t; l4_uint##sz##_t v; } m; \
    m.v = _b->do_read_##sz (reg); \
    return m.t; \
  }

#define REGBLK_WRITE_TEMPLATE(sz) \
  template< typename T >          \
  void write(T value, l4_addr_t reg, typename cxx::enable_if<sizeof(T) == (sz / 8), T>::type = T()) const \
  { \
    union X { T t; l4_uint##sz##_t v; } m; \
    m.t = value; \
    _b->do_write_##sz(m.v, reg); \
  }

/**
 * \brief Helper template that translates to the Register_block_base
 *        interface.
 * \tparam BLOCK The type of the Register_block_base interface to use.
 *
 * This helper translates read<T>(), write<T>(), set<T>(), clear<T>(),
 * and modify<T>() calls to BLOCK::do_read_<xx> and BLOCK::do_write_<xx>.
 */
template< typename BLOCK >
class Register_block_tmpl
: public Register_block_modify_mixin<Register_block_tmpl<BLOCK> >
{
private:
  BLOCK *_b;

public:
  Register_block_tmpl(BLOCK *blk) : _b(blk) {}
  Register_block_tmpl() = default;

  operator BLOCK * () const { return _b; }

  REGBLK_READ_TEMPLATE(8)
  REGBLK_WRITE_TEMPLATE(8)
  REGBLK_READ_TEMPLATE(16)
  REGBLK_WRITE_TEMPLATE(16)
  REGBLK_READ_TEMPLATE(32)
  REGBLK_WRITE_TEMPLATE(32)
  REGBLK_READ_TEMPLATE(64)
  REGBLK_WRITE_TEMPLATE(64)
};


#undef REGBLK_READ_TEMPLATE
#undef REGBLK_WRITE_TEMPLATE

namespace __Type_helper {
  template<unsigned> struct Unsigned;
  template<> struct Unsigned<8> { typedef l4_uint8_t type; };
  template<> struct Unsigned<16> { typedef l4_uint16_t type; };
  template<> struct Unsigned<32> { typedef l4_uint32_t type; };
  template<> struct Unsigned<64> { typedef l4_uint64_t type; };
};


/**
 * \brief Single read only register inside a Register_block_base interface.
 * \tparam BITS The access with of the register in bits.
 * \tparam BLOCK The type for the Register_block_base interface.
 * \note Objects of this type must be used only in temporary contexts
 *       not in global, class, or object scope.
 *
 * Allows simple read only access to a hardware register.
 */
template< unsigned BITS, typename BLOCK >
class Ro_register_tmpl
{
protected:
  BLOCK _b;
  unsigned _o;

public:
  typedef typename __Type_helper::Unsigned<BITS>::type value_type;

  Ro_register_tmpl(BLOCK const &blk, unsigned offset) : _b(blk), _o(offset) {}
  Ro_register_tmpl() = default;

  /**
   * \brief read the value from the hardware register.
   * \return value read from the hardware register.
   */
  operator value_type () const
  { return _b.template read<value_type>(_o); }

  /**
   * \brief read the value from the hardware register.
   * \return value from the hardware register.
   */
  value_type read() const
  { return _b.template read<value_type>(_o); }
};


/**
 * \brief Single hardware register inside a Register_block_base interface.
 * \tparam BITS The access width for the register in bits.
 * \tparam BLOCK the type of the Register_block_base interface.
 * \note Objects of this type msut be used only in temporary contexts
 *       not in global, class, or object scope.
 */
template< unsigned BITS, typename BLOCK >
class Register_tmpl : public Ro_register_tmpl<BITS, BLOCK>
{
public:
  typedef typename Ro_register_tmpl<BITS, BLOCK>::value_type value_type;

  Register_tmpl(BLOCK const &blk, unsigned offset)
  : Ro_register_tmpl<BITS, BLOCK>(blk, offset)
  {}

  Register_tmpl() = default;

  /**
   * \brief write \a val into the hardware register.
   * \param val the value to write into the hardware register.
   */
  Register_tmpl &operator = (value_type val)
  { this->_b.template write<value_type>(val, this->_o); return *this; }

  /**
   * \brief write \a val into the hardware register.
   * \param val the value to write into the hardware register.
   */
  void write(value_type val)
  { this->_b.template write<value_type>(val, this->_o); }

  /**
   * \brief set bits in \a set_bits in the hardware register.
   * \param set_bits bits to be set within the hardware register.
   *
   * This is a read-modify-write function that does a logical or
   * of the old value from the register with \a set_bits.
   *
   * \code
   * unsigned old_value = read();
   * write(old_value | set_bits);
   * \endcode
   */
  value_type set(value_type set_bits)
  { return this->_b.template set<value_type>(set_bits, this->_o); }

  /**
   * \brief clears bits in \a clear_bits in the hardware register.
   * \param clear_bits bits to be cleared within the hardware register.
   *
   * This is a read-modify-write function that does a logical and
   * of the old value from the register with the negated value of
   * \a clear_bits.
   *
   * \code
   * unsigned old_value = read();
   * write(old_value & ~clear_bits);
   * \endcode
   */
  value_type clear(value_type clear_bits)
  { return this->_b.template clear<value_type>(clear_bits, this->_o); }

  /**
   * \brief clears bits in \a clear_bits and sets bits in \a set_bits
   *        in the hardware register.
   * \param clear_bits bits to be cleared within the hardware register.
   * \param set_bits bits to set in the hardware register.
   *
   * This is a read-modify-write function that first does a logical and
   * of the old value from the register with the negated value of
   * \a clear_bits and then does a logical or with \a set_bits.
   *
   * \code{.c}
   * unsigned old_value = read();
   * write((old_value & ~clear_bits) | set_bits);
   * \endcode
   */
  value_type modify(value_type clear_bits, value_type set_bits)
  { return this->_b.template modify<value_type>(clear_bits, set_bits, this->_o); }
};


/**
 * \brief Handles a reference to a register block of the given
 *        maximum access width.
 * \tparam MAX_BITS Maximum access width for the registers in this
 *         block.
 * \tparam BLOCK Type implementing the register accesses (read<>(), write<>(),
 *                modify<>(), set<>(), and clear<>()).
 *
 * Provides access to registers in this block via r<WIDTH>() and
 * operator[]().
 */
template<
  unsigned MAX_BITS,
  typename BLOCK = Register_block_tmpl<
                     Register_block_base<MAX_BITS>
                   >
>
class Register_block
{
private:
  template< unsigned B, typename BLK > friend class Register_block;
  template< unsigned B, typename BLK > friend class Ro_register_block;
  typedef  BLOCK Block;
  Block _b;

public:
  Register_block() = default;
  Register_block(Block const &blk) : _b(blk) {}
  Register_block &operator = (Block const &blk)
  { _b = blk; return *this; }

  template< unsigned BITS >
  Register_block(Register_block<BITS> blk) : _b(blk._b) {}

  typedef Register_tmpl<MAX_BITS, Block> Register;
  typedef Ro_register_tmpl<MAX_BITS, Block> Ro_register;

  /**
   * \brief Read only access to register at offset \a offset.
   * \tparam BITS the access width in bits for the register.
   * \param offset The offset of the register within the register file.
   * \return register object allowing read only access with width \a BITS.
   */
  template< unsigned BITS >
  Ro_register_tmpl<BITS, Block> r(unsigned offset) const
  { return Ro_register_tmpl<BITS, Block>(this->_b, offset); }

  /**
   * \brief Read only access to register at offset \a offset.
   * \param offset The offset of the register within the register file.
   * \return register object allowing read only access with width \a MAX_BITS.
   */
  Ro_register operator [] (unsigned offset) const
  { return this->r<MAX_BITS>(offset); }


  /**
   * \brief Read/write access to register at offset \a offset.
   * \tparam BITS the access width in bits for the register.
   * \param offset The offset of the register within the register file.
   * \return register object allowing read and write access with width \a BITS.
   */
  template< unsigned BITS >
  Register_tmpl<BITS, Block> r(unsigned offset)
  { return Register_tmpl<BITS, Block>(this->_b, offset); }

  /**
   * \brief Read/write access to register at offset \a offset.
   * \param offset The offset of the register within the register file.
   * \return register object allowing read and write access with
   *         width \a MAX_BITS.
   */
  Register operator [] (unsigned offset)
  { return this->r<MAX_BITS>(offset); }
};

/**
 * \brief Handles a reference to a read only register block of the given
 *        maximum access width.
 * \tparam MAX_BITS Maximum access width for the registers in this block.
 * \tparam BLOCK Type implementing the register accesses (read<>()),
 *
 * Provides read only access to registers in this block via r<WIDTH>()
 * and operator[]().
 */
template<
  unsigned MAX_BITS,
  typename BLOCK = Register_block_tmpl<
                     Register_block_base<MAX_BITS> const
                   >
>
class Ro_register_block
{
private:
  template< unsigned B, typename BLK > friend class Ro_register_block;
  typedef  BLOCK Block;
  Block _b;

public:
  Ro_register_block() = default;
  Ro_register_block(BLOCK const &blk) : _b(blk) {}

  template< unsigned BITS >
  Ro_register_block(Register_block<BITS> const &blk) : _b(blk._b) {}

  typedef Ro_register_tmpl<MAX_BITS, Block> Ro_register;
  typedef Ro_register Register;

  /**
   * \brief Read only access to register at offset \a offset.
   * \param offset The offset of the register within the register file.
   * \return register object allowing read only access with width \a MAX_BITS.
   */
  Ro_register operator [] (unsigned offset) const
  { return Ro_register(this->_b, offset); }

  /**
   * \brief Read only access to register at offset \a offset.
   * \tparam BITS the access width in bits for the register.
   * \param offset The offset of the register within the register file.
   * \return register object allowing read only access with width \a BITS.
   */
  template< unsigned BITS >
  Ro_register_tmpl<BITS, Block> r(unsigned offset) const
  { return Ro_register_tmpl<BITS, Block>(this->_b, offset); }
};


/**
 * \brief Implementation helper for register blocks.
 * \param BASE The class implementing read<> and write<> template functions
 *             for accessing the registers. This class must inherit from
 *             Register_block_impl.
 * \param MAX_BITS The maximum access width for the register file.
 *                 Supported values are 8, 16, 32, or 64.
 *
 *
 * This template allows easy implementation of register files by providing
 * read<> and write<> template functions, see Mmio_register_block
 * as an example.
 */
template< typename BASE, unsigned MAX_BITS = 32 >
struct Register_block_impl;

#define REGBLK_IMPL_RW_TEMPLATE(sz, ...) \
  l4_uint##sz##_t do_read_##sz(l4_addr_t reg) const \
  { return static_cast<BASE const *>(this)->template read<l4_uint##sz##_t>(reg); } \
  \
  void do_write_##sz(l4_uint##sz##_t value, l4_addr_t reg) \
  { static_cast<BASE*>(this)->template write<l4_uint##sz##_t>(value, reg); }


template< typename BASE >
struct Register_block_impl<BASE, 8> : public Register_block_base<8>
{
  REGBLK_IMPL_RW_TEMPLATE(8);
};

template< typename BASE >
struct Register_block_impl<BASE, 16> : public Register_block_base<16>
{
  REGBLK_IMPL_RW_TEMPLATE(8);
  REGBLK_IMPL_RW_TEMPLATE(16);
};

template< typename BASE >
struct Register_block_impl<BASE, 32> : public Register_block_base<32>
{
  REGBLK_IMPL_RW_TEMPLATE(8);
  REGBLK_IMPL_RW_TEMPLATE(16);
  REGBLK_IMPL_RW_TEMPLATE(32);
};

template< typename BASE >
struct Register_block_impl<BASE, 64> : public Register_block_base<64>
{
  REGBLK_IMPL_RW_TEMPLATE(8);
  REGBLK_IMPL_RW_TEMPLATE(16);
  REGBLK_IMPL_RW_TEMPLATE(32);
  REGBLK_IMPL_RW_TEMPLATE(64);
};

#undef REGBLK_IMPL_RW_TEMPLATE

/** \brief Dummy register block to be used as a placeholder */
extern Register_block<64> dummy_register_block;

}
