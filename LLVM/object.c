#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
  object->next = vm.objects;
  object->isMarked = false;

  vm.objects = object;
#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

  return object;
}

ObjInstance* newInstance(ObjClass *klass) {
  ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, CECILE_OBJ_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
  return instance;
}

ObjClass* newClass(ObjString *name) {
  ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  klass->name = name;
  return klass;
}

ObjClosure* newClosure(ObjFunction* function) {
  ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*,
                                   function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjFunction* newFunction() {
  ObjFunction* function = ALLOCATE_OBJ(ObjFunction, CECILE_OBJ_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjNative* newNative(NativeFn function) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, CECILE_OBJ_NATIVE);
  native->function = function;
  return native;
}

static ObjString* allocateString(char* chars, int length, int32_t hash) {
  ObjString* string = ALLOCATE_OBJ(ObjString, CECILE_OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  push(CECILE_OBJ_VAL(string));
  tableSet(&vm.strings, string, CECILE_NIL_VAL);
  pop();
  return string;
}

static uint32_t hashString(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString* takeString(char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }
  return allocateString(chars, length, hash);

}

ObjString* copyString(const char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
  if (interned != NULL) return interned;
  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

ObjUpvalue* newUpvalue(Value* slot) {
  ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = CECILE_NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static void printFunction(ObjFunction* function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (CECILE_OBJ_TYPE(value)) {
    case CECILE_OBJ_INSTANCE:
      printf("%s instance",
             CECILE_AS_INSTANCE(value)->klass->name->chars);
      break;
    case OBJ_CLASS:
      printf("%s", CECILE_AS_CLASS(value)->name->chars);
      break;
    case OBJ_UPVALUE:
      printf("upvalue");
      break;
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->function);
      break;
    case CECILE_OBJ_FUNCTION:
      printFunction(CECILE_AS_FUNCTION(value));
      break;
    case CECILE_OBJ_NATIVE:
      printf("<native fn>");
      break;
    case CECILE_OBJ_STRING:
      printf("%s", CECILE_AS_CSTRING(value));
      break;
  }
}
