/*
* Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
*
* @APPLE_LICENSE_HEADER_START@
*
* The contents of this file constitute Original Code as defined in and
* are subject to the Apple Public Source License Version 1.1 (the
* "License").  You may not use this file except in compliance with the
* License.  Please obtain a copy of the License at
* http://www.apple.com/publicsource and read it before using this file.
*
* This Original Code and all software distributed under the License are
* distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
* EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
* INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
* License for the specific language governing rights and limitations
* under the License.
*
* @APPLE_LICENSE_HEADER_END@
*/
/* Copyright (c) 1992, NeXT Computer, Inc.  All rights reserved.
*
*	File:	libc/m98k/gen/fp.h
*	Author:	Derek B Clegg, NeXT Computer, Inc.
*
* HISTORY
*  11-Nov-92  Derek B Clegg (dclegg@next.com)
*	Created.
*
* Common definitions for floating-point numbers.
*/

/* The following definitions for for double precision IEEE format numbers. */

#define EXPONENT_BIAS 1023

#define SIGN_BITS 1
#define EXPONENT_BITS 11
#define FRACTION_BITS 52
#define HI_FRACTION_BITS 20
#define LO_FRACTION_BITS 32

struct double_format {
unsigned sign: SIGN_BITS;
unsigned exponent: EXPONENT_BITS;
unsigned hi_fraction: HI_FRACTION_BITS;
unsigned lo_fraction: LO_FRACTION_BITS;
};

union dbl {
struct double_format s;
unsigned int u[2];
double value;
};

#define PlusInfinity	(1.0/0.0)
#define MinusInfinity	(-1.0/0.0)

#define not_a_number(x)		((x) != (x))
#define positive_infinity(x)	((x) == PlusInfinity)
#define negative_infinity(x)	((x) == MinusInfinity)
