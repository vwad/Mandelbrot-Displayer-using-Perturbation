#ifndef COMPLEX_BIG_NUM_H_
#define COMPLEX_BIG_NUM_H_
#include "gmpxx.h"
#include "mpreal.h"
#include <string>

using namespace mpfr;

class ComplexBigNum {
    public:
        mpreal re;
        mpreal im;

        ComplexBigNum(double re, double im) : re(re), im(im) {};
        ComplexBigNum(std::string re, std::string im) : re(re), im(im) {};
        ComplexBigNum(const mpreal &re, const mpreal &im) : re(re), im(im) {};

        ComplexBigNum operator+=(const ComplexBigNum &n) {
            re = re + n.re;
            im = im + n.im;
            return *this;
        }

        ComplexBigNum operator*(const ComplexBigNum &n) {
            return ComplexBigNum(re*n.re-im*n.im,re*n.im+im*n.re);
        }

        ComplexBigNum operator+(const ComplexBigNum &n) {
            return ComplexBigNum(re+n.re,im+n.im);
        }

        ComplexBigNum operator*=(const ComplexBigNum &n) {
            const mpreal r = re * n.re - im * n.im;
            im = n.re * im + re * n.im;
            re = r;
            return *this;
        }

        ComplexBigNum operator*(const mpreal &n) {
            return ComplexBigNum(re * n, im * n);
        }

        ComplexBigNum operator*=(const mpreal &n) {
            re *= n;
            im *= n;
            return *this;
        }

        ComplexBigNum operator/=(const ComplexBigNum &n) {
            *this = *this * n.reciprocal();
            return *this;
        }

        inline ComplexBigNum reciprocal() const {
            ComplexBigNum result = *this;
            mpreal n = re * re + im * im;
            mpreal inv = mpreal(1.0)/n;

            result.im = -result.im;
            result = result * inv;
            return result;
        }

        inline mpreal norm () const {
            return re * re + im * im;;
        }


};
#endif