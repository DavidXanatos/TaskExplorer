/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#pragma once

class ComplexNumber
{
  public:
    ComplexNumber();
    ComplexNumber( double r, double i = 0.0 );

    double real() const;
    double imag() const;

    friend ComplexNumber operator*(
        const ComplexNumber&, const ComplexNumber& );

    friend ComplexNumber operator+(
        const ComplexNumber&, const ComplexNumber& );

    friend ComplexNumber operator-(
        const ComplexNumber&, const ComplexNumber& );
    friend ComplexNumber operator/(
        const ComplexNumber&, const ComplexNumber& );

  private:
    double m_real;
    double m_imag;
};

inline ComplexNumber::ComplexNumber()
    : m_real( 0.0 )
    , m_imag( -0.0 )
{
}

inline ComplexNumber::ComplexNumber( double re, double im )
    : m_real( re )
    , m_imag( im )
{
}

inline double ComplexNumber::real() const
{
    return m_real;
}

inline double ComplexNumber::imag() const
{
    return m_imag;
}

inline ComplexNumber operator+(
    const ComplexNumber& x1, const ComplexNumber& x2 )
{
    return ComplexNumber( x1.m_real + x2.m_real, x1.m_imag + x2.m_imag );
}

inline ComplexNumber operator-(
    const ComplexNumber& x1, const ComplexNumber& x2 )
{
    return ComplexNumber( x1.m_real - x2.m_real, x1.m_imag - x2.m_imag );
}

inline ComplexNumber operator*(
    const ComplexNumber& x1, const ComplexNumber& x2 )
{
    return ComplexNumber( x1.m_real * x2.m_real - x1.m_imag * x2.m_imag,
        x1.m_real * x2.m_imag + x2.m_real * x1.m_imag );
}

inline ComplexNumber operator/(
    const ComplexNumber& x1, const ComplexNumber& x2 )
{
    double denom = x2.m_real * x2.m_real + x2.m_imag * x2.m_imag;

    return ComplexNumber(
        ( x1.m_real * x2.m_real + x1.m_imag * x2.m_imag ) / denom,
        ( x1.m_imag * x2.m_real - x2.m_imag * x1.m_real ) / denom
        );
}
