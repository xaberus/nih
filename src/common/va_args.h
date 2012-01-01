#define __VA_ARGS_ARG16(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, ...) _15
#define __VA_ARGS_HAS_COMMA(...) __VA_ARGS_ARG16(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)
#define __VA_ARGS_HAS_NO_COMMA(...) __VA_ARGS_ARG16(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1)
#define __VA_ARGS_TRIGGER_PARENTHESIS(...) ,
#define __VA_ARGS_HAS_ZERO_OR_ONE_ARGS(...) __VA_ARGS__HAS_ZERO_OR_ONE_ARGS(__VA_ARGS_HAS_COMMA(__VA_ARGS__), __VA_ARGS_HAS_COMMA(__VA_ARGS_TRIGGER_PARENTHESIS __VA_ARGS__), __VA_ARGS_HAS_COMMA(__VA_ARGS__ (~)), __VA_ARGS_HAS_COMMA(__VA_ARGS_TRIGGER_PARENTHESIS __VA_ARGS__ (~)))
#define __VA_ARGS_PASTE5(_0, _1, _2, _3, _4) _0 ## _1 ## _2 ## _3 ## _4
#define __VA_ARGS__HAS_ZERO_OR_ONE_ARGS(_0, _1, _2, _3) __VA_ARGS_HAS_NO_COMMA(__VA_ARGS_PASTE5(_IS_EMPTY_CASE_, _0, _1, _2, _3))
#define _IS_EMPTY_CASE_0001 ,
#define __VA_ARGS_VA_0(...) __VA_ARGS_HAS_ZERO_OR_ONE_ARGS(__VA_ARGS__)
#define __VA_ARGS_VA_1(...) __VA_ARGS_HAS_ZERO_OR_ONE_ARGS(__VA_ARGS__)
#define __VA_ARGS_VA_2(...) 2
#define __VA_ARGS_VA_3(...) 3
#define __VA_ARGS_VA_4(...) 4
#define __VA_ARGS_VA_5(...) 5
#define __VA_ARGS_VA_6(...) 6
#define __VA_ARGS_VA_7(...) 7
#define __VA_ARGS_VA_8(...) 8
#define __VA_ARGS_VA_9(...) 9
#define __VA_ARGS_VA_10(...) 10
#define __VA_ARGS_VA_11(...) 11
#define __VA_ARGS_VA_12(...) 12
#define __VA_ARGS_VA_13(...) 13
#define __VA_ARGS_VA_14(...) 14
#define __VA_ARGS_VA_15(...) 15
#define __VA_ARGS_VA_16(...) 16
#define __VA_ARGS_PP_RSEQ_N(...) __VA_ARGS_VA_16(__VA_ARGS__),__VA_ARGS_VA_15(__VA_ARGS__),__VA_ARGS_VA_14(__VA_ARGS__),__VA_ARGS_VA_13(__VA_ARGS__), __VA_ARGS_VA_12(__VA_ARGS__),__VA_ARGS_VA_11(__VA_ARGS__),__VA_ARGS_VA_10(__VA_ARGS__), __VA_ARGS_VA_9(__VA_ARGS__), __VA_ARGS_VA_8(__VA_ARGS__),__VA_ARGS_VA_7(__VA_ARGS__),__VA_ARGS_VA_6(__VA_ARGS__),__VA_ARGS_VA_5(__VA_ARGS__), __VA_ARGS_VA_4(__VA_ARGS__),__VA_ARGS_VA_3(__VA_ARGS__),__VA_ARGS_VA_2(__VA_ARGS__),__VA_ARGS_VA_1(__VA_ARGS__), __VA_ARGS_VA_0(__VA_ARGS__)

#define VA_NUM_ARGS(...) VA_NUM_ARGS_IMPL(__VA_ARGS__, __VA_ARGS_PP_RSEQ_N(__VA_ARGS__) )
#define VA_NUM_ARGS_IMPL(...) VA_NUM_ARGS_N(__VA_ARGS__)
#define VA_NUM_ARGS_N( _1, _2, _3, _4, _5, _6, _7, _8, _9,_10,_11,_12,_13,_14,_15,_16,N,...) N
