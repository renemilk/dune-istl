// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
#ifndef DUNE_AMG_COMBINEDFUNCTOR_HH
#define DUNE_AMG_COMBINEDFUNCTOR_HH

#include <dune/common/tuples.hh>
namespace Dune
{
  namespace Amg
  {

    template<std::size_t i>
    struct ApplyHelper
    {
      template<class TT, class T>
      static void apply(TT tuple, const T& t)
      {
        get<i-1>(tuple) (t);
        ApplyHelper<i-1>::apply(tuple, t);
      }
    };
    template<>
    struct ApplyHelper<0>
    {
      template<class TT, class T>
      static void apply(TT tuple, const T& t)
      {}
    };

    template<typename T>
    class CombinedFunctor :
      public T
    {
    public:
      CombinedFunctor(const T& tuple)
        : T(tuple)
      {}

      template<class T1>
      void operator()(const T1& t)
      {
        ApplyHelper<tuple_size<T>::value>::apply(*this, t);
      }
    };


  } //namespace Amg
} // namespace Dune
#endif
