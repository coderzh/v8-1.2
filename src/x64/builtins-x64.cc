// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"
#include "codegen-inl.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm, CFunctionId id) {
  // TODO(1238487): Don't pass the function in a static variable.
  ExternalReference passed = ExternalReference::builtin_passed_function();
  __ movq(kScratchRegister, passed.address(), RelocInfo::EXTERNAL_REFERENCE);
  __ movq(Operand(kScratchRegister, 0), rdi);

  // The actual argument count has already been loaded into register
  // rax, but JumpToBuiltin expects rax to contain the number of
  // arguments including the receiver.
  __ incq(rax);
  __ JumpToBuiltin(ExternalReference(id));
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ push(rbp);
  __ movq(rbp, rsp);

  // Store the arguments adaptor context sentinel.
  __ push(Immediate(ArgumentsAdaptorFrame::SENTINEL));

  // Push the function on the stack.
  __ push(rdi);

  // Preserve the number of arguments on the stack. Must preserve both
  // rax and rbx because these registers are used when copying the
  // arguments and the receiver.
  ASSERT(kSmiTagSize == 1);
  __ lea(rcx, Operand(rax, rax, times_1, kSmiTag));
  __ push(rcx);
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // Retrieve the number of arguments from the stack. Number is a Smi.
  __ movq(rbx, Operand(rbp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  // Leave the frame.
  __ movq(rsp, rbp);
  __ pop(rbp);

  // Remove caller arguments from the stack.
  // rbx holds a Smi, so we convery to dword offset by multiplying by 4.
  ASSERT_EQ(kSmiTagSize, 1 && kSmiTag == 0);
  ASSERT_EQ(kPointerSize, (1 << kSmiTagSize) * 4);
  __ pop(rcx);
  __ lea(rsp, Operand(rsp, rbx, times_4, 1 * kPointerSize));  // 1 ~ receiver
  __ push(rcx);
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : actual number of arguments
  //  -- rbx : expected number of arguments
  //  -- rdx : code entry to call
  // -----------------------------------

  Label invoke, dont_adapt_arguments;
  __ IncrementCounter(&Counters::arguments_adaptors, 1);

  Label enough, too_few;
  __ cmpq(rax, rbx);
  __ j(less, &too_few);
  __ cmpq(rbx, Immediate(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ j(equal, &dont_adapt_arguments);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all expected arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(rax, Operand(rbp, rax, times_pointer_size, offset));
    __ movq(rcx, Immediate(-1));  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incq(rcx);
    __ push(Operand(rax, 0));
    __ subq(rax, Immediate(kPointerSize));
    __ cmpq(rcx, rbx);
    __ j(less, &copy);
    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    // Copy receiver and all actual arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ lea(rdi, Operand(rbp, rax, times_pointer_size, offset));
    __ movq(rcx, Immediate(-1));  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incq(rcx);
    __ push(Operand(rdi, 0));
    __ subq(rdi, Immediate(kPointerSize));
    __ cmpq(rcx, rax);
    __ j(less, &copy);

    // Fill remaining expected arguments with undefined values.
    Label fill;
    __ movq(kScratchRegister,
            Factory::undefined_value(),
            RelocInfo::EMBEDDED_OBJECT);
    __ bind(&fill);
    __ incq(rcx);
    __ push(kScratchRegister);
    __ cmpq(rcx, rbx);
    __ j(less, &fill);

    // Restore function pointer.
    __ movq(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ call(rdx);

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ jmp(rdx);
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // Stack Layout:
  // rsp: return address
  //  +1: Argument n
  //  +2: Argument n-1
  //  ...
  //  +n: Argument 1 = receiver
  //  +n+1: Argument 0 = function to call
  //
  // rax contains the number of arguments, n, not counting the function.
  //
  // 1. Make sure we have at least one argument.
  { Label done;
    __ testq(rax, rax);
    __ j(not_zero, &done);
    __ pop(rbx);
    __ Push(Factory::undefined_value());
    __ push(rbx);
    __ incq(rax);
    __ bind(&done);
  }

  // 2. Get the function to call from the stack.
  { Label done, non_function, function;
    // The function to call is at position n+1 on the stack.
    __ movq(rdi, Operand(rsp, rax, times_pointer_size, +1 * kPointerSize));
    __ testl(rdi, Immediate(kSmiTagMask));
    __ j(zero, &non_function);
    __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
    __ j(equal, &function);

    // Non-function called: Clear the function to force exception.
    __ bind(&non_function);
    __ xor_(rdi, rdi);
    __ jmp(&done);

    // Function called: Change context eagerly to get the right global object.
    __ bind(&function);
    __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

    __ bind(&done);
  }

  // 3. Make sure first argument is an object; convert if necessary.
  { Label call_to_object, use_global_receiver, patch_receiver, done;
    __ movq(rbx, Operand(rsp, rax, times_pointer_size, 0));

    __ testl(rbx, Immediate(kSmiTagMask));
    __ j(zero, &call_to_object);

    __ Cmp(rbx, Factory::null_value());
    __ j(equal, &use_global_receiver);
    __ Cmp(rbx, Factory::undefined_value());
    __ j(equal, &use_global_receiver);

    __ CmpObjectType(rbx, FIRST_JS_OBJECT_TYPE, rcx);
    __ j(below, &call_to_object);
    __ CmpInstanceType(rcx, LAST_JS_OBJECT_TYPE);
    __ j(below_equal, &done);

    __ bind(&call_to_object);
    __ EnterInternalFrame();  // preserves rax, rbx, rdi

    // Store the arguments count on the stack (smi tagged).
    ASSERT(kSmiTag == 0);
    __ shl(rax, Immediate(kSmiTagSize));
    __ push(rax);

    __ push(rdi);  // save edi across the call
    __ push(rbx);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ movq(rbx, rax);
    __ pop(rdi);  // restore edi after the call

    // Get the arguments count and untag it.
    __ pop(rax);
    __ shr(rax, Immediate(kSmiTagSize));

    __ LeaveInternalFrame();
    __ jmp(&patch_receiver);

    // Use the global receiver object from the called function as the receiver.
    __ bind(&use_global_receiver);
    const int kGlobalIndex =
        Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
    __ movq(rbx, FieldOperand(rsi, kGlobalIndex));
    __ movq(rbx, FieldOperand(rbx, GlobalObject::kGlobalReceiverOffset));

    __ bind(&patch_receiver);
    __ movq(Operand(rsp, rax, times_pointer_size, 0), rbx);

    __ bind(&done);
  }

  // 4. Shift stuff one slot down the stack.
  { Label loop;
    __ lea(rcx, Operand(rax, +1));  // +1 ~ copy receiver too
    __ bind(&loop);
    __ movq(rbx, Operand(rsp, rcx, times_pointer_size, 0));
    __ movq(Operand(rsp, rcx, times_pointer_size, 1 * kPointerSize), rbx);
    __ decq(rcx);
    __ j(not_zero, &loop);
  }

  // 5. Remove TOS (copy of last arguments), but keep return address.
  __ pop(rbx);
  __ pop(rcx);
  __ push(rbx);
  __ decq(rax);

  // 6. Check that function really was a function and get the code to
  //    call from the function and check that the number of expected
  //    arguments matches what we're providing.
  { Label invoke, trampoline;
    __ testq(rdi, rdi);
    __ j(not_zero, &invoke);
    __ xor_(rbx, rbx);
    __ GetBuiltinEntry(rdx, Builtins::CALL_NON_FUNCTION);
    __ bind(&trampoline);
    __ Jump(Handle<Code>(builtin(ArgumentsAdaptorTrampoline)),
            RelocInfo::CODE_TARGET);

    __ bind(&invoke);
    __ movq(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ movsxlq(rbx,
           FieldOperand(rdx, SharedFunctionInfo::kFormalParameterCountOffset));
    __ movq(rdx, FieldOperand(rdx, SharedFunctionInfo::kCodeOffset));
    __ lea(rdx, FieldOperand(rdx, Code::kHeaderSize));
    __ cmpq(rax, rbx);
    __ j(not_equal, &trampoline);
  }

  // 7. Jump (tail-call) to the code in register edx without checking arguments.
  ParameterCount expected(0);
  __ InvokeCode(rdx, expected, expected, JUMP_FUNCTION);
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  // Stack at entry:
  //    rsp: return address
  //  rsp+8: arguments
  // rsp+16: receiver ("this")
  // rsp+24: function
  __ EnterInternalFrame();
  // Stack frame:
  //    rbp: Old base pointer
  // rbp[1]: return address
  // rbp[2]: function arguments
  // rbp[3]: receiver
  // rbp[4]: function
  static const int kArgumentsOffset = 2 * kPointerSize;
  static const int kReceiverOffset = 3 * kPointerSize;
  static const int kFunctionOffset = 4 * kPointerSize;
  __ push(Operand(rbp, kFunctionOffset));
  __ push(Operand(rbp, kArgumentsOffset));
  __ InvokeBuiltin(Builtins::APPLY_PREPARE, CALL_FUNCTION);

  if (FLAG_check_stack) {
    // We need to catch preemptions right here, otherwise an unlucky preemption
    // could show up as a failed apply.
    Label retry_preemption;
    Label no_preemption;
    __ bind(&retry_preemption);
    ExternalReference stack_guard_limit =
        ExternalReference::address_of_stack_guard_limit();
    __ movq(kScratchRegister, stack_guard_limit);
    __ movq(rcx, rsp);
    __ subq(rcx, Operand(kScratchRegister, 0));
    // rcx contains the difference between the stack limit and the stack top.
    // We use it below to check that there is enough room for the arguments.
    __ j(above, &no_preemption);

    // Preemption!
    // Because runtime functions always remove the receiver from the stack, we
    // have to fake one to avoid underflowing the stack.
    __ push(rax);
    __ push(Immediate(Smi::FromInt(0)));

    // Do call to runtime routine.
    __ CallRuntime(Runtime::kStackGuard, 1);
    __ pop(rax);
    __ jmp(&retry_preemption);

    __ bind(&no_preemption);

    Label okay;
    // Make rdx the space we need for the array when it is unrolled onto the
    // stack.
    __ movq(rdx, rax);
    __ shl(rdx, Immediate(kPointerSizeLog2 - kSmiTagSize));
    __ cmpq(rcx, rdx);
    __ j(greater, &okay);

    // Too bad: Out of stack space.
    __ push(Operand(rbp, kFunctionOffset));
    __ push(rax);
    __ InvokeBuiltin(Builtins::APPLY_OVERFLOW, CALL_FUNCTION);
    __ bind(&okay);
  }

  // Push current index and limit.
  const int kLimitOffset =
      StandardFrameConstants::kExpressionsOffset - 1 * kPointerSize;
  const int kIndexOffset = kLimitOffset - 1 * kPointerSize;
  __ push(rax);  // limit
  __ push(Immediate(0));  // index

  // Change context eagerly to get the right global object if
  // necessary.
  __ movq(rdi, Operand(rbp, kFunctionOffset));
  __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Compute the receiver.
  Label call_to_object, use_global_receiver, push_receiver;
  __ movq(rbx, Operand(rbp, kReceiverOffset));
  __ testl(rbx, Immediate(kSmiTagMask));
  __ j(zero, &call_to_object);
  __ Cmp(rbx, Factory::null_value());
  __ j(equal, &use_global_receiver);
  __ Cmp(rbx, Factory::undefined_value());
  __ j(equal, &use_global_receiver);

  // If given receiver is already a JavaScript object then there's no
  // reason for converting it.
  __ CmpObjectType(rbx, FIRST_JS_OBJECT_TYPE, rcx);
  __ j(less, &call_to_object);
  __ CmpInstanceType(rcx, LAST_JS_OBJECT_TYPE);
  __ j(less_equal, &push_receiver);

  // Convert the receiver to an object.
  __ bind(&call_to_object);
  __ push(rbx);
  __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
  __ movq(rbx, rax);
  __ jmp(&push_receiver);

  // Use the current global receiver object as the receiver.
  __ bind(&use_global_receiver);
  const int kGlobalOffset =
      Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
  __ movq(rbx, FieldOperand(rsi, kGlobalOffset));
  __ movq(rbx, FieldOperand(rbx, GlobalObject::kGlobalReceiverOffset));

  // Push the receiver.
  __ bind(&push_receiver);
  __ push(rbx);

  // Copy all arguments from the array to the stack.
  Label entry, loop;
  __ movq(rax, Operand(rbp, kIndexOffset));
  __ jmp(&entry);
  __ bind(&loop);
  __ movq(rcx, Operand(rbp, kArgumentsOffset));  // load arguments
  __ push(rcx);
  __ push(rax);

  // Use inline caching to speed up access to arguments.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  __ Call(ic, RelocInfo::CODE_TARGET);
  // It is important that we do not have a test instruction after the
  // call.  A test instruction after the call is used to indicate that
  // we have generated an inline version of the keyed load.  In this
  // case, we know that we are not generating a test instruction next.

  // Remove IC arguments from the stack and push the nth argument.
  __ addq(rsp, Immediate(2 * kPointerSize));
  __ push(rax);

  // Update the index on the stack and in register rax.
  __ movq(rax, Operand(rbp, kIndexOffset));
  __ addq(rax, Immediate(Smi::FromInt(1)));
  __ movq(Operand(rbp, kIndexOffset), rax);

  __ bind(&entry);
  __ cmpq(rax, Operand(rbp, kLimitOffset));
  __ j(not_equal, &loop);

  // Invoke the function.
  ParameterCount actual(rax);
  __ shr(rax, Immediate(kSmiTagSize));
  __ movq(rdi, Operand(rbp, kFunctionOffset));
  __ InvokeFunction(rdi, actual, CALL_FUNCTION);

  __ LeaveInternalFrame();
  __ ret(3 * kPointerSize);  // remove function, receiver, and arguments
}


void Builtins::Generate_JSConstructCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  //  -- rdi: constructor function
  // -----------------------------------

  Label non_function_call;
  // Check that function is not a smi.
  __ testl(rdi, Immediate(kSmiTagMask));
  __ j(zero, &non_function_call);
  // Check that function is a JSFunction.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(not_equal, &non_function_call);

  // Jump to the function-specific construct stub.
  __ movq(rbx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movq(rbx, FieldOperand(rbx, SharedFunctionInfo::kConstructStubOffset));
  __ lea(rbx, FieldOperand(rbx, Code::kHeaderSize));
  __ jmp(rbx);

  // edi: called object
  // eax: number of arguments
  __ bind(&non_function_call);

  // Set expected number of arguments to zero (not changing eax).
  __ movq(rbx, Immediate(0));
  __ GetBuiltinEntry(rdx, Builtins::CALL_NON_FUNCTION_AS_CONSTRUCTOR);
  __ Jump(Handle<Code>(builtin(ArgumentsAdaptorTrampoline)),
          RelocInfo::CODE_TARGET);
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
    // Enter a construct frame.
  __ EnterConstructFrame();

  // Store a smi-tagged arguments count on the stack.
  __ shl(rax, Immediate(kSmiTagSize));
  __ push(rax);

  // Push the function to invoke on the stack.
  __ push(rdi);

  // Try to allocate the object without transitioning into C code. If any of the
  // preconditions is not met, the code bails out to the runtime call.
  Label rt_call, allocated;

  // TODO(x64): Implement inlined allocation.

  // Allocate the new receiver object using the runtime call.
  // rdi: function (constructor)
  __ bind(&rt_call);
  // Must restore edi (constructor) before calling runtime.
  __ movq(rdi, Operand(rsp, 0));
  __ push(rdi);
  __ CallRuntime(Runtime::kNewObject, 1);
  __ movq(rbx, rax);  // store result in rbx

  // New object allocated.
  // rbx: newly allocated object
  __ bind(&allocated);
  // Retrieve the function from the stack.
  __ pop(rdi);

  // Retrieve smi-tagged arguments count from the stack.
  __ movq(rax, Operand(rsp, 0));
  __ shr(rax, Immediate(kSmiTagSize));

  // Push the allocated receiver to the stack. We need two copies
  // because we may have to return the original one and the calling
  // conventions dictate that the called function pops the receiver.
  __ push(rbx);
  __ push(rbx);

  // Setup pointer to last argument.
  __ lea(rbx, Operand(rbp, StandardFrameConstants::kCallerSPOffset));

  // Copy arguments and receiver to the expression stack.
  Label loop, entry;
  __ movq(rcx, rax);
  __ jmp(&entry);
  __ bind(&loop);
  __ push(Operand(rbx, rcx, times_pointer_size, 0));
  __ bind(&entry);
  __ decq(rcx);
  __ j(greater_equal, &loop);

  // Call the function.
  ParameterCount actual(rax);
  __ InvokeFunction(rdi, actual, CALL_FUNCTION);

  // Restore context from the frame.
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));

  // If the result is an object (in the ECMA sense), we should get rid
  // of the receiver and use the result; see ECMA-262 section 13.2.2-7
  // on page 74.
  Label use_receiver, exit;
  // If the result is a smi, it is *not* an object in the ECMA sense.
  __ testl(rax, Immediate(kSmiTagMask));
  __ j(zero, &use_receiver);

  // If the type of the result (stored in its map) is less than
  // FIRST_JS_OBJECT_TYPE, it is not an object in the ECMA sense.
  __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rcx);
  __ j(greater_equal, &exit);

  // Throw away the result of the constructor invocation and use the
  // on-stack receiver as the result.
  __ bind(&use_receiver);
  __ movq(rax, Operand(rsp, 0));

  // Restore the arguments count and leave the construct frame.
  __ bind(&exit);
  __ movq(rbx, Operand(rsp, kPointerSize));  // get arguments count
  __ LeaveConstructFrame();

  // Remove caller arguments from the stack and return.
  ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ pop(rcx);
  __ lea(rsp, Operand(rsp, rbx, times_4, 1 * kPointerSize));  // 1 ~ receiver
  __ push(rcx);
  __ ret(0);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Expects five C++ function parameters.
  // - Address entry (ignored)
  // - JSFunction* function (
  // - Object* receiver
  // - int argc
  // - Object*** argv
  // (see Handle::Invoke in execution.cc).

  // Platform specific argument handling. After this, the stack contains
  // an internal frame and the pushed function and receiver, and
  // register rax and rbx holds the argument count and argument array,
  // while rdi holds the function pointer and rsi the context.
#ifdef __MSVC__
  // MSVC parameters in:
  // rcx : entry (ignored)
  // rdx : function
  // r8 : receiver
  // r9 : argc
  // [rsp+0x20] : argv

  // Clear the context before we push it when entering the JS frame.
  __ xor_(rsi, rsi);
  // Enter an internal frame.
  __ EnterInternalFrame();

  // Load the function context into rsi.
  __ movq(rsi, FieldOperand(rdx, JSFunction::kContextOffset));

  // Push the function and the receiver onto the stack.
  __ push(rdx);
  __ push(r8);

  // Load the number of arguments and setup pointer to the arguments.
  __ movq(rax, r9);
  // Load the previous frame pointer to access C argument on stack
  __ movq(kScratchRegister, Operand(rbp, 0));
  __ movq(rbx, Operand(kScratchRegister, EntryFrameConstants::kArgvOffset));
  // Load the function pointer into rdi.
  __ movq(rdi, rdx);
#else  // !defined(__MSVC__)
  // GCC parameters in:
  // rdi : entry (ignored)
  // rsi : function
  // rdx : receiver
  // rcx : argc
  // r8  : argv

  __ movq(rdi, rsi);
  // rdi : function

  // Clear the context before we push it when entering the JS frame.
  __ xor_(rsi, rsi);
  // Enter an internal frame.
  __ EnterInternalFrame();

  // Push the function and receiver and setup the context.
  __ push(rdi);
  __ push(rdx);
  __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Load the number of arguments and setup pointer to the arguments.
  __ movq(rax, rcx);
  __ movq(rbx, r8);
#endif  // __MSVC__
  // Current stack contents:
  // [rsp + 2 * kPointerSize ... ]: Internal frame
  // [rsp + kPointerSize]         : function
  // [rsp]                        : receiver
  // Current register contents:
  // rax : argc
  // rbx : argv
  // rsi : context
  // rdi : function

  // Copy arguments to the stack in a loop.
  // Register rbx points to array of pointers to handle locations.
  // Push the values of these handles.
  Label loop, entry;
  __ xor_(rcx, rcx);  // Set loop variable to 0.
  __ jmp(&entry);
  __ bind(&loop);
  __ movq(kScratchRegister, Operand(rbx, rcx, times_pointer_size, 0));
  __ push(Operand(kScratchRegister, 0));  // dereference handle
  __ addq(rcx, Immediate(1));
  __ bind(&entry);
  __ cmpq(rcx, rax);
  __ j(not_equal, &loop);

  // Invoke the code.
  if (is_construct) {
    // Expects rdi to hold function pointer.
    __ movq(kScratchRegister,
            Handle<Code>(Builtins::builtin(Builtins::JSConstructCall)),
            RelocInfo::CODE_TARGET);
    __ call(kScratchRegister);
  } else {
    ParameterCount actual(rax);
    // Function must be in rdi.
    __ InvokeFunction(rdi, actual, CALL_FUNCTION);
  }

  // Exit the JS frame. Notice that this also removes the empty
  // context and the function left on the stack by the code
  // invocation.
  __ LeaveInternalFrame();
  // TODO(X64): Is argument correct? Is there a receiver to remove?
  __ ret(1 * kPointerSize);  // remove receiver
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}

} }  // namespace v8::internal
