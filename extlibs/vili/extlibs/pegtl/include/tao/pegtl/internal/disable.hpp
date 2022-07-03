// Copyright (c) 2014-2021 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAO_PEGTL_INTERNAL_DISABLE_HPP
#define TAO_PEGTL_INTERNAL_DISABLE_HPP

#include "../config.hpp"

#include "enable_control.hpp"
#include "seq.hpp"
#include "success.hpp"

#include "../apply_mode.hpp"
#include "../rewind_mode.hpp"
#include "../type_list.hpp"

namespace TAO_PEGTL_NAMESPACE::internal
{
   template< typename... Rules >
   struct disable
      : disable< seq< Rules... > >
   {};

   template<>
   struct disable<>
      : success
   {};

   template< typename Rule >
   struct disable< Rule >
   {
      using rule_t = disable;
      using subs_t = type_list< Rule >;

      template< apply_mode,
                rewind_mode M,
                template< typename... >
                class Action,
                template< typename... >
                class Control,
                typename ParseInput,
                typename... States >
      [[nodiscard]] static bool match( ParseInput& in, States&&... st )
      {
         return Control< Rule >::template match< apply_mode::nothing, M, Action, Control >( in, st... );
      }
   };

   template< typename... Rules >
   inline constexpr bool enable_control< disable< Rules... > > = false;

}  // namespace TAO_PEGTL_NAMESPACE::internal

#endif
