#include "chunk.h"
#include <string.h>
#include "common.h"
#include "value.h"
#include "vm.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "compiler.h"
#include "debug.h"
#include <stdarg.h>
#include "object.h"
#include "memory.h"
#include <time.h>
VM vm;

static Value clockNative(int argCount, Value* args) {
  return CECILE_NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", 
            function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}

static void defineNative(const char* name, NativeFn function) {
  push(CECILE_OBJ_VAL(copyString(name, (int)strlen(name))));
  push(CECILE_OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, CECILE_AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void initVM() {
  resetStack();
  vm.objects = NULL;
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;
  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;
  
  initTable(&vm.globals);
  initTable(&vm.strings);

  
  defineNative("clock", clockNative);
}

void freeVM() {
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  freeObjects();
} 

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}

static bool call(ObjClosure* closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError("Expected %d arguments but got %d.",
        closure->function->arity, argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }


  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}


static bool callValue(Value callee, int argCount) {
  if (CECILE_IS_OBJ(callee)) {
    switch (CECILE_OBJ_TYPE(callee)) {
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
      case CECILE_OBJ_NATIVE: {
        NativeFn native = CECILE_AS_NATIVE(callee);
        Value result = native(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
      default:
        break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}

static ObjUpvalue* captureUpvalue(Value* local) {
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue* createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL &&
         vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}
static bool isFalsey(Value value) {
  return CECILE_IS_NIL(value) || (CECILE_IS_BOOL(value) && !CECILE_AS_BOOL(value));
}

static void concatenate() {
  ObjString* b = CECILE_AS_STRING(peek(0));
  ObjString* a = CECILE_AS_STRING(peek(1));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString* result = takeString(chars, length);
  pop();
  pop();
  push(CECILE_OBJ_VAL(result));
}

static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];
  #define READ_BYTE() (*frame->ip++)
  #define READ_SHORT() \
    (frame->ip += 2, \
    (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
  #define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])

  #define READ_STRING() CECILE_AS_STRING(READ_CONSTANT())
  #define BINARY_OP(valueType, op) \
    do { \
      if (!CECILE_IS_NUMBER(peek(0)) || !CECILE_IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = CECILE_AS_NUMBER(pop()); \
      double a = CECILE_AS_NUMBER(pop()); \
      push(valueType(a op b)); \
    } while (false)

  for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
      printf("          ");
      for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(&frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
    #endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_NIL: push(CECILE_NIL_VAL); break;
      case OP_TRUE: push(CECILE_BOOL_VAL(true)); break;
      case OP_FALSE: push(CECILE_BOOL_VAL(false)); break;
      case OP_POP: pop(); break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name); 
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }

      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(CECILE_BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_GREATER: BINARY_OP(CECILE_BOOL_VAL, >); break;
      case OP_LESS: BINARY_OP(CECILE_BOOL_VAL, <); break;
      case OP_ADD: {
        if (CECILE_IS_STRING(peek(0)) && CECILE_IS_STRING(peek(1))) {
          concatenate();
        } else if (CECILE_IS_NUMBER(peek(0)) && CECILE_IS_NUMBER(peek(1))) {
          double b = CECILE_AS_NUMBER(pop());
          double a = CECILE_AS_NUMBER(pop());
          push(CECILE_NUMBER_VAL(a + b));
        } else {
          runtimeError(
              "Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT: BINARY_OP(CECILE_NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(CECILE_NUMBER_VAL, *); break;
      case OP_DIVIDE: BINARY_OP(CECILE_NUMBER_VAL, /); break;
      case OP_NOT:
        push(CECILE_BOOL_VAL(isFalsey(pop())));
        break;
      case OP_NEGATE: 
        if (!CECILE_IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number."); 
          return INTERPRET_RUNTIME_ERROR;
        }
        push(CECILE_NUMBER_VAL(-CECILE_AS_NUMBER(pop())));
        break;
      case OP_PRINT: {
        printValue(pop());
        printf("\n");
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjFunction* function = CECILE_AS_FUNCTION(READ_CONSTANT());
        ObjClosure* closure = newClosure(function);
        push(CECILE_OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] =
                captureUpvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE:
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;
      case OP_RETURN: {
        Value result = pop();
        closeUpvalues(frame->slots);
        vm.frameCount--;
        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
    }
  }
  #undef READ_BYTE
  #undef READ_CONSTANT 
  #undef READ_STRING
  #undef READ_SHORT
  #undef BINARY_OP
}

InterpretResult interpret(const char* source) {
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(CECILE_OBJ_VAL(function));
  ObjClosure* closure = newClosure(function);
  pop();
  push(CECILE_OBJ_VAL(closure));
  call(closure, 0);


  return run();

}


