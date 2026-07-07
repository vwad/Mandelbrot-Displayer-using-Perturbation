#ifndef COMPLEX_BIG_NUM_H_
#define COMPLEX_BIG_NUM_H_
#include "gmpxx.h"
#include <string>

class ComplexBigNum {
    public:
        mpf_class re;
        mpf_class im;

        ComplexBigNum(double re, double im) : re(re), im(im) {};
        ComplexBigNum(std::string re, std::string im) : re(re), im(im) {};
        ComplexBigNum(const mpf_class &re, const mpf_class &im) : re(re), im(im) {};

        ComplexBigNum operator+=(const ComplexBigNum &n) {
            re += n.re;
            im += n.im;
            return *this;
        }

        ComplexBigNum operator*(const ComplexBigNum &n) {
            return ComplexBigNum(re*n.re-im*n.im,re*n.im+im*n.re);
        }

        ComplexBigNum operator+(const ComplexBigNum &n) {
            return ComplexBigNum(re+n.re,im+n.im);
        }

        ComplexBigNum operator*=(const ComplexBigNum &n) {
            const mpf_class r = re * n.re - im * n.im;
            im = n.re * im + re * n.im;
            re = r;
            return *this;
        }

        ComplexBigNum operator*(const mpf_class &n) {
            return ComplexBigNum(re * n, im * n);
        }

        ComplexBigNum operator*=(const mpf_class &n) {
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
            mpf_class n = re * re + im * im;
            mpf_class inv = mpf_class(1.0)/n;

            result.im = -result.im;
            result*=inv;
            return result;
        }

        inline mpf_class norm () const {
            return re * re + im * im;;
        }


};
#endif