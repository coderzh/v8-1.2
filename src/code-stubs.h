// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_CODE_STUBS_H_
#define V8_CODE_STUBS_H_

namespace v8 {
namespace internal {


// Stub is base classes of all stubs.
class CodeStub BASE_EMBEDDED {
 public:
  enum Major {
    CallFunction,
    GenericBinaryOp,
    SmiOp,
    Compare,
    RecordWrite,  // Last stub that allows stub calls inside.
    ConvertToDouble,
    WriteInt32ToHeapNumber,
    StackCheck,
    UnarySub,
    RevertToNumber,
    ToBoolean,
    Instanceof,
    CounterOp,
    ArgumentsAccess,
    Runtime,
    CEntry,
    JSEntry,
    GetProperty,   // ARM only
    SetProperty,   // ARM only
    InvokeBuiltin,  // ARM only
    JSExit,        // ARM only
    NUMBER_OF_IDS
  };

  // Retrieve the code for the stub. Generate the code if needed.
  Handle<Code> GetCode();

  static Major MajorKeyFromKey(uint32_t key) {
    return static_cast<Major>(MajorKeyBits::decode(key));
  };
  static int MinorKeyFromKey(uint32_t key) {
    return MinorKeyBits::decode(key);
  };
  static const char* MajorName(Major major_key);

  virtual ~CodeStub() {}

 protected:
  static const int kMajorBits = 5;
  static const int kMinorBits = kBitsPerInt - kSmiTagSize - kMajorBits;

 private:
  // Generates the assembler code for the stub.
  virtual void Generate(MacroAssembler* masm) = 0;

  // Returns information for computing the number key.
  virtual Major MajorKey() = 0;
  virtual int MinorKey() = 0;

  // The CallFunctionStub needs to override this so it can encode whether a
  // lazily generated function should be fully optimized or not.
  virtual InLoopFlag InLoop() { return NOT_IN_LOOP; }

  // Returns a name for logging/debugging purposes.
  virtual const char* GetName() { return MajorName(MajorKey()); }

#ifdef DEBUG
  virtual void Print() { PrintF("%s\n", GetName()); }
#endif

  // Computes the key based on major and minor.
  uint32_t GetKey() {
    ASSERT(static_cast<int>(MajorKey()) < NUMBER_OF_IDS);
    return MinorKeyBits::encode(MinorKey()) |
           MajorKeyBits::encode(MajorKey());
  }

  bool AllowsStubCalls() { return MajorKey() <= RecordWrite; }

  class MajorKeyBits: public BitField<uint32_t, 0, kMajorBits> {};
  class MinorKeyBits: public BitField<uint32_t, kMajorBits, kMinorBits> {};

  friend class BreakPointIterator;
};

} }  // namespace v8::internal

#endif  // V8_CODE_STUBS_H_
