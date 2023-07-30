#include <stdio.h>

#include "object.h"
#include "memory.h"
#include "value.h"
#include <string.h>

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values,
                                                                  oldCapacity, array->capacity);
        }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray *array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  initValueArray(array);
}

void printValue(Value value) {
#ifdef NAN_BOXING
  if (CECILE_IS_BOOL(value)) {
    printf(CECILE_AS_BOOL(value) ? "true" : "false");
  } else if (CECILE_IS_NIL(value)) {
    printf("nil");
  } else if (CECILE_IS_NUMBER(value)) {
    printf("%g", CECILE_AS_NUMBER(value));
  } else if (CECILE_IS_OBJ(value)) {
    printObject(value);
  }
#else
   switch (value.type) {
    case CECILE_VAL_BOOL:
      printf(CECILE_AS_BOOL(value) ? "true" : "false");
      break;
    case CECILE_VAL_NIL: printf("nil"); break;
    case CECILE_VAL_NUMBER: printf("%g", CECILE_AS_NUMBER(value)); break;
    case CECILE_VAL_OBJ: printObject(value); break;
  }
#endif
}

bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
  if (CECILE_IS_NUMBER(a) && CECILE_IS_NUMBER(b)) {
    return CECILE_AS_NUMBER(a) == CECILE_AS_NUMBER(b);
  }
  return a == b;
#else
  if (a.type != b.type) return false;

  switch (a.type) {
    case CECILE_VAL_BOOL: return CECILE_AS_BOOL(a) == CECILE_AS_BOOL(b);
    case CECILE_VAL_NIL: return true;
    case CECILE_VAL_NUMBER: return CECILE_AS_NUMBER(a) == CECILE_AS_NUMBER(b);
    case CECILE_VAL_OBJ:    return CECILE_AS_OBJ(a) == CECILE_AS_OBJ(b);
    default: return false;

  }
 #endif
}
