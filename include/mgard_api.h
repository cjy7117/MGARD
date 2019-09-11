// Copyright 2017, Brown University, Providence, RI.
//
//                         All Rights Reserved
//
// Permission to use, copy, modify, and distribute this software and
// its documentation for any purpose other than its incorporation into a
// commercial product or service is hereby granted without fee, provided
// that the above copyright notice appear in all copies and that both
// that copyright notice and this permission notice appear in supporting
// documentation, and that the name of Brown University not be used in
// advertising or publicity pertaining to distribution of the software
// without specific, written prior permission.
//
// BROWN UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR ANY
// PARTICULAR PURPOSE.  IN NO EVENT SHALL BROWN UNIVERSITY BE LIABLE FOR
// ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
//
// MGARD: MultiGrid Adaptive Reduction of Data
// Authors: Mark Ainsworth, Ozan Tugluk, Ben Whitney
// Corresponding Author: Ozan Tugluk
//
// version: 0.0.0.2
//
// This file is part of MGARD.
//
// MGARD is distributed under the OSI-approved Apache License, Version 2.0.
// See accompanying file Copyright.txt for details.
//

#include "mgard.h"


#ifndef MGARD_API_H
#define MGARD_API_H


//comments!!!!
/// Use this version of mgard_compress to compress your data with a tolerance measured in  relative L-infty norm
unsigned char *mgard_compress(int itype_flag, double  *data, int &out_size, int nrow, int ncol, int nfib, double tol);

// Use this version of mgard_compress to compress your data with a tolerance measured in  relative s-norm.
//Set s=0 for L2-norm
unsigned char *mgard_compress(int itype_flag, double  *data, int &out_size, int nrow, int ncol, int nfib, double tol, double s);

// Use this version of mgard_compress to compress your data with a tolerance measured in  relative s-norm
// where the tolerance is specified for a quantity of interest qoi
unsigned char *mgard_compress(int itype_flag, double  *data, int &out_size, int nrow, int ncol, int nfib, double tol, double (*qoi) (int, int, int, std::vector<double>), double s );

// Use this version of mgard_compress to compute the  s-norm of a quantity of interest.
double  mgard_compress( int nrow, int ncol, int nfib,  double (*qoi) (int, int, int, std::vector<double>), double s );
 
 // Use this version of mgard_compress to compress your data with a tolerance in -s norm
 // with given s-norm of quantity of interest qoi
unsigned char *mgard_compress(int itype_flag, double  *data, int &out_size, int nrow, int ncol, int nfib, double tol, double norm_of_qoi, double s );

double  *mgard_decompress(int itype_flag, unsigned char *data, int data_len, int nrow, int ncol, int nfib); // decompress L-infty compressed data

double  *mgard_decompress(int itype_flag, unsigned char *data, int data_len, int nrow, int ncol, int nfib, double s); // decompress s-norm


#endif