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
#include "debug.h"


namespace v8 {
namespace internal {

#ifdef ENABLE_DEBUGGER_SUPPORT

bool Debug::IsDebugBreakAtReturn(v8::internal::RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsJSReturn(rinfo->rmode()));
  // 11th byte of patch is 0x49, 11th byte of JS return is 0xCC (int3).
  ASSERT(*(rinfo->pc() + 10) == 0x49 || *(rinfo->pc() + 10) == 0xCC);
  return (*(rinfo->pc() + 10) == 0x49);
}

void Debug::GenerateCallICDebugBreak(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED
}

void Debug::GenerateConstructCallDebugBreak(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED
}

void Debug::GenerateKeyedLoadICDebugBreak(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED
}

void Debug::GenerateKeyedStoreICDebugBreak(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED
}

void Debug::GenerateLoadICDebugBreak(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED
}

void Debug::GenerateReturnDebugBreak(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED
}

void Debug::GenerateReturnDebugBreakEntry(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED
}

void Debug::GenerateStoreICDebugBreak(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED
}

void Debug::GenerateStubNoRegistersDebugBreak(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED
}

#endif  // ENABLE_DEBUGGER_SUPPORT

} }  // namespace v8::internal
