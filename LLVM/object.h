#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "value.h"

#define CECILE_OBJ_TYPE(value)        (CECILE_AS_OBJ(value)->type)
#define CECILE_IS_FUNCTION(value)     isObjType(value, CECILE_OBJ_FUNCTION)
#define CECILE_IS_NATIVE(value)       isObjType(value, CECILE_OBJ_NATIVE)
#define CECILE_IS_STRING(value)       isObjType(value, CECILE_OBJ_STRING)

#define CECILE_AS_STRING(value)       ((ObjString*)CECILE_AS_OBJ(value))
#define CECILE_AS_FUNCTION(value)     ((ObjFunction*)CECILE_AS_OBJ(value))
#define CECILE_AS_NATIVE(value) \
    (((ObjNative*)CECILE_AS_OBJ(value))->function)
#define CECILE_AS_CSTRING(value)      (((ObjString*)CECILE_AS_OBJ(value))->chars)


typedef enum {
  CECILE_OBJ_FUNCTION,
  CECILE_OBJ_NATIVE,
  CECILE_OBJ_STRING,
} ObjType;


struct Obj {
  ObjType type;
  struct Obj* next;
};

typedef struct {
  Obj obj;
  int arity;
  Chunk chunk;
  ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

struct ObjString {
  Obj obj;
  int length;
  char* chars;
  uint32_t hash;
};

ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
  return CECILE_IS_OBJ(value) && CECILE_AS_OBJ(value)->type == type;
}

#endif
