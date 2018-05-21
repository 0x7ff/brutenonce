#define W_NONZERO(i) ((((i) > 2) && ((i) < 15)) ? (0u) : (w[i]))
#define S(i) w[i] = rotate(W_NONZERO((i) - 3) ^ W_NONZERO((i) - 8) ^ W_NONZERO((i) - 14) ^ W_NONZERO((i) - 16), 1u);
#define R0(b, c, d) (bitselect(d, c, b) + 0x5a827999u)
#define R1(b, c, d) (((b) ^ (c) ^ (d)) + 0x6ed9eba1u)
#define R2(b, c, d) (bitselect(b, c, (d) ^ (b)) + 0x8f1bbcdcu)
#define R3(b, c, d) (((b) ^ (c) ^ (d)) + 0xca62c1d6u)
#define R(a, b, c, d, e, l, i)                           \
	(e) += rotate(a, 5u) + R##l(b, c, d) + W_NONZERO(i); \
	(b) = rotate(b, 30u)

#define R5(l, i)                  \
	R(a, b, c, d, e, l, i);       \
	R(e, a, b, c, d, l, (i) + 1); \
	R(d, e, a, b, c, l, (i) + 2); \
	R(c, d, e, a, b, l, (i) + 3); \
	R(b, c, d, e, a, l, (i) + 4);

#define S2(i) S(i) S((i) + 1)
#define S4(i) S2(i) S2((i) + 2)
#define S8(i) S4(i) S4((i) + 4)
#define S16(i) S8(i) S8((i) + 8)
#define S32(i) S16(i) S16((i) + 16)
#define R15(l, i) R5(l, i) R5(l, (i) + 5) R5(l, (i) + 10)
#define R20(l, i) R15(l, i) R5(l, (i) + 15)

__kernel void
brutenonce(__global uint *nonce, __constant uint *n, const uint w0, const uint w16, const uint w19, const uint w22, const uint w24, const uint w28, const uint w30, const uint w1)
{
	uint a = 0x67452301u, b = 0xefcdab89u, c = 0x98badcfeu, d = 0x10325476u, e = 0xc3d2e1f0u, w[80];
	w[0] = w0;
	w[1] = w1 + get_global_id(0);
	
	w[2] = 0x80000000u;
	w[15] = 0x40u;
	w[16] = w16;
	w[18] = 0x81u;
	w[19] = w19;
	w[21] = 0x102u;
	w[22] = w22;
	w[24] = w24;
	w[27] = 0x408u;
	w[28] = w28;
	w[30] = w30;
	
	S(17);
	S(20);
	S(23);
	S2(25);
	S(29);

	S32(31);
	S8(63);
	S4(71);
	S2(75);
	S(77);

	R20(0, 0);
	R20(1, 20);
	R20(2, 40);
	R15(3, 60);
	R(a, b, c, d, e, 3, 75);
	R(e, a, b, c, d, 3, 76);
	R(d, e, a, b, c, 3, 77);
	if(e == n[4]) {
		S(78);
		R(c, d, e, a, b, 3, 78);
		if(d == n[3] && b == n[1]) {
			S(79);
			R(b, c, d, e, a, 3, 79);
			if(c == n[2] && a == n[0]) {
				*nonce = w[1];
			}
		}
	}
}
