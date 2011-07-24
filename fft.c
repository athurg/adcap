#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#undef USE_ifft

static inline void swap (float *a, float *b)
{
	float t;
	t = *a;
	*a = *b;
	*b = t;
}

static void bitrp (float xreal [], float ximag [], int n)
{
	// 位反转置换 Bit-reversal Permutation
	int i, j, a, b, p;

	for (i = 1, p = 0; i < n; i *= 2) {
		p ++;
	}
	for (i = 0; i < n; i ++) {
		a = i;
		b = 0;
		for (j = 0; j < p; j ++) {
			b = (b << 1) + (a & 1);    // b = b * 2 + a % 2;
			a >>= 1;        // a = a / 2;
		}
		if ( b > i) {
			swap (&xreal[i], &xreal[b]);
			swap (&ximag[i], &ximag[b]);
		}
	}
}

void fft(float xreal [], float ximag [], int n)
{
	// 快速傅立叶变换，将复数 x 变换后仍保存在 x 中，xreal, ximag 分别是 x 的实部和虚部
	float *wreal, *wimag, treal, timag, ureal, uimag, arg;
	int m, k, j, t, index1, index2;

	wreal = malloc(n * sizeof(float));
	wimag = wreal + n/2;

	bitrp (xreal, ximag, n);

	// 计算1的前 n/2个n次方根的共轭复数
	// W'j = wreal [j] + i * wimag [j] , j = 0, 1, ... , n / 2 - 1
	arg = - 2 * M_PI / n;
	treal = cos (arg);
	timag = sin (arg);
	wreal [0] = 1.0;
	wimag [0] = 0.0;
	for (j = 1; j < n / 2; j ++) {
		wreal [j] = wreal [j - 1] * treal - wimag [j - 1] * timag;
		wimag [j] = wreal [j - 1] * timag + wimag [j - 1] * treal;
	}

	for (m = 2; m <= n; m *= 2) {
		for (k = 0; k < n; k += m) {
			for (j = 0; j < m / 2; j ++) {
				index1 = k + j;
				index2 = index1 + m / 2;
				t = n * j / m;    // 旋转因子 w 的实部在 wreal [] 中的下标为 t
				treal = wreal [t] * xreal [index2] - wimag [t] * ximag [index2];
				timag = wreal [t] * ximag [index2] + wimag [t] * xreal [index2];
				ureal = xreal [index1];
				uimag = ximag [index1];
				xreal [index1] = ureal + treal;
				ximag [index1] = uimag + timag;
				xreal [index2] = ureal - treal;
				ximag [index2] = uimag - timag;
			}
		}
	}
	free(wreal);
}

#ifdef USE_ifft
void  ifft (float xreal [], float ximag [], int n)
{
	// 快速傅立叶逆变换
	float *wreal, *wimag, treal, timag, ureal, uimag, arg;
	int m, k, j, t, index1, index2;

	wreal = malloc(n * sizeof(float));
	wimag = wreal + n/2;

	bitrp (xreal, ximag, n);

	// 计算1的前 n/2个n次方根
	// Wj = wreal [j] + i * wimag [j] , j = 0, 1, ... , n / 2 - 1
	arg = 2 * M_PI / n;
	treal = cos (arg);
	timag = sin (arg);
	wreal [0] = 1.0;
	wimag [0] = 0.0;
	for (j = 1; j < n / 2; j ++) {
		wreal [j] = wreal [j - 1] * treal - wimag [j - 1] * timag;
		wimag [j] = wreal [j - 1] * timag + wimag [j - 1] * treal;
	}

	for (m = 2; m <= n; m *= 2) {
		for (k = 0; k < n; k += m) {
			for (j = 0; j < m / 2; j ++) {
				index1 = k + j;
				index2 = index1 + m / 2;
				t = n * j / m;    // 旋转因子 w 的实部在 wreal [] 中的下标为 t
				treal = wreal [t] * xreal [index2] - wimag [t] * ximag [index2];
				timag = wreal [t] * ximag [index2] + wimag [t] * xreal [index2];
				ureal = xreal [index1];
				uimag = ximag [index1];
				xreal [index1] = ureal + treal;
				ximag [index1] = uimag + timag;
				xreal [index2] = ureal - treal;
				ximag [index2] = uimag - timag;
			}
		}
	}

	for (j=0; j < n; j ++) {
		xreal [j] /= n;
		ximag [j] /= n;
	}

	free(wreal);
}
#endif

