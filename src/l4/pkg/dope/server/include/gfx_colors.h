#ifndef _DOPE_GFX_COLORS_H_
#define _DOPE_GFX_COLORS_H_


/*** Encapsulation of properties of a pixel */
/* A pixel occupies the given number of bytes in video memory and
 * has a representation in a some machine useable type (byte, short,
 * unsigned).
 */
template< int _Bytes >
class Pixel_traits;

/*** Spezilization for 2byte */
template<>
class Pixel_traits<2>
{
public:
	enum { Bpp = 2 };
	typedef u16 Color;  /* color value for computations */
	typedef u16 Pixel; /* refernece to video memory representation */
};

/*** Spezialization for 4byte */
template<>
class Pixel_traits<4>
{
public:
	enum { Bpp = 4 };
	typedef u32 Color;
	typedef u32 Pixel;
};


/*** Spezialization for 3byte */
template<>
class Pixel_traits<3>
{
public:
	enum { Bpp = 3 };
	typedef u32 Color; /* use 32bit for arithmetic */

	/*** Encapsulation for 3byte memory references */
	/* Looks like a pointer to a native data type  */
	class Pixel
	{
	private:
		char _b[3];
		u32 get() const 
		{
			return (u32)(*(u16 const *)_b) | ((u32)_b[2] << 16) ;
		}

	public:
		Pixel(u32 c) 
		{
			*(u16*)_b = (u16)c;
			_b[2] = c >> 16;
		}

		Pixel &operator = (u32 c)
		{
			*(u16*)_b = (u16)c;
			_b[2] = c >> 16;
			return *this;
		}

		operator u32 () const { return get(); }
	} __attribute__((packed));
};


/*** Encapsulation of color bit fields of any kind */
/* _Pixel                 must be the Pixel_traits for the mode.
 * _<X>shift and _<X>size is the bit shift and number of bits used for
 *                        each color (and alpha)
 *
 * Local types are:
 *   Color   the register representation of a color.
 *   Pixel   the reference to a pixel in video memory.
 *   R,G,B,A the traits for the single components
 *           (define Shift, Size, and Mask for each color component)
 *   C_space the sorted representation of the color components,
 *           Used to find the uppermost, middle, and lowermost component
 *           of a pixel
 *
 * Static methods:
 *   blend   does optimized alpha blending
 */
template<
	typename _Pixel,
	int _Rshift, int _Rsize,
	int _Gshift, int _Gsize,
	int _Bshift, int _Bsize,
	int _Ashift = 0, int _Asize = 0
>
class Color_traits
{
public:
	enum { Bpp = _Pixel::Bpp };
	typedef typename _Pixel::Color Color;
	typedef typename _Pixel::Pixel Pixel;

	typedef Color_traits<
		_Pixel, _Rshift, _Rsize, _Gshift,
		_Gsize, _Bshift, _Bsize, _Ashift, _Asize
	> This;

	template< int _Shift, int _Size, typename _C = Color >
	class Component
	{
	public:
		typedef _C Color;
		enum {
			Shift = _Shift,
			Size  = _Size,
			Mask  = ((1UL << Size) - 1) << Shift
		};

		static unsigned get(Color c) {
			return (c & Mask) >> Shift;
		}
	};

	typedef Component<_Rshift,_Rsize> R;
	typedef Component<_Gshift,_Gsize> G;
	typedef Component<_Bshift,_Bsize> B;
	typedef Component<_Ashift,_Asize> A;

	template<
		typename _R,
		typename _G,
		typename _B,
		bool _RG = ((unsigned)_R::Shift > (unsigned)_G::Shift),
		bool _RB = ((unsigned)_R::Shift > (unsigned)_B::Shift),
		bool _BG = ((unsigned)_B::Shift > (unsigned)_G::Shift)
	>
	class C_list;
	
	template< typename _R, typename _G, typename _B >
	class C_list<_R, _G, _B, true, true, true>
	{
	public:
		typedef _R Msb;
		typedef _B Mid;
		typedef _G Lsb;
	};

	template< typename _R, typename _G, typename _B >
	class C_list<_R, _G, _B, true, true, false>
	{
	public:
		typedef _R Msb;
		typedef _G Mid;
		typedef _B Lsb;
	};

	template< typename _R, typename _G, typename _B >
	class C_list<_R, _G, _B, true, false, true>
	{
	public:
		typedef _B Msb;
		typedef _R Mid;
		typedef _G Lsb;
	};

	template< typename _R, typename _G, typename _B >
	class C_list<_R, _G, _B, false, true, false>
	{
	public:
		typedef _G Msb;
		typedef _R Mid;
		typedef _B Lsb;
	};

	template< typename _R, typename _G, typename _B >
	class C_list<_R, _G, _B, false, false, false>
	{
	public:
		typedef _G Msb;
		typedef _B Mid;
		typedef _R Lsb;
	};

	template< typename _R, typename _G, typename _B >
	class C_list<_R, _G, _B, false, false, true>
	{
	public:
		typedef _B Msb;
		typedef _G Mid;
		typedef _R Lsb;
	};

	class C_space : public C_list<R,G,B>
	{
	public:
		enum {
			Mix_mask = (R::Mask | G::Mask | B::Mask)
				& ~((1UL << R::Shift) | (1UL << G::Shift) 
				  | (1UL << B::Shift))
		};
	};

	/*** BLEND 16BIT COLOR WITH SPECIFIED ALPHA VALUE ***/
	static Color blend(Color color, int alpha) {
		enum {
			AMask = C_space::Msb::Mask | C_space::Lsb::Mask,
			BMask = C_space::Mid::Mask
		};

		return ((((alpha >> 3) * (color & AMask)) >> 5) & AMask)
		      | (((alpha * (color & BMask)) >> 8) & BMask);
	}
};

/*** PRIVATE color conversion functions */
namespace Conv_local {

template< int X >
class IT
{};

/*** Conversion for a single color component */
template< typename From, typename To >
static inline typename To::Color convert_comp(typename From::Color c, IT<true> const &) {
	enum { Sh = From::Shift - To::Shift + From::Size - To::Size };
	return ((c & From::Mask) >> Sh) & To::Mask;
}

template< typename From, typename To >
static inline typename To::Color convert_comp(typename From::Color c, IT<false> const &) {
	enum { Sh = From::Shift - To::Shift + From::Size - To::Size };
	return (((typename To::Color)(c & From::Mask)) << -Sh) & To::Mask;
}

template< typename From, typename To >
static inline typename To::Color convert_comp(typename From::Color c) {
	enum { Sh = From::Shift - To::Shift + From::Size - To::Size };
	return convert_comp<From, To>(c, IT<(Sh > 0)>());
}
}


/*** PUBLIC conversion functions from one color mode to another */
/* Provides:
 *   convert   to convert a single color from one color space to another
 *   blit      paints the given color to the given pixel, does color
 *             conversion if necessary, and considers alpha values if
 *             available
 */
template< typename From, typename To >
class Conv
{
public:
	typedef typename To::Color T_color;
	typedef typename To::Pixel T_pixel;
	typedef typename From::Color F_color;

	static T_color convert(F_color c) {
		typedef typename To::R TR;
		typedef typename To::G TG;
		typedef typename To::B TB;
		typedef typename From::R FR;
		typedef typename From::G FG;
		typedef typename From::B FB;
		return Conv_local::convert_comp<FR, TR>(c)
		       | Conv_local::convert_comp<FG, TG>(c)
		       | Conv_local::convert_comp<FB, TB>(c);
	}

	static void blit(F_color c, T_pixel *d) {
		if (From::A::Size > 0)
			*d = To::blend(convert(c), From::A::get(c))
			       + To::blend(*d, 255 - From::A::get(c));
		else
			*d = convert(c);
	}

};

/*** Specialized conversion if source and target color space are equal */
template< typename T >
class Conv<T,T>
{
public:
	typedef typename T::Color T_color;
	typedef typename T::Pixel T_pixel;
	typedef typename T::Color F_color;

	static T_color convert(F_color c) { return c; }
	static void blit(F_color c, T_pixel *d) {
		if (T::A::Size > 0)
			*d = T::blend(c, T::A::get(c)) + T::blend(*d, 255 - T::A::get(c));
		else
			*d = c;
	}
};


/*** Typical instances for color spaces */
typedef Color_traits<Pixel_traits<2>, 10,5,5,5,0,5>      Rgb15;
typedef Color_traits<Pixel_traits<2>, 0,5,5,5,10,5>      Bgr15;
typedef Color_traits<Pixel_traits<2>, 11,5,5,6,0,5>      Rgb16;
typedef Color_traits<Pixel_traits<3>, 16,8,8,8,0,8>      Rgb24;
typedef Color_traits<Pixel_traits<4>, 16,8,8,8,0,8>      Rgb32;
typedef Color_traits<Pixel_traits<4>, 24,8,16,8,8,8,0,8> Rgba32;

#endif
