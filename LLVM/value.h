#ifndef clox_value_h
#define clox_value_h

#include "common.h"
#include <string.h>

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN     ((uint64_t)0x7ffc000000000000)

#define TAG_NIL   1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE  3 // 11.

typedef uint64_t Value;

#define CECILE_IS_BOOL(value)      (((value) | 1) == CECILE_TRUE_VAL)
#define CECILE_IS_NIL(value)       ((value) == CECILE_NIL_VAL)
#define CECILE_IS_NUMBER(value)    (((value) & QNAN) != QNAN)
#define CECILE_AS_NUMBER(value)    valueToNum(value)
#define CECILE_AS_BOOL(value)      ((value) == CECILE_TRUE_VAL)
#define CECILE_IS_OBJ(value) \
    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define CECILE_NUMBER_VAL(num) numToValue(num)
#define CECILE_OBJ_VAL(obj) \
    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))
#define CECILE_AS_OBJ(value) \
    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define CECILE_BOOL_VAL(b)     ((b) ? CECILE_TRUE_VAL : CECILE_FALSE_VAL)
#define CECILE_FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define CECILE_TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define CECILE_NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))

static inline double valueToNum(Value value) {
  double num;
  memcpy(&num, &value, sizeof(Value));
  return num;
}

static inline Value numToValue(double num) {
  Value value;
  memcpy(&value, &num, sizeof(double));
  return value;
}

#else


typedef enum {
  CECILE_VAL_BOOL,
  CECILE_VAL_NIL,
  CECILE_VAL_OBJ,
  CECILE_VAL_NUMBER,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj* obj;
  } as; 
} Value;

#define CECILE_IS_BOOL(value)    ((value).type == CECILE_VAL_BOOL)
#define CECILE_IS_NIL(value)     ((value).type == CECILE_VAL_NIL)
#define CECILE_IS_NUMBER(value)  ((value).type == CECILE_VAL_NUMBER)
#define CECILE_IS_OBJ(value)     ((value).type == CECILE_VAL_OBJ)

#define CECILE_AS_OBJ(value)     ((value).as.obj)
#define CECILE_AS_BOOL(value)   ((value).as.boolean)
#define CECILE_AS_NUMBER(value)   ((value).as.number)

#define CECILE_BOOL_VAL(value)  ((Value){CECILE_VAL_BOOL, {.boolean = value}})
#define CECILE_NIL_VAL          ((Value){CECILE_VAL_NIL, {.number = 0}})
#define CECILE_NUMBER_VAL(value)      ((Value){CECILE_VAL_NUMBER, {.number = value}})
#define CECILE_OBJ_VAL(object)   ((Value){CECILE_VAL_OBJ, {.obj = (Obj*)object}})

#endif

typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif

