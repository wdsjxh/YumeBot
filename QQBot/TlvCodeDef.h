#ifndef TLV_CODE
#define TLV_CODE(name, code, ...)
#endif

#ifndef ATTRIBUTE_SET
#define ATTRIBUTE_SET(...) __VA_ARGS__
#endif

#ifndef NO_OP
#define NO_OP
#endif

#ifndef IS_OPTIONAL
#define IS_OPTIONAL(defaultValue) NO_OP
#endif

#ifndef TEMPLATE_ARGUMENT
#define TEMPLATE_ARGUMENT(...) NO_OP
#endif

#ifndef FIELD
#define FIELD(name, tag, type, attribute)
#endif

#ifndef BYTE
#define BYTE(name, tag, attribute) FIELD(name, tag, Byte, attribute)
#endif

#ifndef SHORT
#define SHORT(name, tag, attribute) FIELD(name, tag, Short, attribute)
#endif

#ifndef INT
#define INT(name, tag, attribute) FIELD(name, tag, Int, attribute)
#endif

#ifndef LONG
#define LONG(name, tag, attribute) FIELD(name, tag, Long, attribute)
#endif

#ifndef FLOAT
#define FLOAT(name, tag, attribute) FIELD(name, tag, Float, attribute)
#endif

#ifndef DOUBLE
#define DOUBLE(name, tag, attribute) FIELD(name, tag, Double, attribute)
#endif

#ifndef STRING1
#define STRING1(name, tag, attribute) FIELD(name, tag, String1, attribute)
#endif

#ifndef STRING4
#define STRING4(name, tag, attribute) FIELD(name, tag, String4, attribute)
#endif

#ifndef MAP
#define MAP(name, tag, attribute) FIELD(name, tag, Map, attribute)
#endif

#ifndef LIST
#define LIST(name, tag, attribute) FIELD(name, tag, List, attribute)
#endif

#ifndef STRUCT
#define STRUCT(name, tag, attribute) FIELD(name, tag, StructBegin, attribute)
#endif

#ifndef ZERO_TAG
#define ZERO_TAG(name, tag, attribute) FIELD(name, tag, ZeroTag, attribute)
#endif

#ifndef SIMPLE_LIST
#define SIMPLE_LIST(name, tag, attribute) FIELD(name, tag, SimpleList, attribute)
#endif

TLV_CODE(TlvTest, 0, INT(TestInt, 0, ATTRIBUTE_SET()), FLOAT(TestFloat, 1, ATTRIBUTE_SET(IS_OPTIONAL(1.0f))))

#undef SIMPLE_LIST
#undef ZERO_TAG
#undef STRUCT
#undef STRUCT_END
#undef STRUCT_BEGIN
#undef LIST
#undef MAP
#undef STRING4
#undef STRING1
#undef DOUBLE
#undef FLOAT
#undef LONG
#undef INT
#undef SHORT
#undef BYTE
#undef FIELD

#undef TEMPLATE_ARGUMENT
#undef IS_OPTIONAL
#undef NO_OP
#undef ATTRIBUTE_SET

#undef TLV_CODE
