#!/bin/bash
########################################################################
# A quick hack to generate Signature templates.
# Usage:
#  $0 to
#  $0 from to
#
# The first form is equivalent to: $0 0 to
#
# Generates Signature<> specializations taking $from .. $to arguments.
# e.g. ($0 3) creates specializations taking 0..3 arguments.
########################################################################

from=${1-0}
to=$2

if [[ 'x' = "x${to}" ]]; then
    to=$from
    from=0
fi

tparam='typename RV'
targs=''

if [[ 0 = $from ]]; then
from=$((from + 1))
cat <<EOF

template <typename RV>
struct Signature< RV () >
{
    typedef RV ReturnType;
    enum { IsConst = 0 };
    typedef void Context;
    typedef RV (*FunctionType)();
    typedef tmp::NilType Head;
    typedef Head Tail;
};
template <typename RV>
struct Signature< RV (*)() > : Signature<RV ()>
{};

template <typename T, typename RV>
struct Signature< RV (T::*)() > : Signature<RV ()>
{
    typedef T Context;
    typedef RV (T::*FunctionType)();
};

#if defined(CVV8_CONFIG_ENABLE_CONST_OVERLOADS) && CVV8_CONFIG_ENABLE_CONST_OVERLOADS

template <typename RV>
struct Signature< RV () const > : Signature<RV ()>
{
    enum { IsConst = 1 };
};
template <typename T, typename RV>
struct Signature< RV (T::*)() const > : Signature<RV () const>
{
    typedef T Context;
    typedef RV (T::*FunctionType)() const;
};

#endif /* CVV8_CONFIG_ENABLE_CONST_OVERLOADS */
EOF

fi # $from==0

i=$from
while [[ $i -le $to ]]; do
    tparam="${tparam}, typename A${i}"
    if [[ "X" != "X${targs}" ]]; then
        targs="${targs}, A${i}"
    else
        targs="A1"
    fi

    head="${targs%%,*}"
    tail="${targs#*,}"
    if [[ "$tail" = "$head" ]]; then # happens on 1-arity form
        # reminder to self: whether or not Sig<x (y)>::Tail should be NilType
        # or Sig<x ()> is basically a philosophical question, i think. i find
        # the latter more asthetic but it does cause minor problems in some
        # particular type-list-using algorithms.
        if [[ $i -eq 0 ]]; then
            tail="tmp::NilType"
        else
            tail="Signature< RV () >"
        fi
    else
        tail="Signature<RV (${tail})>"
    fi
    
    cat <<EOF
//! Specialization for ${i} arg(s).
template <$tparam>
struct Signature< RV (${targs}) >
{
    typedef RV ReturnType;
    static const bool IsConst = false;
    typedef void Context;
    typedef RV (*FunctionType)(${targs});
    typedef ${head} Head;
    typedef ${tail} Tail;
};

//! Specialization for ${i} arg(s).
template <$tparam>
struct Signature< RV (*)(${targs}) > : Signature<RV (${targs})>
{};

//! Specialization for T non-const methods taking ${i} arg(s).
template <typename T, $tparam>
struct Signature< RV (T::*)(${targs}) > : Signature<RV (${targs})>
{
    typedef T Context;
    typedef RV (T::*FunctionType)(${targs});
};

#if defined(CVV8_CONFIG_ENABLE_CONST_OVERLOADS) && CVV8_CONFIG_ENABLE_CONST_OVERLOADS
//! Specialization for ${i} arg(s).
template <$tparam>
struct Signature< RV (${targs}) const > : Signature<RV (${targs})>
{
    static const bool IsConst = true;
};

//! Specialization for T const methods taking ${i} arg(s).
template <typename T, $tparam>
struct Signature< RV (T::*)(${targs}) const > : Signature<RV (${targs}) const>
{
    typedef T Context;
    typedef RV (T::*FunctionType)(${targs}) const;
};
#endif /*CVV8_CONFIG_ENABLE_CONST_OVERLOADS*/
EOF
    #echo $tparam
    #echo $targs
    i=$((i + 1))
done

